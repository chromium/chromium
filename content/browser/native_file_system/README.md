# Native File System API

This directory contains part of the browser side implementation of the
native file system API.

See https://wicg.github.io/native-file-system/ for the spec for this API.

## Related directories

[`//storage/browser/file_system/`](../../../storage/browser/file_system) contains the
backend this API is built on top of,
[`blink/renderer/modules/native_file_system`](../../../third_party/blink/renderer/modules/native_file_system)
contains the renderer side implementation and
[`blink/public/mojom/native_file_system`](../../../third_party/blink/public/mojom/native_file_system)
contains the mojom interfaces for these APIs.
