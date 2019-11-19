# Reporting

Reporting is a central mechanism for sending out-of-band error reports
to origins from various other components (e.g. HTTP Public Key Pinning,
Interventions, or Content Security Policy could potentially use it).

The parts of it that are exposed to the web platform are specified in the [draft
spec](https://w3c.github.io/reporting/). This document assumes that you've read
that one.

## Reporting in Chromium

Reporting is implemented as part of the network stack in Chromium, such
that it can be used by other parts of the network stack (e.g. HPKP) or
by non-browser embedders as well as by Chromium.

### Inside `//net`

* The top-level class is the *`ReportingService`*. This lives in the
  `URLRequestContext`, and provides the high-level operations used by
  other parts of `//net` and other components: queueing reports,
  handling configuration headers, clearing browsing data, and so on.

    * A *`ReportingPolicy`* specifies a number of parameters for the Reporting
      API, such as the maximum number of reports and endpoints to queue, the
      time interval between delivery attempts, whether or not to persist reports
      and clients across network changes, etc. It is used to create a
      `ReportingService` obeying the specified parameters.

    * Within `ReportingService` lives *`ReportingContext`*, which in turn
      contains the inner workings of Reporting, spread across several classes:

        * The *`ReportingCache`* stores undelivered reports and endpoint
          configurations (aka "clients" in the spec).

        * The *`ReportingHeaderParser`* parses `Report-To:` headers and updates
          the cache accordingly.

        * The *`ReportingDeliveryAgent`* reads reports from the cache, decides
          which endpoints to deliver them to, and attempts to do so. It uses a
          couple of helper classes:

            * The *`ReportingUploader`* does the low-level work of delivering
              reports: accepts a URL and JSON from the `DeliveryAgent`, creates
              a `URLRequest`, and parses the result. It also handles sending
              CORS preflight requests for cross-origin report uploads.

            * The *`ReportingEndpointManager`* chooses an endpoint from the
              cache when one is requested by the `ReportingDeliveryAgent`, and
              manages exponential backoff (using `BackoffEntry`) for failing
              endpoints.

        * The *`ReportingGarbageCollector`* periodically examines the cache
          and removes reports that have remained undelivered for too long, or
          that have failed delivery too many times.

        * The *`ReportingBrowsingDataRemover`* examines the cache upon request
          and removes browsing data (reports and endpoints) of selected types
          and origins.

        * The *`ReportingDelegate`* calls upon the `NetworkDelegate` (see below)
          to check permissions for queueing/sending reports and setting/using
          clients.

* The `ReportingService` is set up in a `URLRequestContext` by passing a
  `ReportingPolicy` to the `URLRequestContextBuilder`. This creates a
  `ReportingService` which is owned by the `URLRequestContextStorage`.

* `Report-To:` headers are processed by an `HttpNetworkTransaction` when they
  are received, and passed on to the `ReportingService` to be added to the
  cache.

### Outside `//net`

* In the network service, a `network::NetworkContext` queues reports by getting
  the `ReportingService` from the `URLRequestContext`.

* The JavaScript [ReportingObserver](https://w3c.github.io/reporting/#observers)
  interface lives [in `//third_party/blink/renderer/core/frame/`][1].

    * It queues reports via the `NetworkContext` using a
      `blink::mojom::ReportingServiceProxy` (implemented [in
      `//content/browser/net/`][2]), which can queue Intervention, Deprecation,
      CSP Violation, and Feature Policy Violation reports.

* The `ChromeNetworkDelegate` [in `//chrome/browser/net/`][3] checks permissions
  for queueing reports and setting/using clients based on whether cookie access
  is allowed, and checks permissions for sending reports using a
  `ReportingPermissionsChecker`, which checks whether the user has allowed
  report uploading via the BACKGROUND_SYNC permission.

* Cronet can configure "preloaded" `Report-To:` headers (as well as Network
  Error Logging headers) when initializing a `CronetURLRequestContext`, to allow
  embedders to collect and send reports before having received a header in an
  actual response.

    * This functionality is tested on Android by way of sending Network Error
      Logging reports [in the Cronet Java tests][4].

[1]: https://chromium.googlesource.com/chromium/src/+/HEAD/third_party/blink/renderer/core/frame/reporting_observer.h
[2]: https://chromium.googlesource.com/chromium/src/+/HEAD/content/browser/net/reporting_service_proxy.cc
[3]: https://chromium.googlesource.com/chromium/src/+/HEAD/chrome/browser/net/chrome_network_delegate.h
[4]: https://chromium.googlesource.com/chromium/src/+/HEAD/components/cronet/android/test/javatests/src/org/chromium/net/NetworkErrorLoggingTest.java
