# Web Crypto

This directory contains the cryptographic code for Chromium's [Web
Crypto](https://www.w3.org/TR/WebCryptoAPI/) implementation.

The Web Crypto implementation is split between Blink and this directory.

Blink is responsible for parsing Web Crypto's Web IDL, and translating requests
into method calls on `blink::WebCrypto`, which in turn is implemented here by
[WebCryptoImpl](webcrypto_impl.h).

`WebCryptoImpl` is what carries out the actual cryptographic operations. Crypto
is done directly in the renderer process, in software, using BoringSSL. There is
intentionally no support for hardware backed tokens.

Threading:

The Web Crypto API expects asynchronous completion of operations, even when
used from Web Workers. `WebCryptoImpl` takes a blanket approach of dispatching
incoming work to a small worker pool. This favors main thread
responsiveness/simplicity over throughput. Operations minimally take two thread
hops.

The split of responsibilities between Blink and `content`
(`content` is what registers `blink::WebCrypto` to the Blink Platform) is dated
and could be simplified. See also
[crbug.com/614385](https://bugs.chromium.org/p/chromium/issues/detail?id=614385).
