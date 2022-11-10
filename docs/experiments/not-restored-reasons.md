# Back/forward cache NotRestoredReasons API

Contact: bfcache-dev@chromium.org

This document describes the status of the current implementation of the
[NotRestoredReasons API](https://github.com/rubberyuzu/bfcache-not-retored-reason/blob/main/NotRestoredReason.md)
in Chrome, and how to enable it.

Starting from version 108,
Chrome experimentally supports NotRestoredReasons API.
This allows sites to access what reasons are preventing their pages from
being restored from back/forward cache..

Note that this API is not available by default.
Chrome plans to do an origin trial
to evaluate its effectiveness
and to allow site authors to give feedback.

## What’s supported

A new field [notRestoredReasons](https://github.com/rubberyuzu/bfcache-not-retored-reason/blob/main/NotRestoredReason.md) is added to 
performanceNavigationTiming API.
This new field will report the reasons that prevented back/forward cache on the page in a frame tree structure.

## Activation

The policy can be enabled in several ways.

### Using the Chromium UI

Enable the flag chrome://flags/#enable-experimental-web-platform-features .
You also have to enable back/forward cache flag via chrome://flags/#back-forward-cache .

### Using Origin Trial

The [Origin Trial](https://developer.chrome.com/blog/origin-trials/) feature is named `NotRestoredReasons`.
Registration for the trial is [here](https://developer.chrome.com/origintrials/#/view_trial/3101854243351429121).
See [feature status](https://chromestatus.com/feature/5760325231050752) to find out
if the origin trial is currently running.

The [origin trial tutorial](https://developer.chrome.com/docs/web-platform/origin-trials/#take-part-in-an-origin-trial) describes how to participate.

### Verifying the API is working

In devtools, run the following piece of javascript after navigating to a page, and clicking back/forward button.

```js
performance.getEntriesByType('navigation')[0].notRestoredReasons;
```

If it gives you a value, it succeeds.
If it gives `undefined` then the feature is not enabled.

## Related Links

- [Chrome Status](https://chromestatus.com/feature/5760325231050752)
- [NotRestoredReasons API Explainer](https://github.com/rubberyuzu/bfcache-not-retored-reason/blob/main/NotRestoredReason.md)
