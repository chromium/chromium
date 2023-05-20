# 103 Early Hints

Contact: net-dev@chromium.org

As of version 103, Chrome supports
[Early Hints](https://datatracker.ietf.org/doc/html/rfc8297).
Early Hints enable browsers to preload subresources or preconnect to servers
before the main response is served. See the
[explainer](https://github.com/bashi/early-hints-explainer/blob/main/explainer.md)
how it works.

## What’s supported

Chrome supports [preload](https://w3c.github.io/preload/) and
[preconnect](https://w3c.github.io/resource-hints/#dfn-preconnect) in
Early Hints for top-level frame navigation.

## What’s not supported

To reduce security and privacy implications, the HTML and Fetch living standards
have some restrictions on when Early Hints can be handled. Chrome ignores Early
Hints sent in the following situations to comply these specifications.

* Early Hints sent on subresource requests
* Early Hints sent on iframe navigation
* Early Hints sent on HTTP/1.1 or earlier

Chrome ignores the second and following Early Hints responses. Chrome only
handles the first Early Hints response so that Chrome doesn't apply inconsistent
security policies (e.g. Content-Security-Policy).

Chrome doesn’t handle
[dns-prefetch](https://w3c.github.io/resource-hints/#dfn-dns-prefetch) and
[prefetch](https://w3c.github.io/resource-hints/#dfn-prefetch) in Early Hints
yet. We consider supporting them in the future.

## Checking Early Hints preload is working

If a resource is preloaded by Early Hints, the corresponding
[PerformanceResourceTiming](https://w3c.github.io/resource-timing/#sec-performanceresourcetiming)
object reports `initiatorType` is "early-hints".
```
performance.getEntriesByName('https://a.test/style.css')[0].initiatorType
// => 'early-hints'
```

### Reliability of `initiatorType`

Due to an implementation limitation, `initiatorType` may not always set to
"early-hints". See
[the proposal](https://docs.google.com/document/d/1V7xX2cRNxcsuIrtZk4srdZuqXynnYlPG9fFDnELXC_Y/edit?usp=sharing)
for more details.
