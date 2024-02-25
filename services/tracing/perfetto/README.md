# Perfetto in Chrome

[TOC]

## Overview

Perfetto is a project intended to provide a common platform for performance
instrumentation and logging across Chrome and Android, and this directory
contains the  code integrating the Perfetto library into Chrome. Specifically
it provides a Mojo-based transportation layer which any individual new data source can
build on to send logging protos to Perfetto, and an implementation which lets
`about://tracing` generate a Chrome Tracing trace  (`TRACE_EVENT0`, etc) using
Perfetto rather than `/base/trace_event/trace_log.cc` as a backend.

The library itself lives in [AOSP](https://android.googlesource.com/platform/external/perfetto/)
and is rolled in [/third_party/chrome/](https://cs.chromium.org/chromium/src/third_party/perfetto/).

## Perfetto Documentation

[Project page](https://android.googlesource.com/platform/external/perfetto/+/master/README.md)

[Life of a Perfetto tracing Session](https://perfetto.dev/docs/design-docs/life-of-a-tracing-session)

[Internal documentation](http://go/perfetto-project)

## Directory Structure

```
//services/tracing/                   <-- Perfetto is embedded by the tracing service
              /perfetto/              <-- Internal service implementation code
              /public/
                     /cpp/perfetto    <-- C++ client libraries used by the data source providers.
                     /mojom/          <-- Mojom interfaces
//third_party/perfetto/               <-- DEPS-rolled external library
```

## Adding a new data source

A data source is a provider of a specific type of data in the form of protobufs,
like Chrome Trace Events, memory-infra memory dumps, netlog, etc. It registers itself
with Perfetto with a given string identifier (e.g. `org.chromium.trace-event`) and if enabled
by the central Perfetto service, writes its protos into the provided Perfetto TraceWriter(s).

These data source providers can live in any child process, and the Mojo transportation layer
will take care of the details of passing them to the central service through shared memory
buffers.

To add a new data source:

* Add a new string identifier in [perfetto_service.mojom](/services/tracing/public/mojom/perfetto_service.mojom).
* Register the data source in [ProducerHost::OnConnect](/services/tracing/perfetto/producer_host.cc).
* Set up the data source in [ProducerClient::StartDataSource](/services/tracing/public/cpp/perfetto/producer_client.cc).
* Tear down the data source in [ProducerClient::StopDataSource](/services/tracing/public/cpp/perfetto/producer_client.cc).
* For each thread that wants to log a proto, use a separate TraceWriter created using
  [ProducerClient::CreateTraceWriter](/services/tracing/public/cpp/perfetto/producer_client.cc).

## Contact

For any questions about Perfetto in Chrome or adding a new data source, please
start a thread on [tracing@chromium.org](https://groups.google.com/a/chromium.org/forum/#!forum/tracing).
