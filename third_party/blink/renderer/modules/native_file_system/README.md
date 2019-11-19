# Native File System API

This directory contains the renderer side implementation of the native file
system API.

## Related directories

[`//content/browser/native_file_system/`](../../../content/browser/native_file_system)
contains the browser side implementation and
[`blink/public/mojom/native_file_system`](../../../third_party/blink/public/mojom/native_file_system)
contains the mojom interfaces for these APIs.

## APIs In this directory

This directory contains the implementation of the new and still under
development [Native File System API](https://github.com/WICG/native-file-system/blob/master/EXPLAINER.md).

It consists of the following parts:

 * `FileSystemHandle`, `FileSystemFileHandle` and `FileSystemDirectoryHandle`:
   these interfaces mimic the old `Entry` interfaces, but expose a more modern
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
