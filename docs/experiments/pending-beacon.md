# Pending Beacon API

- Status: **Deprecated** (Replaced by [FetchLater API](fetch-later.md))
- Contact: pending-beacon-experiment@chromium.org
- API Feedback: https://github.com/WICG/pending-beacon/issues

This document describes the status of the current implementation of the
[**Pending Beacon API**](https://wicg.github.io/pending-beacon/)
(a.k.a. PendingBeacon API) in Chrome, and how to enable it manually.

Starting from [version 107][status], Chrome experimentally supports the
Pending Beacon API,
which allows website authors to specify one or more beacons (HTTP requests) that
should be sent reliably when the page is being unloaded.

See the [public explainer](https://github.com/WICG/pending-beacon#readme) to
learn more about how it works.

Note that this API is not enabled by default. Instead, Chrome plans to run A/B
testing to evaluate its impact. But Chrome also provides some ways to fully
opt-in to the API for web developers who what to try the features.

## What’s supported

Chrome supports all the JavaScript APIs described in the explainer,
specifically:

- [`class PendingPostBeacon`](https://github.com/WICG/pending-beacon#pendingpostbeacon)
- [`class PendingGetBeacon`](https://github.com/WICG/pending-beacon#pendinggetbeacon)
- and all of the properties and methods described in
  [PendingBeacon](https://github.com/WICG/pending-beacon#pendingbeacon), with
  some behaviors not supported.

## What’s not supported

The following features are not yet supported in Chrome:

- When calling `setData(data)` on a `PendingPostBeacon`, the `data` payload
  cannot be
  - A complex body, i.e. for a `data` of `FormData` type, it can only have
    single [entry/part][formdata-entry].
  - A streaming body, i.e. a `data` cannot be a [ReadableStream].
- Crash recovery related behaviors and privacy requirements: not yet supported.
  Chrome currently doesn't store any PendingBeacon on disk.
- Delete pending beacons for a site if a user clears site data: not supported
  yet, as crash recovery from disk is not yet supported.
- Beacon requests are not yet observable in Chrome DevTools.

The following features work differently than the one described in explainer:

- Beacons must not leak navigation history to the network provider that it
  should not know:
  supported by forcing to send out all beacons on navigating away from a
  document.
- The beacon destination URL should be modifiable: only `PendingGetBeacon` can
  update its URL via `setURL()` method.
- Beacon Sending behavior: the Chrome implementation currently queues all
  created instances of Pending*Beacon for sending. But in the explainer, it
  specifies that a `PendingPostBeacon` is only queued if it has non-undefined
  and non-null data (described in `setData()` method).
- Beacons must be sent over HTTPS: current implementation doesn't enforce HTTPS,
  which means if web developer creates a `Pending*Beacon` with HTTP URL
  property, it will still work.
- Beacons max TTL is bound by Chrome's back/forward cache TTL, which is
  currently **10 minutes**.

[formdata-entry]: https://developer.mozilla.org/en-US/docs/Web/API/FormData/entries
[ReadableStream]: https://developer.mozilla.org/en-US/docs/Web/API/ReadableStream

## Activation

The API can be enabled by a command line flag, or via
[Origin Trial](https://developer.chrome.com/blog/origin-trials/).

### Using command line flag

Passing the `--enable-features=PendingBeaconAPI` command line flag
to Chrome enables PendingBeacon API support.

### Using Origin Trial

[**Trial for Pending Beacon**](https://developer.chrome.com/origintrials/#/view_trial/1581889369113886721).

You can opt any page on your origin into PendingBeacon API Origin Trial by
[requesting a token][ot-tutorial] for your origin via the above link. Include
the token in your page so that Chrome can recognize your page is opted in.

The simplest way is to include the following line in your page:

```html
<meta http-equiv="origin-trial" content="**your token**">
```

Or you can also include the token in your HTTP request.

*** note
**NOTE**: Even with the Origin Trial token, **NOT all of visits to your page**
will have the API enabled. It is because as mentioned above, Chrome plan to do
A/B testing on the API.
***

[ot-tutorial]: https://developer.chrome.com/docs/web-platform/origin-trials/#take-part-in-an-origin-trial

### Verifying the API is working

Added the following line to a web page, and load the page into a Chrome tab.

```html
<script>
new PendingGetBeacon('/test');
</script>
```

Close the tab, and you should be able to observe a request sent to `/test` on
your web server that hosts the page.

## Related Links

- [Chrome Platform Status - Feature: Declarative PendingBeacon API][status]
- [Pending Beacon Explainer on GitHub](https://github.com/WICG/pending-beacon#readme)
- [Pending Beacon API Spec (draft)](https://wicg.github.io/pending-beacon/)
- Ask questions about API & Spec via [new issue](https://github.com/WICG/pending-beacon/issues/new)
- [Pending Beacon Design Doc in Chromium](https://docs.google.com/document/d/1QIFUu6Ne8x0W62RKJSoTtZjSd_bIM2yXZSELxdeuTFo/edit#)

[status]: https://chromestatus.com/feature/5690553554436096
