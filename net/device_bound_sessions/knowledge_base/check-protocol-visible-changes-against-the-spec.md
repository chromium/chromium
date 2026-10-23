---
name: net/device_bound_sessions/check-protocol-visible-changes-against-the-spec
rubric: correctness
severity: should_fix
---

**Rule.** Trigger on anything a server or website can observe:
`Secure-Session-*` headers and parameters, JWT claims, binding and attestation
statements, `.well-known` URL derivation, and `Sec-Fetch-*` or redirect
behavior. Ask for (1) a link to the relevant section of the W3C DBSC spec
(https://w3c.github.io/webappsec-dbsc/) or the DBSC-SSO explainer, and a check
of names and constraints against it; (2) where Chrome intentionally diverges,
implements a pending spec change, or the spec is silent, a comment linking the
spec issue or pull request with a TODO, plus the chosen behavior documented; (3)
web platform tests for website-visible behavior, and tests for which fields are
present or absent in each format. Keep identifiers aligned with the spec's
parameter names.

**Why.** Servers and other browsers implement DBSC from the spec, so behavior a
server can observe that silently differs from it breaks interoperability, and
the divergence is hard to find later. A spec link or a tracked TODO makes an
intentional difference reviewable, and web platform tests catch regressions in
what websites see.

## Examples

Bad:

```cpp
// Sends the registration statement as the binding statement, so it carries
// "fmt" too, without checking what the DBSC-SSO explainer defines.
std::optional<std::string> SerializeBindingStatement(
    const crypto::AttestationStatement& statement) {
  return base::WriteJson(CreateRegistrationStatement(statement))
      .transform([](std::string_view json) { return Base64UrlEncode(json); });
}
```

Good:

```cpp
// Serializes an attestation statement into a Base64URL-encoded JSON binding
// statement as defined in
// https://github.com/WICG/dbsc-sso#relying-partys-session-initialization,
// suitable for the Secure-Session-KeyBinding header. Unlike the registration
// statement, it omits "fmt", per https://github.com/WICG/dbsc-sso/issues/15.
// TODO(crbug.com/481630807): Drop the issue reference once
// https://github.com/WICG/dbsc-sso/pull/22 updates the explainer.
std::optional<std::string> SerializeBindingStatement(
    const crypto::AttestationStatement& statement) {
  return base::WriteJson(CreateBindingStatement(statement))
      .transform([](std::string_view json) { return Base64UrlEncode(json); });
}
```
