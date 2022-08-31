# Page Unload Beacon API

Contact: pending-beacon-experiment@chromium.org

This document describes the status of the current implementation of the
[Page Unload Beacon API](https://wicg.github.io/unload-beacon/)
(a.k.a. PendingBeacon API) in Chrome, and how to enable it manually.

Starting from version 106 Beta, Chrome experimentally supports the
Page Unload Beacon API,
which allows website authors to specify one or more beacons (HTTP requests) that should be sent reliably when the page is being unloaded.

See the [public explainer](https://github.com/WICG/unload-beacon#readme) about
how it works.

Note that this API is not enabled by default. Instead, Chrome plans to run A/B testing to evaluate its impact. But Chrome also provides some ways to opt-in to the API for web developers who what to try the features.

## What’s supported

Chrome supports all the JavaScript APIs described in the explainer, specifically:

- [`class PendingPostBeacon`](https://github.com/WICG/unload-beacon#pendingpostbeacon)
- [`class PendingGetBeacon`](https://github.com/WICG/unload-beacon#pendinggetbeacon)
- and all of the properties and methods described in [PendingBeacon](https://github.com/WICG/unload-beacon#pendingbeacon), with some behaviors not supported.

## What’s not supported

The following features are not yet supported in Chrome:

- Crash recovery from disk upon next launching Chrome: not yet supported. Chrome currently doesn't store any PendingBeacon on disk.
- Delete pending beacons for a site if a user clears site data: not supported yet, as crash recovery from disk is not yet supported.
- Beacons are only sent over the same network that was active when the beacon was registered: not supported yet.
- Post-unload beacons are not sent if background sync is disabled for a site: not supported yet.
- Beacon requests are not yet observable in Chrome DevTools.

The following features work differently than the one described in explainer:

- The beacon destination URL should be modifiable: only `PendingGetBeacon` can
  update its URL via `setURL()` method.
- Beacon Sending behavior: the Chrome implementation currently queues all
  created instances of Pending*Beacon for sending. But in the explainer, it
  specifies that a `PendingPostBeacon` is only queued if it has non-undefined
  and non-null data (described in `setData()` method).
- Beacons must be sent over HTTPS: current implementation doesn't enforce HTTPS,
  which means if web developer creates a Pending*Beacon with HTTP URL property, it will still work.
- Beacons max TTL is bound by Chrome's back/forward cache TTL, which is currently 3 minutes.

## Activation

The API can be enabled by a command line flag, or via
[Origin Trial](https://developer.chrome.com/blog/origin-trials/).

### Using command line flag

Passing the `--enable-features=PendingBeaconAPI` command line flag
to Chrome enables PendingBeacon API support.

### Using Origin Trial

TODO: Add tutorial once Origin Trial is approved.

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

- [Page Unload Beacon Design Doc in Chromium](https://docs.google.com/document/d/1QIFUu6Ne8x0W62RKJSoTtZjSd_bIM2yXZSELxdeuTFo/edit#)
- [Chrome Status](https://chromestatus.com/feature/5690553554436096)
- [Page Unload Beacon Explainer on GitHub](https://github.com/WICG/unload-beacon#readme)
- [Page Unload Beacon API Spec (draft)](https://wicg.github.io/unload-beacon/)
- Ask questions about API & Spec via [new issue](https://github.com/WICG/unload-beacon/issues/new)
