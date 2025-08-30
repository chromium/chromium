local-gateway-go
================

WIP rewrite of the local gateway module from Dappnet mainline in Go.

Status:

 - [x] Resolve .eth domains to content hash
 - [x] HTTP gateway - resolve http://localhost:10422 to IPFS content by specifying `Host: vitalik.eth`
 - [x] HTTPS gateway - self-signed certificates generation is working.
 - [x] Rewrite SOCKS5 proxy, also in Go.
 - [ ] Handle IPFS errors like the JS gateway does.
 - [ ] Add BitTorrent support.
 - [ ] Listen to ENS record updates, and refresh cache.

## Demo.

```
go build
./local-gateway

cd socks5
go build
./socks5-proxy
```