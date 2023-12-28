# Reporting

Reporting is a central mechanism for sending out-of-band error reports
to origins from various other components (e.g. HTTP Public Key Pinning,
Interventions, or Content Security Policy could potentially use it).

The parts of it that are exposed to the web platform are specified in three
documents:
 * The original API implemented in Chrome (Reporting V0) can be found at
     [https://www.w3.org/TR/2018/WD-reporting-1-20180925/].
 * The newer API is split into two parts. Document and worker-level reporting
     (Reporting V1) is specified in the [draft reporting spec]
     (https://w3c.github.io/reporting/), while Network-level reporting is
     specified in the [draft network reporting spec]
     (https://w3c.github.io/reporting/network-reporting.html).

This document assumes that you've read those ones.

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
          configurations (aka "clients" in the V0 spec, and the named endpoint
          per reporting source in the V1 spec).

        * The *`ReportingHeaderParser`* parses `Report-To:` and
          `Reporting-Endpoints' headers and updates the cache accordingly.

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
  `ReportingService` which is owned by the `URLRequestContext`.

* `Report-To:` headers are processed by an `HttpNetworkTransaction` when they
  are received, and passed on to the `ReportingService` to be added to the
  cache.

* `Reporting-Endpoints:` headers are initially parsed by
  `PopulateParsedHeaders`, where the raw header data is run through the
  Structured Headers parser. If valid, this structure is stored on the network
  response until a reporting source can be associated with it, and is then
  passed through the `ReportingService` to be further validated and added to the
  cache.

* A reporting source, used only by V1 reports, is a `base::UnguessableToken`
  associated with the document (or worker eventually) which configures reporting
  using a `Reporting-Endpoints:` header. This same token must be passed into
  the `ReportingService` when a report is queued for the correct endpoint to be
  found. Since the `ReportingService` in `//net` does not know anything about
  documents or workers, it tracks configurations and reports using this source
  token. Any object creating such a token is responsible for informing the
  `ReportingService` when the token will no longer be used (when the document
  is destroyed, for instance.) This will cause any outstanding reports for that
  token to be sent, and the configuration removed from the cache.

### Outside `//net`

* In the network service, a `network::NetworkContext` queues reports by getting
  the `ReportingService` from the `URLRequestContext`.

* The JavaScript [ReportingObserver](https://w3c.github.io/reporting/#observers)
  interface lives [in `//third_party/blink/renderer/core/frame/`][1].

    * It queues reports via the `NetworkContext` using a
      `blink::mojom::ReportingServiceProxy` (implemented [in
      `//content/browser/network/`][2]), which can queue Intervention, Deprecation,
      CSP Violation, and Permissions Policy Violation reports.

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

## Differences between V0 and V1 reporting

The original V0 reporting API included support for the `Report-To` header only,
which configures endpoint groups which apply to an entire origin. This is still
required for Network Error Logging, as those reports are not associated with
any successful document load.

All V0 reports destined for the same endpoint group may be bundled together for
delivery, regardless of their source (subject to NAK isolation).

V1 reporting drops the `Report-To` header in favor of `Reporting-Endpoints`,
which configures named endpoints (single URLs) which are only valid for the
network resource with which the header was sent. (In general, this means
documents and workers, since other resources do not currently generate reports.
Chrome ignores any `Reporting-Endpoints` headers on those responses.) The V1 API
does not support multiple weighted URLs for an endpoint, or failover between
them.

V1 reports from the same source may be bundled together in a single delivery,
but must be delivered separtely from other reports, even those coming from a
different `Document` object at the same URL.

## Supporting both V0 and V1 reporting in the same codebase

Chrome cannot yet drop support for NEL, and therefore for the `Report-To`
header. Until we can, it is possible for reports to be sent to endpoints
configured with either header. NEL reports can only go to those endpoint groups
configured with `Report-To`.

To support both mechanisms simultaneously, we do the following:

* V1 endpoints are stored in the cache along with V0 endpoint groups. Separate
  maps are kept of (origin -> endpoint groups) and (source token -> endpoints).

* All reports which can be associated with a specific source (currently all
  reports except for NEL, which requires origin-scoped V0 configuration) must be
  queued with that source's reporting source token.

* When a report is to be delivered, the `ReportingDeliveryAgent` will first
  attempt to find a matching V1 endpoint for the source. Only if that is
  unsuccessful, because the source is null, or because the named endpoint is not
  configured, will it fall back to searching for a matching V0 named endpoint
  group.

[1]: https://chromium.googlesource.com/chromium/src/+/HEAD/third_party/blink/renderer/core/frame/reporting_observer.h
[2]: https://chromium.googlesource.com/chromium/src/+/HEAD/content/browser/network/reporting_service_proxy.cc
[3]: https://chromium.googlesource.com/chromium/src/+/HEAD/chrome/browser/net/chrome_network_delegate.h
[4]: https://chromium.googlesource.com/chromium/src/+/HEAD/components/cronet/android/test/javatests/src/org/chromium/net/NetworkErrorLoggingTest.java
