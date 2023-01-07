# File System Access API

This directory contains part of the browser side implementation of the
File System Access API.

This specification for API is split across two repositories:
- https://github.com/whatwg/fs/, which specifies features available within the
  Origin Private File System, including the `SyncAccessHandle` API, and
- https://wicg.github.io/file-system-access/, which additionally specifies
  features allowing users to interact with their local file system, primarily
  via the `show*Picker()` APIs.

## Related directories

[`//storage/browser/file_system/`](../../../storage/browser/file_system) contains the
backend this API is built on top of,
[`blink/renderer/modules/file_system_access`](../../../third_party/blink/renderer/modules/file_system_access)
contains the renderer side implementation and
[`blink/public/mojom/file_system_access`](../../../third_party/blink/public/mojom/file_system_access)
contains the mojom interfaces for these APIs.
