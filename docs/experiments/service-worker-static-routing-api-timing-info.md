# Timing information for ServiceWorker Static Routing API

- Contact: chrome-loading@google.com

This document describes the status of the current implementation of the
[**Timing Info for ServiceWorker Static Routing API**](https://github.com/WICG/service-worker-static-routing-api/blob/main/resource-timing-api.md)
in Chrome, and how to enable it.

Note that this feature is not available by default.
Chrome plans to do an origin trial starting from M131 to evaluate its
effectiveness and to allow site authors to give feedback.

## What’s supported

The API is implemented according to the
[explainer](https://github.com/WICG/service-worker-static-routing-api/blob/main/resource-timing-api.md).

Following new fields are added to the resourceTimingAPI:
- workerMatchedRouterSource
- workerActualRouterSource
- workerCacheLookupStart

In addition, a new value `cache-storage` is added to the `deliveryType` field.
Please refer to the explainer for details about each field.

## Activation

The API can be enabled by participating in the [Origin Trial](https://developer.chrome.com/blog/origin-trials/).

The feature is named `ServiceWorkerStaticRouterTimingInfo`.
Registration for the trial is [here](https://developer.chrome.com/origintrials/#/view_trial/1689412810217357313).
See [feature status](https://chromestatus.com/feature/6309742380318720) to find out
if the origin trial is currently running.

The [origin trial tutorial](https://developer.chrome.com/docs/web-platform/origin-trials/#take-part-in-an-origin-trial) describes how to participate.


## Verifying the API is working

In devtools, run the following piece of javascript after navigating to a page
which uses ServiceWorker static routing API.

```js
performance.getEntriesByType('navigation')[0].matchedSourceType;
```

If it gives you a value, it succeeds.
If it gives `undefined` then the feature is not enabled.

## Related Links

- [Timing Info for ServiceWorker Static Routing API Explinaer on GitHub](https://github.com/WICG/service-worker-static-routing-api/blob/main/resource-timing-api.md)
- [Chrome Status](https://chromestatus.com/feature/6309742380318720)
