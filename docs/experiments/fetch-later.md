# fecthLater API

- Contact: pending-beacon-experiment@chromium.org
- API Feedback: https://github.com/WICG/pending-beacon/issues

This document describes the status of the current implementation of the
[**FetchLater API**][spec-pr] in Chrome, and how to enable it manually.

Starting from [version 121][status], Chrome experimentally supports the
FetchLater API,
which allows website authors to specify one or more beacons (HTTPS requests)
that should be sent reliably when the page is being unloaded.

See the [public explainer][explainer] to learn more about how it works.

Note that this API is not enabled by default. Instead, Chrome plans to run
Origin Trials to evaluate its impact. But Chrome also provides some ways to
fully opt-in to the API for web developers who what to try the features.

[spec-pr]: https://github.com/whatwg/fetch/pull/1647
[explainer]: https://github.com/WICG/pending-beacon/blob/main/docs/fetch-later-api.md
[status]: https://chromestatus.com/feature/4654499737632768

## What’s supported

Chrome supports all the JavaScript APIs described in the spec PR,
specifically:

- [`fetchLater()`](https://whatpr.org/fetch/1647/9ca4bda...37a66c9.html#dom-global-fetch-later)
  method, a `fetch()`-like API.
- [`Deferred fetching`](https://whatpr.org/fetch/1647/9ca4bda...37a66c9.html#deferred-fetching)
  behavior.

## What’s not supported

The following features are not finalized in the spec PR, and hence not supported
in Chrome:

- The API is only supported in Document context. Supporting in ServiceWorker is
  blocked by ServiceWorker version of https://crbug.com/1356128.
- Crash recovery related behaviors is not supported.
  (Discussed in [pending-beacon/issues/34](https://github.com/WICG/pending-beacon/issues/34))
- Retry after network failure is not supported.
  (Discussed in [pending-beacon-/issues/40](https://github.com/WICG/pending-beacon/issues/40))
- A fetchLater request is not observable in Chrome DevTools after its initiating
  document is closed, which is due to the current
  [DevTools limitation](https://chromestatus.com/feature/4654499737632768?gate=4947446974644224);
  if the document is still alive, the request should be visible.

The following features work differently than the one described in explainer and
spec:

- To address the privacy requirement (see
  [pending-beacon/issues/30](https://github.com/WICG/pending-beacon/issues/30#issuecomment-1888554622)),
  any pending fetchLater requests on a document are **all flushed out** if the
  document enters BFCache, no matter BackgroundSync permission is on or not.
- The maximum time a fetchLater request can be pending is bound by Chrome's
  back/forward cache TTL, which is currently **10 minutes** after page goes into
  the cache. Note that due to the above forced-flushing behavior, in reality
  there should be no request pending after a page being cached.

## Activation

The API can be enabled by a command line flag.

### Using command line flag

Passing `--enable-features=FetchLaterAPI --enable-blink-features=FetchLaterAPI`
command line flag to Chrome enables FetchLater API support.

### Verifying the API is working

Added the following line to an HTTPS web page, and load the page into a Chrome
tab.

```html
<script>
fetchLater('/test');
</script>
```

Close the tab, and you should be able to observe a request sent to `/test` on
your web server that hosts the page.

## Related Links

- [Chrome Platform Status - Feature: FetchLater API][status]
- [FetchLater Explainer on GitHub](https://github.com/WICG/pending-beacon/blob/main/docs/fetch-later-api.md)
- [FetchLater API Spec (draft)](https://whatpr.org/fetch/1647/9ca4bda...37a66c9.html#dom-global-fetch-later)
- Ask questions about API & Spec via [new issue](https://github.com/WICG/pending-beacon/issues/new)
- [FetchLater API Design Doc in Chromium](https://docs.google.com/document/d/1U8XSnICPY3j-fjzG35UVm6zjwL6LvX6ETU3T8WrzLyQ/edit#heading=h.ms1oipx914vf)
