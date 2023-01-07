# FileSystem API

This directory contains the renderer side implementation of various filesystem
related APIs.

## Related directories

[`//storage/browser/file_system/`](../../../../../storage/browser/file_system)
contains part of the browser side implementation, while
[`//content/browser/file_system/`](../../../../../content/browser/file_system)
contains the rest of the browser side implementation and
[`blink/public/mojom/filesystem`](../../../public/mojom/filesystem) contains the
mojom interfaces for these APIs.

## APIs In this directory

### File and Directory Entries API

First of all this directory contains the implementation of the
[Entries API](https://wicg.github.io/entries-api). This API consists of
types to expose read-only access to file and directory entries to the web,
primarily used by drag-and-drop and `<input type=file>`. Our implementation
doesn't match the interface names of the spec, but otherwise should be pretty
close to the spec.

TODO(mek): More details

### File API: Directories and FileSystem

Secondly this directory contains the implementation of something similar to the
deprecated [w3c file-system-api](https://www.w3.org/TR/2012/WD-file-system-api-20120417/).
This API is very similar to the previous Entries API, but it also adds support
for writing and modifying to files and directories, as well as a way to get
access to a origin scoped sandboxed filesystem.

TODO(mek): More details
