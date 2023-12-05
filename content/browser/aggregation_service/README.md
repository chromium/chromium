# Aggregation service

This directory contains the implementation of the client-side logic for the [Aggregation service](https://github.com/WICG/attribution-reporting-api/blob/main/AGGREGATE.md#data-processing-through-a-secure-aggregation-service) proposed for the [Attribution Reporting API](https://github.com/WICG/attribution-reporting-api).

Currently, this library's consumers are:

* Attribution Reporting
  ([//content/browser/attribution_reporting](/content/browser/attribution_reporting))
* Private Aggregation
  ([//content/browser/private_aggregation](/content/browser/private_aggregation))

## Processing pipeline

In broad strokes, this library exposes functions built from a few basic
operations:

* **Scheduling** a report means assembling and sending the report at a randomly
  selected time. This library schedules reports by configuring a timer that will
  run the assemble-and-send procedure when it fires. Because the browser may be
  killed at any time, it must also save the reporting time to persistent
  storage.
* **Assembly** takes a report request
  ([`AggregatableReportRequest`](https://source.chromium.org/search?q=class:content::AggregatableReportRequest%5Cb))
  and serializes and encrypts its cleartext payload with the aggregation
  service's public key to produce an aggregatable report
  ([`AggregatableReport`](https://source.chromium.org/search?q=class:content::AggregatableReport%5Cb)).
  This library automatically fetches the aggregation service server's public key
  if a valid key is not already stored on disk. For more information on payload
  encryption, see
  [payload_encryption.md](/content/browser/aggregation_service/payload_encryption.md).
* **Sending** an aggregatable report means serializing it and delivering the
  bytes to the associated reporting origin via HTTP. This library implements
  retry logic in order to tolerate transient network interruptions. (From there,
  the reporting origin batches and forwards the aggregatable reports to the
  aggregation service server, but we are now well outside the scope of the
  client-side logic implemented by this library. For more info, see [this
  section](https://github.com/WICG/attribution-reporting-api/blob/main/AGGREGATE.md#data-processing-through-a-secure-aggregation-service)
  of the Attribution Reporting documentation.)

Consumers can use the following methods of the public interface, defined in
[`aggregation_service.h`](https://source.chromium.org/chromium/chromium/src/+/main:content/browser/aggregation_service/aggregation_service.h).

1. [`AggregationService::ScheduleReport()`](https://source.chromium.org/search?q=function:content::AggregationService::ScheduleReport&sq=)
   schedules a report to be assembled and sent after a randomized delay. This is
   used for standard Private Aggregation
   [reports](https://github.com/patcg-individual-drafts/private-aggregation-api#reports).
1. [`AggregationService::AssembleAndSendReport()`](https://source.chromium.org/search?q=function:content::AggregationService::AssembleAndSendReport&sq=)
   assembles and sends a report immediately. This is used for Private
   Aggregation [duplicate debug
   reports](https://github.com/patcg-individual-drafts/private-aggregation-api#duplicate-debug-report).
1. [`AggregationService::AssembleReport()`](https://source.chromium.org/search?q=function:content::AggregationService::AssembleReport&sq=)
   just assembles a report. This is used for Attribution Reporting [aggregatable
   reports](https://github.com/WICG/attribution-reporting-api/blob/main/AGGREGATE.md#aggregatable-reports).
   Note that this consumer implements their own scheduling and sending logic.

## Histogram naming

* `PrivacySandbox.AggregationService.ScheduledRequests` contains histograms that
  pertain to requests created by `ScheduleReport()`.
* `PrivacySandbox.AggregationService.UnscheduledRequests` contains histograms
  that pertain to requests created by `AssembleAndSendReport()`.

## Command-line tool
A command-line tool that generates aggregatable reports for testing is available. Please see //tools/aggregation_service's [README](../../../tools/aggregation_service/README.md) for more detail

----

**TODO**: Expand this README.
