package main

import (
	"log"
	gw "github.com/dappnetbby/local-gateway/gw"
	"github.com/akamensky/argparse"
	"os"
	"fmt"
	"strconv"
)

var logger = log.New(os.Stdout, "", log.Ldate|log.Ltime)
const VERSION = "0.0.1"
const GATEWAY_PORT = 10422

func main() {
	parser := argparse.NewParser("local-gateway", "Dappnet Local Gateway Server")
	
	// Add command line arguments
	portPtr := parser.String("p", "port", &argparse.Options{Default: strconv.Itoa(GATEWAY_PORT), Help: "Port to listen on"})
	parentPidPtr := parser.String("", "parent-pid", &argparse.Options{Help: "Parent process ID (Chrome launcher)"})

	// Parse input
	err := parser.Parse(os.Args)
	if err != nil {
		fmt.Print(parser.Usage(err))
		os.Exit(1)
	}

	// Parse port
	port, err := strconv.Atoi(*portPtr)
	if err != nil {
		logger.Printf("Invalid port: %s\n", *portPtr)
		os.Exit(1)
	}

    logger.Printf("Dappnet Local Gateway v%s\n", VERSION)
	logger.Println("Copyright Liam Zebedee and contributors")
	
	if *parentPidPtr != "" {
		logger.Printf("Started by parent process: %s\n", *parentPidPtr)
	}

	gateway := gw.NewGatewayServer(uint(port))
	
	go func(){
		logger.Println("")
		logger.Printf("Listening on http://0.0.0.0:%d\n", port)
		if err := gateway.Start(); err != nil {
            logger.Printf("Error starting server: %v\n", err)
        }
	}()

	ch := make(chan bool)
	<-ch
}