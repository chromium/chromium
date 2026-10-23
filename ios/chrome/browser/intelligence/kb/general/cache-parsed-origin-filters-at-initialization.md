---
name: ios/cache-parsed-origin-filters-at-initialization
description: When filtering messages or events by URL or origin, cache parsed url::Origin objects during initialization instead of converting strings on each callback
rubric: design
severity: nit
citations:
  - path: docs/security/origin-vs-url.md
    heading: Don't use the GURL type to store origins.
    quote: 'Use the following datatypes to represent origins: - C++: url::Origin or blink::SecurityOrigin (instead of GURL or blink::KURL).'
---

**Rule.** Do not parse strings into `GURL` or `url::Origin` instances repeatedly
inside per-message or per-event filter callbacks. Parse and cache static origin
or URL filter criteria into `url::Origin` objects during construction or
initialization, and compare pre-parsed objects directly using
`IsSameOriginWith()`.

**Why.** Parsing strings into `GURL` and `url::Origin` instances involves string
parsing, canonicalization, and heap allocations. Performing these conversions
repeatedly inside message handlers or event dispatchers introduces allocation
churn on hot paths when the filter set is invariant.

## Examples

Bad:

```cpp
bool JavaScriptFeature::ShouldHandleMessageFromUrl(std::optional<GURL> url) {
  if (origin_filter_ == OriginFilter::kPublic || !url.has_value()) {
    return url.has_value();
  }
  url::Origin message_origin = url::Origin::Create(*url);
  for (NSString* origin in GetOriginFilter()) {
    if (message_origin.IsSameOriginWith(
            url::Origin::Create(GURL(base::SysNSStringToUTF8(origin))))) {
      return true;
    }
  }
  return false;
}
```

Good:

```cpp
// In constructor:
if (origin_filter_ != OriginFilter::kPublic) {
  for (NSString* origin in GetOriginList(origin_filter_)) {
    origin_filter_origins_.push_back(
        url::Origin::Create(GURL(base::SysNSStringToUTF8(origin))));
  }
}

// In message handler:
bool JavaScriptFeature::ShouldHandleMessageFromOrigin(
    const url::Origin& origin) {
  if (origin_filter_ == OriginFilter::kPublic) {
    return true;
  }
  for (const url::Origin& filter_origin : origin_filter_origins_) {
    if (origin.IsSameOriginWith(filter_origin)) {
      return true;
    }
  }
  return false;
}
```
