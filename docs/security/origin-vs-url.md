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

**_Good_**
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

**_Bad_**
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
  (see also `url::Origin::FromJavaObject`
  and `url::Origin::CreateJavaObject`).


## Avoid converting URLs to origins.

**_Good_**
```c++
url::Origin origin = GetLastCommittedOrigin();
```

**_Bad_**
```c++
GURL url = ...;
GURL origin = url.GetOrigin();
// BUG: |origin| will be empty if |url| is a blob: URL like
//      "blob:http://origin/guid-goes-here".
// BUG: |origin| will be incorrect if |url| is an "about:blank" URL
//      or if |url| came from a sandboxed frame.
// NOTE: |GURL origin| is also an anti-pattern; see the "Use correct type to
//       represent origins" section below.
```

**_Risky_**
```c++
// If you know what you are doing (e.g., don't care about about:blank and/or
// sandboxed frames) and really need to convert a GURL into url::Origin then
// url::Origin::Create will correctly handle filesystem: and/or blob: URLs
// (unlike the "bad" example above).
GURL url = ...;
url::Origin origin = url::Origin::Create(url);
// |origin| will be "http://origin/" if |url| is a blob: or filesystem: URL
// like "blob:http://origin/guid-goes-here".
```
