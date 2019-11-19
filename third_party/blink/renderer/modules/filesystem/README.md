# FileSystem API

This directory contains the renderer side implementation of various filesystem
related APIs.

## Related directories

[`//storage/browser/file_system/`](../../../storage/browser/file_system) contains part
of the browser side implementation, while
[`//content/browser/fileapi/`](../../../content/browser/fileapi) contains the
rest of the browser side implementation and
[`blink/public/mojom/filesystem`](../../../third_party/blink/public/mojom/filesystem)
contains the mojom interfaces for these APIs.

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

### Writable Files

Finally this directory contains the implementation of the new and still under
development [Writable Files API](https://github.com/WICG/writable-files/blob/master/EXPLAINER.md).
This API is mostly implemented on top of the same backend as the previous two
APIs, but hopes to eventually replace both of those, while also adding new
functionality.

It consists of the following parts:

 * `FileSystemBaseHandle`, `FileSystemFileHandle` and `FileSystemDirectoryHandle`:
   these interfaces mimic the old `Entry` interfaces (and inherit from `EntryBase`
   to share as much of the implementation as possible), but expose a more modern
   promisified API.

 * `getSystemDirectory`: An entry point (exposed via `FileSystemDirectoryHandle`)
   that today only gives access to the same sandboxed filesystem as what was
   available through the old API. In the future this could get extended to add
   support for other directories as well.

 * `FileSystemWriter`: a more modern API with similar functionality to the
   old `FileWriter` API. The implementation of this actually does make use of
   a different mojom interface than the old API. But since the functionality is
   mostly the same, hopefully we will be able to migrate the old implementation
   to the new mojom API as well.

 * `chooseFileSystemEntries`: An entry point, currently on `window`, that lets
   a website pop-up a file picker, prompting the user to select one or more
   files or directories, to which the website than gets access.

Since the `Handle` interfaces are based on the implementation of the `Entry`
interfaces, internally and across IPC these are still represented by
`filesystem://` URLs. Hopefully in the future we will be able to change this and
turn it into a more capabilities based API (where having a mojo handle gives you
access to specific files or directories), as with the current implementation it
is very hard to properly support transferring handles to other processes via
postMessage (which is something we do want to support in the future).
