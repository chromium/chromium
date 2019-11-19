# ContentCapture

[Rendered](https://chromium.googlesource.com/chromium/src/+/master/third_party/blink/renderer/core/content_capture/README.md)

This directory contains ContentCapture which is for capturing on-screen text
content and streaming it to a client.

The implementation injects a cc::NodeId into cc::DrawTextBlobOp in paint
stage, schedules a best-effort task to retrieve on-screen text content (using
an r-tree to capture all cc::NodeId intersecting the screen), and streams
the text out through ContentCaptureClient interface. The ContentCaptureTask is
a best-effort task in the idle queue and could be paused if there are
higher-priority tasks.
