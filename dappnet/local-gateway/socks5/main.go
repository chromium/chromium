package main

import (
	"github.com/dappnetbby/socks5-proxy/socks5"
	"log"
)

func main() {
	conf := &socks5.Config{}
	server, err := socks5.New(conf)
	if err != nil {
		panic(err)
	}

	// Create SOCKS5 proxy on localhost port 8000
	log.Println("Starting SOCKS5 proxy on http://127.0.0.1:6801")
	if err := server.ListenAndServe("tcp", "127.0.0.1:6801"); err != nil {
		panic(err)
	}
}