---
name: net/device_bound_sessions/never-match-failure-sentinels-against-provided-values
rubric: security
severity: should_fix
---

**Rule.** Flag comparisons between a locally computed value that signals failure
with a sentinel (such as an empty string) and a value provided by another
source, such as a server response, parsed input or another API, that may
legitimately hold the same value. Fallible helpers should return `std::optional`
or `base::expected`, and the caller must handle the failure with an explicit
`SessionError` before comparing. Also flag sibling helpers that use different
failure conventions for the same kind of result, and ask for a test where the
provided value is empty.

**Why.** When a failure is encoded as an ordinary value, comparing it with a
provided value that can legitimately be the same makes the failure look like a
match. In DBSC this can accept the wrong key: a JWK thumbprint computation that
fails and returns `""` compares equal to an empty `provider_key`. Returning
`std::optional` or `base::expected` forces callers to handle the failure before
comparing, and one failure convention per kind of result keeps the mistake from
creeping back.

## Examples

Bad:

```cpp
// ComputeThumbprintOrEmpty() returns "" on failure, and the server may send "".
if (ComputeThumbprintOrEmpty(key) == params.provider_key()) {
  AcceptFederatedKey();
}
```

Good:

```cpp
std::optional<std::string> thumbprint = CreateJwkThumbprint(key);
if (!thumbprint.has_value()) {
  return base::unexpected(SessionError::kInvalidFederatedKey);
}
if (*thumbprint == params.provider_key()) {
  AcceptFederatedKey();
}
```
