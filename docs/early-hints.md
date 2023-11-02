# 103 Early Hints

Contact: early-hints-experiment@chromium.org

As of version 95, Chrome experimentally supports
[Early Hints](https://datatracker.ietf.org/doc/html/rfc8297).
Early Hints enable browsers to preload subresources or preconnect to servers
before the main response is served. See the
[explainer](https://github.com/bashi/early-hints-explainer/blob/main/explainer.md)
how it works.

Currently Chrome is running A/B testing in the field to evaluate the performance
impact of Early Hints. Chrome also provides some ways to opt-in Early Hints for
web developers who want to try the feature. This document describes the status
of the current implementation and how to enable Early Hints support.

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

## Activation

Early Hints can be enabled by a command line flag, or via
[Origin Trial](https://developer.chrome.com/blog/origin-trials/).

### Using command line flag

Passing the `--enable-features=EarlyHintsPreloadForNavigation` command line flag
to Chrome enables Early Hints support.

### Using Origin Trial

**Note**: As of version 98 the origin trial has expired. The Chrome networking
team is preparing to ship the feature and the origin trial may be available
again in the near future until it's fully shipped.

You can opt any page on your origin into Early Hints by
[requesting a token for your origin](https://developer.chrome.com/origintrials/#/view_trial/2856408063659737089).
Include the token in both Early Hints and the final response so that Chrome can
recognize your pages opted in Early Hints.

```
HTTP/1.1 103 Early Hints
Origin-Trial: **your token**
Link: </style.css>; rel="preload"; as="style"
Link: <https://cdn.test>; rel="preconnect"

HTTP/1.1 200 OK
Origin-Trial: **your token**
Link: </style.css>; rel="preload"; as="style"
Link: <https://cdn.test>; rel="preconnect"
Content-Type: text/html; charset=utf-8

<!DOCTYPE html>
...
```

`<meta http-equiv="origin-trial" content="**your token**">` also works for the
final response but doesn’t work for Early Hints since Early Hints cannot convey
a response body.

### Checking Early Hints preload is working

If a resource is preloaded by Early Hints, the corresponding
[PerformanceResourceTiming](https://w3c.github.io/resource-timing/#sec-performanceresourcetiming)
object reports `initiatorType` is "early-hints".
```
performance.getEntriesByName('https://a.test/style.css')[0].initiatorType
// => 'early-hints'
```

## Resources

* [Fastly's test page](https://early-hints.fastlylabs.com/)
