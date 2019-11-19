# WebSocket API

This directory contains:
- the implementation of
  [the WebSocket API](https://html.spec.whatwg.org/multipage/web-sockets.html).
- the Pepper implementation of the WebSocket API

They use WebSocketChannelImpl to connect to the WebSocket service i.e. the
blink.mojom.WebSocket implementation in content/browser/websockets/.

## Design docs

See also [//net/websockets/README.md](../../../../../net/websockets/README.md)
for the design of the network service side of the implementation.

* [WebSocket SafeBrowsing
  Support](https://docs.google.com/document/d/1iR3XMIQukqlXb6ajIHE91apHZAxyF_wvRoB5JGeJYPs/edit)
  describes how SafeBrowsing checks are done on WebSocket URLs. Some class names
  and details have changed as Worker WebSockets are no longer proxies via the
  main thread.
