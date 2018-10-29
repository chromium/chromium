# FileSystem API

This directory contains part of the browser side implementation of various
filesystem related APIs, as well as interfaces to treat various types of native
and non-native filesystems mostly the same without having to worry about the
underlying implementation.

## Related directories

[`//content/browser/fileapi/`](../../../content/browser/fileapi) contains the
rest of the browser side implementation, while
[`blink/renderer/modules/filesystem`](../../../third_party/blink/renderer/modules/filesystem)
contains the renderer side implementation and
[`blink/public/mojom/filesystem`](../../../third_party/blink/public/mojom/filesystem)
contains the mojom interfaces for these APIs.
