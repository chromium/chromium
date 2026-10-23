---
name: net/device_bound_sessions/validate-server-supplied-origins
rubric: security
severity: should_fix
---

**Rule.** Treat every origin or URL taken from a DBSC header or JSON body as
hostile (for example, `target_origin` in `Secure-Session-GenerateKey`, or the
`.well-known` `relying_origins` list). Require both `!origin.opaque()` (or
`url.is_valid()`) and `IsSecure()`, as `GenerateKeyParam::Parse()` and
`registration_fetcher_param.cc` do. Malformed entries must neither consume nor
bypass limits such as the cap on `relying_origins` labels, and dropping an
existing `is_valid()` check is a behavior change. Ask for tests with an
`http://` origin, a `file://` URL, and a malformed entry placed first.

**Why.** Origins that pass these checks become keys for per-site sessions,
quotas and caps, so a weak check lets a server create state for an origin it
shouldn't control or get past a limit. `!origin.opaque()` alone accepts
`http://` and `file://` origins, which DBSC's secure-origin requirement
excludes, and `GURL` can still expose a host when `is_valid()` is false, so
omitting `url.is_valid()` in helpers like `GetOriginLabel()` lets a malformed
entry consume a `relying_origins` cap slot and block valid origins after it.

## Examples

Bad:

```cpp
url::Origin target = url::Origin::Create(GURL(*target_origin_str));
if (target.opaque()) {
  return std::nullopt;  // http:// and file:// still get through.
}
```

Good:

```cpp
url::Origin target = url::Origin::Create(GURL(*target_origin_str));
if (target.opaque() ||
    !net::device_bound_sessions::IsSecure(target.GetURL())) {
  return std::nullopt;
}
```
