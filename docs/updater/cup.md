# Client Update Protocol (CUP)

CUP is an HTTP protocol extension used to secure communication with CUP-capable
servers when TLS is untrustworthy or impractical for other reasons.

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

Current CUP communications are secured by an ECDSA key pair. The client has the
public key hardcoded into the application binary. The server signs
request-response pairs with the private key, and the client verifies using the
public key.

In Chromium, CUP keys are rotated annually. Each key pair has a corresponding
version number. To interoperate with old clients, the server must keep all
historical versions of the key, and be prepared to sign with any of them. The
client should only keep a single version (whichever was most recent at compile
time).

## Details

### Description
The server publishes an elliptic curve field/equation and a public key curve
point to be used by the client. In practice, these values are hardcoded into
each client at compile time.

For each request, the client assembles three components:
 1. The message body (the request body to be sent to the server).
 2. A random value to be used as a nonce for freshness (at least 256 bits).
 3. A code to identify the public key the client will use to verify this
    request. The client converts the public key id and nonce to a string: the
    public key is converted to decimal, and the nonce to any url-safe encoding.

The client stores the request body in a buffer, in UTF-8 format; it
appends the keyid/nonce string to this buffer. It calculates a SHA-256 hash of
this combined buffer, which it stores for validation later. It sends the
request and the keyid/nonce string to the server.

The server receives the request body, public key id, and nonce; it performs
the same appending operation, and computes the SHA-256 hash of the received data
buffer.

The server attempts to find a matching ECDSA private key for the specified
public key id, returning an HTTP error if no such private key exists. Finally,
it assembles the response body.

Before sending, the server stores the response body (also in UTF-8) in a
buffer. It appends the computed SHA-256 hash of the request body+keyid+nonce to
the buffer. It then calculates an ECDSA signature over that combined buffer,
using the server’s private key. It sends the ECDSA signature and the response
body + client hash back to the user.

The client receives the response body, observed client hash, and ECDSA signature.
The client compares the observed client hash to its stored request hash. If
there is a mismatch, then either the request or response have been tampered with
and the response is rejected.

The client concatenates the request hash to the response body, and verifies the
signature using its public key. If verification fails, then either the request
or response have been tampered with and the response is rejected.

### HTTP Implementation
The request body is a POST body. The key ID and nonce, are transmitted in a
query parameter appended to the requested URL, using the format `&cup2key=%d:%u`
where the first parameter is the key ID, the second is the freshness nonce.

For debugging purposes, the request hash is sent to the server using a query
parameter appended to the requested URL, using the format `&cup2hreq=%s` where
%s is the lowercase hexadecimal value of the hash (in big-endian format).

The server returns the ECDSA signature and the request hash it computed in at
least one of three forms:
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

In all forms, `signature` is a DER-encoded ASN.1 sequence of "R" and "S",
rendered in lowercase hexadecimal representation.

In all forms, `hash` is a 256-bit value rendered in lowercase hexadecimal
representation (big-endian).

### K-Repetition
A grave danger in any system involving ECDSA is the danger of K repetition.

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

