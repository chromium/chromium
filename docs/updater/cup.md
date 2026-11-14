# Client Update Protocol (CUP)

CUP is a protocol, typically carried over HTTP, used to secure communication
with CUP-capable servers when TLS is untrustworthy or impractical for other
reasons.

CUP provides:
  * Integrity of the client's request body.
  * Integrity of the server's response body.
  * Freshness of the server's response body.

CUP does not provide:
  * Integrity of the client's request headers or request line.
  * Integrity of the server's response headers.
  * Confidentiality of the client's request.
  * Confidentiality of the server's response.
  * Freshness of the client's request.

CUP requires:
  * Control of both the client and server.

CUP communications are secured by a digital signature algorithm (for example,
ECDSA or ML-DSA). The client has the public key hardcoded into the application
binary. The server signs request-response pairs with the private key, and the
client verifies using the public key.

In Chromium, CUP keys are rotated annually. Each key pair has a corresponding
version number. To interoperate with old clients, the server must keep all
historical versions of the key, and be prepared to sign with any of them. The
client should only keep a single version (whichever was most recent at compile
time).

## Details

### Description
For each request, the client assembles three components:
 1. The message body (the request body to be sent to the server).
 2. A random value to be used as a nonce for freshness (at least 256 bits).
 3. A code to identify the public key the client will use to verify this
    request. The key ID also determines the signature algorithm.

The client joins the key ID and nonce into a string `cup2key = <key_id>:<nonce>`
and computes `request_hash = SHA-256(request_body)`. It stores both, and sends
the request body, `cup2key`, and `request_hash` to the server.

The server:
 1. Finds the private key for the key ID, or returns an error.
 2. Builds the response body.
 3. Computes
    `inner_hash = SHA-256(request_hash | SHA-256(response_body) | cup2key)`,
    where `|` is concatenation and `cup2key` is in UTF-8.
 4. Signs `inner_hash` with the private key, and returns the signature and its
    `request_hash` with the response body.

The client accepts the response only if:
 1. The returned request hash matches its stored `request_hash`.
 2. The signature verifies against an `inner_hash` that the client builds from
    its stored `request_hash`, the received response body, and the `cup2key` it
    sent.

### HTTP Implementation
The request body is a POST body. The key ID and nonce are transmitted in a
query parameter appended to the requested URL, using the format
`&cup2key=<key_id>:<nonce>`.

For debugging purposes, the request hash is sent to the server using a query
parameter appended to the requested URL, using the format `&cup2hreq=%s` where
%s is the lowercase hexadecimal value of the hash (in big-endian format).

The server returns the signature and the request hash it computed in at least
one of three forms:
 1. The `X-Cup-Server-Proof` HTTP header, with the value in the format
    `signature:hash`.
 2. The `ETag` HTTP header, with the value in the format of
    `W/"signature:hash"`.
 3. The `ETag` HTTP header, with the value in the format of `signature:hash`.

If multiple forms are present in the response, clients should prefer form 1,
falling back to form 2 only if form 1 is not present, and falling back to form 3
only if form 1 and 2 are not present.

In practice, multiple forms allow the communication to navigate different types
of proxies that mutate request headers.

In all forms, `signature` is the signature rendered in lowercase hexadecimal
representation. Its encoding depends on the signature algorithm.

In all forms, `hash` is a 256-bit value rendered in lowercase hexadecimal
representation (big-endian).

### Chromium Implementation
Chromium clients verify ML-DSA-44 signatures. ML-DSA is designed to resist
attacks by quantum computers, which ECDSA is not. Older clients verify ECDSA
(P-256) signatures, so servers must continue to support both.

The key ID depends on the algorithm:
  * ML-DSA-44: `ML-DSA-44-<version>`.
  * ECDSA: the version number in decimal.

Chromium base64url-encodes the nonce.

The signature depends on the algorithm:
  * ML-DSA-44: the signed message is `inner_hash`. The signature is the
    canonical byte representation of the ML-DSA-44 signature as defined in
    FIPS 204 (2,420 bytes).
  * ECDSA: ECDSA with SHA-256 over `inner_hash`, so the signed digest is
    `SHA-256(inner_hash)`. The signature is a DER-encoded ASN.1 sequence of "R"
    and "S".

### K-Repetition
A grave danger in any system involving ECDSA is the danger of K repetition.
ML-DSA is not vulnerable to this attack.

Computing an ECDSA signature starts with selecting a random 256-bit integer,
called K. The combination of K and the public key are used to produce the first
half of the signature, called R; the values of R, K, the private key, and the
message digest are used to compute the other half of the signature, called S.

Because of this process, if the same value of K is chosen for two signatures,
both signatures will have the same value for R. If a malicious user can acquire
two messages that have different bodies but identical R values, a
straightforward computation yields the server's private key.

Assuming that a good PRNG is used, and properly seeded, the probability of a
collision is small even across a large number of signatures. However, regular
key rotation is still recommended:
  1. Key material may be disclosed due to server compromise, and organizations
     should be prepared to remediate by performing a key rotation. Regularly
     exercising a key rotation process is important preparation.
  2. PRNGs are not always perfectly secure or properly seeded.

## See Also
Previous documents describing CUP can be found at:
  * [Original Public Design](https://github.com/google/omaha/blob/master/doc/ClientUpdateProtocol.md)
  * [ECDSA Extension](https://github.com/google/omaha/blob/master/doc/ClientUpdateProtocolEcdsa.md)

Chromium's implementation of CUP can be found in
[components/client\_update\_protocol](https://source.chromium.org/chromium/chromium/src/+/main:components/client_update_protocol/).
