package lib

import (
	"fmt"
	"crypto/tls"
	"io"
	"log"
	"net/http"
	"os"
	"strings"
)

var logger = log.New(os.Stdout, "[Gateway] ", log.Ldate|log.Ltime)

type GatewayServer struct {
	srv *http.Server
	ensResolver *ENSResolver
	
	certificateCache map[string]*tls.Certificate
}

func NewGatewayServer(port uint) *GatewayServer {
	s := &GatewayServer{}
	s.ensResolver = NewENSResolver()
	s.certificateCache = make(map[string]*tls.Certificate)

	// Start the HTTPS server.
	s.srv = &http.Server{
		Addr: fmt.Sprintf(":%d", port),
		Handler: s,
	}

	return s
}

func (s *GatewayServer) Start() error {
	return s.srv.ListenAndServe()
}

func (s *GatewayServer) ServeHTTP(res http.ResponseWriter, req *http.Request) {
	// Get the hostname.
	hostname := req.Host

	// Replace .dapp suffix with .eth if present
	if strings.HasSuffix(hostname, ".dapp") {
		hostname = strings.TrimSuffix(hostname, ".dapp") + ".eth"
	}

	

	// Construct the full URL that the user sees.
	humanURL := fmt.Sprintf("http://%s%s", hostname, req.URL.Path)
	logger.Printf("Request for %s\n", humanURL)

	// We've already generated the certifiate.
	// Now we want to resolve the hostname to its contenthash.
	contenthash := s.ensResolver.Resolve(hostname)
	logger.Printf("Resolved %s to %s\n", hostname, contenthash)

	// Now we want to forward the request to the gateway.
	client := &http.Client{
		CheckRedirect: func(req *http.Request, via []*http.Request) error {
			logger.Printf("Redirecting to %s\n", req.URL)
			return http.ErrUseLastResponse
		},
	}

	ipfsHost := "http://0.0.0.0:8080"
	ipfsContenthashPath := contenthash
	gatewayPath := ipfsHost + ipfsContenthashPath + req.URL.Path

	logger.Printf("Forwarding request to %s\n", gatewayPath)

	// GET.
	httpReq, err := http.NewRequest("GET", gatewayPath, nil)
	if err != nil {
		logger.Printf("Error creating request: %v\n", err)
		return
	}

	// Copy headers.
	for key, values := range req.Header {
		for _, value := range values {
			httpReq.Header.Add(key, value)
		}
	}

	// Send the request.
	resp, err := client.Do(httpReq)
	defer resp.Body.Close()
	if err != nil {
		logger.Printf("Error sending request: %v\n", err)
		return
	}

	// Copy headers.
	for key, values := range resp.Header {
		for _, value := range values {
			res.Header().Add(key, value)
		}
	}

	// Copy status code.
	res.WriteHeader(resp.StatusCode)

	// Stream response body.
	io.Copy(res, resp.Body)
}