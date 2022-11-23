# File System Access API

This directory contains the renderer side implementation of the file
system access API.

## Related directories

[`//content/browser/file_system_access/`](../../../content/browser/file_system_access)
contains the browser side implementation and
[`blink/public/mojom/file_system_access`](../../../third_party/blink/public/mojom/file_system_access)
contains the mojom interfaces for these APIs.

## APIs In this directory

This directory contains the implementation of the File System Access API. This
API spans two specifications:
  * [whatwg/fs](https://fs.spec.whatwg.org/) specifies access to the
    [origin private file system](https://fs.spec.whatwg.org/#origin-private-file-system)
    and all shared interfaces.
  * [WICG/file-system-access](https://wicg.github.io/file-system-access/)
    specifies access to the local file system, including the picker APIs and
    drag-and-drop.

It consists of the following parts:

 * `FileSystemHandle`, `FileSystemFileHandle` and `FileSystemDirectoryHandle`:
   these interfaces mimic the old `Entry` interfaces, but expose a more modern
   promisified API.

 * `StorageManager.getDirectory`: An entry point that gives access to the same
   [sandboxed filesystem](https://fs.spec.whatwg.org/#origin-private-file-system)
   as what is available through the old API.

 * `FileSystemWritableFileStream`: a more modern API with similar functionality to the
   old `FileWriter` API. The implementation of this actually does make use of
   a different mojom interface than the old API. But since the functionality is
   mostly the same, hopefully we will be able to migrate the old implementation
   to the new mojom API as well.

 * `showOpenFilePicker`, `showSaveFilePicker` and `showDirectorPicker`: Entry points
   on `window`, that let a website pop-up a file or directory picker, prompting the
   user to select one or more files or directories, to which the website than gets access.
