[net/spdy](https://source.chromium.org/chromium/chromium/src/+/main:net/spdy/)
provides the HTTP/2 implementation for Chromium. It takes care of things like
session management and flow control. It relies on the [QUICHE
library](https://source.chromium.org/chromium/chromium/src/+/main:net/third_party/quiche/src/)
for serializing and decoding.

The specifications for HTTP/2 and its header compression algorithm QPACK are
published at [RFC 9114](https://www.rfc-editor.org/rfc/rfc9114.html) and [RFC
7541](https://httpwg.org/specs/rfc7541.html).`
