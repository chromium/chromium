# FileSystem API

This directory contains part of the browser side implementation of various
filesystem related APIs.

## Related directories

[`//storage/browser/fileapi/`](../../../storage/browser/fileapi) contains the
rest of the browser side implementation, while
[`blink/renderer/modules/filesystem`](../../../third_party/blink/renderer/modules/filesystem)
contains the renderer side implementation and
[`blink/public/mojom/filesystem`](../../../third_party/blink/public/mojom/filesystem)
contains the mojom interfaces for these APIs.

## In this directory

[`FileSystemManagerImpl`](file_system_manager_impl.h) is the main entry point
for calls from the renderer, it mostly redirects incoming mojom calls to a
`storage::FileSystemContext` instance.

[`FileSystemChooser`](file_system_chooser.h) uses ui::SelectFileDialog to show
a file or directory picker, and is responsible for granting a process the right
permissions for actually accessing the files that were selected.
