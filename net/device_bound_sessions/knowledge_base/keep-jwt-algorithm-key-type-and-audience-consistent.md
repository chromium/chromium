---
name: net/device_bound_sessions/keep-jwt-algorithm-key-type-and-audience-consistent
rubric: correctness
severity: should_fix
---

**Rule.** Build DBSC JWTs only through the builders in `session_binding_utils.h`
(`CreateKeyRegistrationHeaderAndPayload()`, `CreateKeyRefreshHeaderAndPayload()`
and `CreateOuterRegistrationHeaderAndPayload()`), which enforce consistency in
code: they create the JWK for the requested algorithm with
`CreateJwkForAlgorithm()`, which fails unless `ECDSA_SHA256` gets an EC P-256
key and `RSA_PKCS1_SHA256` gets an RSA key, and they build `aud` with
`RemoveQueryAndFragment()`. Flag code that assembles a JWT header or payload by
hand, pairs `CreateJwk()` with a separately chosen `alg`, or sets `aud` from a
raw `spec()`; if a new token type is needed, add a builder that keeps these
checks. When behavior depends on the key algorithm, test with both
`ECDSA_SHA256` and `RSA_PKCS1_SHA256` keys; the fake unexportable key provider
supports both.

**Why.** A server verifies the signature with the embedded JWK under the
declared `alg`, so a token whose `alg` and key type disagree fails verification.
`aud` must match what the server expects for the endpoint, so building it
differently in different tokens (for example, keeping a query string in one)
makes otherwise valid tokens fail. Keeping these checks inside the builders
enforces them for every token. DBSC tests default to ECDSA keys, so RSA paths
stay untested unless a test asks for them.

## Examples

Bad:

```cpp
// Hand-built token: `alg` comes from the caller, the JWK follows the key's
// actual type, and `aud` keeps the query string.
header.Set("alg", *ToJoseAlgorithm(algorithm));
header.Set("jwk", *CreateJwk(public_key));
payload.Set("aud", destination_url.spec());
```

Good:

```cpp
// The builder creates the JWK for `algorithm`, fails if the key type doesn't
// match it, and normalizes `aud`.
std::optional<std::string> header_and_payload =
    CreateKeyRegistrationHeaderAndPayload(challenge, algorithm, pubkey_spki,
                                          destination_url, authorization);
```
