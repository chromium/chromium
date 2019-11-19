# Media Benchmarks

There is no detailed external version of the documentation for media Telemetry harness.

Googlers can find the internal documentation here: http://go/videostack-chromeperf-sheriff

## Overview

The media benchmarking harness consists of the
[media story set](../../../../tools/perf/page_sets/media_cases.py),
and the [media
metric](https://chromium.googlesource.com/catapult.git/+/HEAD/tracing/tracing/metrics/media_metric.html).
Those two elements are brought together by the
[media benchmark](../../../../tools/perf/benchmarks/media.py).

The media story set is a set webpages along with
instructions for automating interactions with them. The media story set consists
of two types of stories: src= stories and MSE stories. The src= stories simply
run a saved media file from start to end. The MSE stories directly call various
MSE APIs.

The media metric takes in a trace file and computes various metrics such as
seek time and time to play using the trace events.
