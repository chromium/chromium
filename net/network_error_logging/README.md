# Network Error Logging (NEL)

Network Error Logging (NEL) provides out-of-band reporting of network errors
via the Reporting API (see `//net/reporting`). Site operators can specify a
NEL policy that defines the Reporting endpoint(s) on which they wish to receive
reports about client-side errors encountered while connecting to the site. The
draft spec can be found [here](https://w3c.github.io/network-error-logging/).

This directory contains the core implementation of NEL.

## Implementation overview

Most of the action takes place in
[`NetworkErrorLoggingService`](https://source.chromium.org/chromium/chromium/src/+/main:net/network_error_logging/network_error_logging_service.h;l=42;drc=a9e9d6cbb3e5920f9207118cf9501ff0745bb536),
which handles receiving/processing `NEL:` response headers and
generating/queueing reports about network requests. The
`NetworkErrorLoggingService` is owned by the `URLRequestContext`.

Information about network requests comes directly from
[`HttpNetworkTransaction`](https://source.chromium.org/chromium/chromium/src/+/main:net/http/http_network_transaction.cc;l=1364;drc=a9e9d6cbb3e5920f9207118cf9501ff0745bb536),
which informs `NetworkErrorLoggingService` of the details of the request such
as the remote IP address and outcome (a `net::Error` code).

For dictionary-compressed (`dcb` and `dcz`) responses using an available shared
dictionary, body completion is deferred to `SharedDictionaryNetworkTransaction`
so that NEL observes the decoded body's final result rather than just the
underlying HTTP stream. Invalid compression-dictionary signatures or hashes,
content decoding failures, and oversized Zstandard windows are reported as
application-phase `http.response.invalid.content_encoding`. This classification
maps the net error codes directly without adding report fields. Transport errors,
cancellation, and success sampling retain their existing semantics. Local
dictionary read failures and unavailable decoders leave NEL deferred and retain
cancellation reporting on destruction, rather than reporting an invalid server
response. Responses reported at headers, such as HTTP errors and redirects, are
not deferred.

This mechanism covers CDT (`dcb`/`dcz`) only, not ordinary `br`, `gzip`, or `zstd`
decoding in `URLRequestHttpJob`. It reports the decoder's result; empty streams
and truncated streams ending at clean HTTP EOF retain the existing decoder
compatibility behavior.
DevTools-emulated disconnects are not forwarded as body-decoding completion.

The `NetworkErrorLoggingService` finds a NEL policy applicable to the request
(previously set by a `NEL` header), and if one exists, potentially queues a
NEL report to be uploaded out-of-band to the policy's specified Reporting
endpoint via the
[`ReportingService`](https://source.chromium.org/chromium/chromium/src/+/main:net/reporting/reporting_service.h;l=56;drc=c3305c04ac6800d488bfc8b2f3249fd13186984a).

Received NEL policies are persisted to disk by a `PersistentNelStore`, whose
main implementation is the
[`SqlitePersistentReportingAndNelStore`](https://source.chromium.org/chromium/chromium/src/+/main:net/extras/sqlite/sqlite_persistent_reporting_and_nel_store.h;l=30;drc=456596a0b27623349d38e49d0e9812b24d47d5d8).
