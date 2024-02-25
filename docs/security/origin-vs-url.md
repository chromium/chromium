# Use origin (rather than URL) for security decisions.

URLs are often not sufficient for security decisions, since the origin
may not be present in the URL (e.g., `about:blank`),
may be tricky to parse (e.g., `blob:` or `filesystem:` URLs),
or may be
[opaque](https://html.spec.whatwg.org/multipage/origin.html#concept-origin-opaque)
despite a normal-looking URL (e.g., the security context may be
[sandboxed](https://developer.mozilla.org/en-US/docs/Web/HTML/Element/iframe#attr-sandbox)).
Use origins whenever possible.


## Use `GetLastCommittedOrigin()` or `GetSecurityContext()`.

### Good

```c++
// Example of browser-side code.
content::RenderFrameHost* frame = ...;
if (safelist.Matches(frame->GetLastCommittedOrigin()) {
  // ...
}

// Example of renderer-side code.  Note that browser-side checks are still
// needed to ensure that a compromised renderer cannot bypass renderer-side-only
// checks.
blink::Frame* frame = ...;
if (safelist.Matches(frame->GetSecurityContext()->GetSecurityOrigin()) {
  // ...
}
```

### Bad

```c++
// Example of browser-side code.
content::RenderFrameHost* frame = ...;
if (safelist.Matches(frame->GetLastCommittedURL()) {
  // BUG: doesn't work for about:blank or sandboxed frames.
}

// Example of renderer-side code.  Note that browser-side checks are still
// needed to ensure that a compromised renderer cannot bypass renderer-side-only
// checks.
blink::LocalFrame* frame = ...;
if (safelist.Matches(frame->GetDocument()->Url()) {
  // BUG: doesn't work for about:blank or sandboxed frames.
  // BUG: doesn't work for RemoteFrame(s) which don't have a local Document
  //      object and don't know the URL (only the origin) of the frame.
}
```


## Don't use the `GURL` type to store origins.

`GURL origin` is an anti-pattern - representing origins as a URL-focused data
type means that 1) some information is lost (e.g., origin's nonce and precursor
information) and 2) some unnecessary information may be present (e.g., URL path
and/or query parts are never part of an origin).

Use the following datatypes to represent origins:

- C++: `url::Origin` or `blink::SecurityOrigin`
  (instead of `GURL` or `blink::KURL`).
- Mojo: `url.mojom.Origin`
  (instead of `url.mojom.Url`).
  Remember to
  [validate data](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/security/mojo.md#Validate-privilege_presuming-data-received-over-IPC)
  received over IPC from untrustworthy processes
  or even better
  [avoid sending origins](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/security/mojo.md#Do-not-send-unnecessary-or-privilege_presuming-data)
  in the first place.
- Java: `org.chromium.url.Origin`
  (see also `url::Origin::FromJavaObject` and `url::Origin::ToJavaObject`).


## Avoid converting URLs to origins.

### Good

```c++
url::Origin origin = GetLastCommittedOrigin();
```

### Bad

```c++
GURL url = ...;
GURL origin = url.DeprecatedGetOriginAsURL();
// BUG: `origin` will be incorrect if `url` is an "about:blank" URL
// BUG: `origin` will be incorrect if `url` came from a sandboxed frame.
// BUG: `origin` will be incorrect when `url` (rather than
//      `base_url_for_data_url`) is used when working with loadDataWithBaseUrl
//      (see also https://crbug.com/1201514).
// BUG: `origin` will be empty if `url` is a blob: URL like
//      "blob:http://origin/guid-goes-here".
// NOTE: `GURL origin` is also an anti-pattern; see the "Use correct type to
//       represent origins" section below.

// Blink-specific example:
KURL url = ...;
scoped_refptr<SecurityOrigin> origin = SecurityOrigin::Create(url);
// BUG: `origin` will be incorrect if `url` is an "about:blank" URL
// BUG: `origin` will be incorrect if `url` came from a sandboxed frame.
```

### Risky

If you know what you are doing and really need to convert a URL into an origin,
then you can consider using `url::Origin::Create`, `url::SchemeHostPort`, or
`url::Origin::Resolve` (noting the limitations of those APIs - see below for
more details).

```c++
const GURL& url = ...;

// WARNING: `url::Origin::Create(url)` can give unexpected results if:
// 1) `url` is "about:blank", or "about:srcdoc"
//    (returning unique, opaque origin rather than the real origin of the frame)
// 2) `url` comes from a sandboxed frame
//    (potentially returning a non-opaque origin, when an opaque one is needed)
// 3) `base_url_for_data_url` should be used instead of `url`
//
// WARNING: `url::Origin::Create(url)` has some additional subtleties:
// 4) if `url` is a blob: or filesystem: URL like "blob:http://origin/blob-guid"
//    then the inner origin will be returned (unlike with `url::SchemeHostPort`)
// 5) data: URLs will be correctly be translated into opaque origins, but the
//    precursor origin will be lost (unlike with `url::Resolve`).
url::Origin origin = url::Origin::Create(url);

// WARNING: `url::SchemeHostPort(url)` will *mechanically* extract the scheme,
// host, and port of the URL, discarding other parts of the URL.  This may have
// unexpected results when:
// 1) `url` is "about:blank", or "about:srcdoc"
// 2) `url` comes from a sandboxed frame, i.e. when an opaque origin is expected
// 3) `url` is a data: URL, i.e. when an opaque origin is expected
// 4) `url` is a blob: or filesystem: URL like "blob:http://origin/blob-guid"
//    (the inner origin will *not* be returned - unlike `url::Origin::Create`)
url::SchemeHostPort scheme_host_port = url::SchemeHostPort(url);

// `url::Origin::Resolve` should work okay when:
// 1) `url` is "about:blank", or "about:srcdoc"
// 2) `url` comes from a sandboxed frame (i.e. when `base_origin` is opaque)
// 3) `url` is a data: URL (i.e. propagating precursor of `base_origin`)
// 4) `url` is a blob: or filesystem: URL like "blob:http://origin/blob-guid"
//
// WARNING: It is simpler and more robust to just use `GetLastCommittedOrigin`
// (instead of combining `GetLastCommittedOrigin` and `GetLastCommittedURL`
// using `url::Origin::Resolve`).
// WARNING: `url::Origin::Resolve` is unaware of `base_url_for_data_url`.
const url::Origin& base_origin = ...
url::Origin origin = url::Origin::Resolve(url, base_origin);
```
