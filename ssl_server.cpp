//g++ -Wall -o ssl_server ssl_server.cpp -L/usr/lib -lssl -lcrypto

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <string.h>
#include <resolv.h>
#include <netdb.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#define SSL_SERVER_RSA_CERT	"ssl_server.crt"
#define SSL_SERVER_RSA_KEY	"ssl_server.key"
#define SSL_SERVER_RSA_CA_CERT	"ca.crt"
#define SSL_SERVER_RSA_CA_PATH	"/home/nmathew/cacert/"

#define SSL_SERVER_ADDR		"/home/nmathew/ssl_server"

#define OFF	0
#define ON	1

int main(void)
{
	int verify_peer = ON;
	const SSL_METHOD *server_meth;
	SSL_CTX *ssl_server_ctx;
	int serversocketfd;
	int clientsocketfd;
	//struct sockaddr_un serveraddr;
	struct sockaddr_in serveraddr;
	struct hostnet *host;

	int handshakestatus;

	SSL_library_init();
	SSL_load_error_strings();
	server_meth = SSLv23_server_method();
	ssl_server_ctx = SSL_CTX_new(server_meth);
	
	if(!ssl_server_ctx)
	{
		ERR_print_errors_fp(stderr);
		return -1;
	}
	
	if(SSL_CTX_use_certificate_file(ssl_server_ctx, SSL_SERVER_RSA_CERT, SSL_FILETYPE_PEM) <= 0)	
	{
		ERR_print_errors_fp(stderr);
		return -1;		
	}

	
	if(SSL_CTX_use_PrivateKey_file(ssl_server_ctx, SSL_SERVER_RSA_KEY, SSL_FILETYPE_PEM) <= 0)	
	{
		ERR_print_errors_fp(stderr);
		return -1;		
	}
	
	if(SSL_CTX_check_private_key(ssl_server_ctx) != 1)
	{
		printf("Private and certificate is not matching\n");
		return -1;
	}	

	if(verify_peer)
	{	
		//See function man pages for instructions on generating CERT files
		if(!SSL_CTX_load_verify_locations(ssl_server_ctx, SSL_SERVER_RSA_CA_CERT, NULL))
		{
			ERR_print_errors_fp(stderr);
			return -1;		
		}
		SSL_CTX_set_verify(ssl_server_ctx, SSL_VERIFY_PEER, NULL);
		SSL_CTX_set_verify_depth(ssl_server_ctx, 1);
	}

	if((serversocketfd = socket(PF_INET, SOCK_STREAM, 0)) < 0)
	{
		printf("Error on socket creation\n");
		return -1;
	}
	memset(&serveraddr, 0, sizeof(struct sockaddr_un));
	//serveraddr.sun_family = AF_UNIX;
	//serveraddr.sun_path[0] = 0;
	//strncpy(&(serveraddr.sun_path[1]), SSL_SERVER_ADDR, strlen(SSL_SERVER_ADDR) + 1);
	
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(4242);
	serveraddr.sin_addr.s_addr = INADDR_ANY;


	if(bind(serversocketfd, (struct sockaddr *)&serveraddr, sizeof(struct sockaddr_in)))
	{
		printf("server bind error\n");
		return -1;
	}
	
	if(listen(serversocketfd, SOMAXCONN))
	{
		printf("Error on listen\n");
		return -1;
	}	
	while(1)
	{
		SSL *serverssl;
		char buffer[1024];
		int bytesread = 0;
		int addedstrlen;
		int ret;
	
		clientsocketfd = accept(serversocketfd, NULL, 0);
		serverssl = SSL_new(ssl_server_ctx);
		if(!serverssl)
		{
			printf("Error SSL_new\n");
			return -1;
		}
		SSL_set_fd(serverssl, clientsocketfd);
		
		if((ret = SSL_accept(serverssl))!= 1)
		{
			printf("Handshake Error %d\n", SSL_get_error(serverssl, ret));
			return -1;
		}
		
		if(verify_peer)
		{
			X509 *ssl_client_cert = NULL;

			ssl_client_cert = SSL_get_peer_certificate(serverssl);
			
			if(ssl_client_cert)
			{
				long verifyresult;

				verifyresult = SSL_get_verify_result(serverssl);
				if(verifyresult == X509_V_OK)
					printf("Certificate Verify Success\n"); 
				else
					printf("Certificate Verify Failed\n"); 
				X509_free(ssl_client_cert);				
			}
			else
				printf("There is no client certificate\n");
		}
		bytesread = SSL_read(serverssl, buffer, sizeof(buffer));
		addedstrlen = strlen("Appended by SSL server");
		strncpy(&buffer[bytesread], "Appended by SSL server", addedstrlen);
		buffer[bytesread +  addedstrlen ] = '\0';
		SSL_write(serverssl, buffer, bytesread + addedstrlen + 1);
		SSL_shutdown(serverssl);
		close(clientsocketfd);
		clientsocketfd = -1;
		SSL_free(serverssl);
		serverssl = NULL;
	}	
	close(serversocketfd);
	SSL_CTX_free(ssl_server_ctx);
	return 0;	
}