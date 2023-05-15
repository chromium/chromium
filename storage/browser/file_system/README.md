# FileSystem API

This directory contains part of the browser side implementation of various
filesystem related APIs, as well as interfaces to treat various types of native
and non-native filesystems mostly the same without having to worry about the
underlying implementation.

## Related directories

[`//content/browser/file_system/`](../../../content/browser/file_system) contains the
rest of the browser side implementation, while
[`blink/renderer/modules/filesystem`](../../../third_party/blink/renderer/modules/filesystem)
contains the renderer side implementation and
[`blink/public/mojom/filesystem`](../../../third_party/blink/public/mojom/filesystem)
contains the mojom interfaces for these APIs.

# File System Types

There are two kinds of file system types supported by the file system
implementation: Internal types and public types.
[`//storage/common/file_system/file_system_types.h`](../../common/file_system/file_system_types.h)
has an `enum` of all the various types that exist. Many of these are Chrome OS specific or
only make sense in the context of extensions.

## Public Types

The three public file system types are:

 - Sandboxed file systems (with both a temporary and a persistent variant),
   implemented by `storage::SandboxFileSystemBackend`;
 - "Isolated" file systems, which wrap one of the internal types,
   managed by `storage::IsolatedContext` and implemented by
   `storage::IsolatedFileSystemBackend`; and
 - "External" file systems, which also wrap one of the internal types,
   managed by `storage::ExternalMountPoints`.

### External File Systems

External File Systems are only used by Chrome OS. A lot of the code for this
(besides `ExternalMountPoints` itself) lives in
[`//chrome/browser/ash/fileapi/`](../../../chrome/browser/ash/fileapi/).

TODO(mek): Document this more.

### Sandboxed File Systems

The sandbox file system backend provides for per-origin, quota-managed
file systems, as specified in the (deprecated) [File API: Directories and System
specification](https://dev.w3.org/2009/dap/file-system/file-dir-sys.html).
There are two flavors of this, "temporary" and "persistent".

This same file system (or at least the "temporary" version) is also exposed via
the new File System Access API.

### Isolated File Systems

Isolated file systems generally are used to expose files from other file system
types to the web for the [Files and Directory Entries API](https://wicg.github.io/entries-api/),
either via Drag&Drop or `<input type=file>`. They are also used for the (deprecated)
[Chrome Apps chrome.fileSystem API](https://developer.chrome.com/apps/fileSystem),
and the new [File System Access API](http://wicg.github.io/file-system-access/).

# Interesting Classes

## `FileSystemContext`

This is the main entry point for any interaction with the file system
subsystem. It is created (via `content::CreateFileSystemContext`) and owned
by each Storage Partition.

It owns:
 - Via `scoped_refptr` a optional `ExternalMountPoints` instance to
   deal with `BrowserContext` specific external file systems. Currently always
   `nullptr`, except on Chrome OS.

 - A `SandboxFileSystemBackendDelegate`. This is used by both the
   backend for the regular sandboxed file system, and for the chrome extensions
   specific "sync" file system.

 - Via `scoped_refptr` a bunch of `FileSystemBackend` instances. These
   are either created by the `FileSystemContext` itself (for sandbox and
   isolated file systems) or passed in to constructor after requesting the
   additional backends from the content embedder via
   `ContentBrowserClient::GetAdditionalFileSystemBackends`.

And further more it references:
 - An ordered set of URL crackers (`MountPoints` instances). This
   consists of the optional browser context specific `ExternalMountPoints`,
   a global singleton `ExternalMountPoints` and finally a global singleton
   `IsolatedContext`.

## `FileSystemURL`

The equivalent of a file path, but for the virtual file systems that this API
serves. `FileSystemContext::CreateFileStreamReader` takes a `FileSystemURL`
argument the same way that `fopen` takes a file path.

## `SandboxFileSystemBackend`

The main entry point of support for sandboxed file systems. This class forwards
all operations to the `FileSystemContext` owned `SandboxFileSystemBackendDelegate`
instance, which does the actual work.

### `SandboxFileSystemBackendDelegate`

The actual main entry point for sandboxed file systems, but as mentioned also
used for the Chrome Extensions specific sync file system.

This class delegates operating on files to a `ObfuscatedFileUtil`
instance it owns (wrapped in a `AsyncFileUtilAdapter`).

It is however responsible for interacting with the quota system. In order to do that
it maintains a separate cache of how much disk is being used by various origins/file
system types using a `FileSystemUsageCache` instance. This basically adds a flat file
in every directory for a origin/file system type containing the total disk usage of
that directory, and some extra meta data.

### `ObfuscatedFileUtil`

This class uses several leveldb databases to translate virtual file paths given
by arbitrary untrusted apps to obfuscated file paths that are supported by the underlying
file system. First of all it uses `SandboxPrioritizedOriginDatabase` to map
origins to paths, and then owns one `SandboxDirectoryDatabase` per origin per
file system type (each with their own leveldb database) to map files within that
origin to obfuscated file names.

See class comments for `ObfuscatedFileUtil` for more detail on how this works
as well.

Important to note that this class doesn't actually deal with any operations on the
files themselves. It merely keeps track of metadata. For actual operations on
the files this class merely translates the virtual file paths to "real" file paths
and then passes those on to a `ObfuscatedFileUtilDelegate` instance for the
actual file operations. There are two possible implementations of this delegate,
one that stores all the files in memory (for incognito mode) and one that stores
the files on disk inside the profile directory.

## `IsolatedContext`

This class keeps track of all currently registered Isolate File Systems. Isolated
file systems are identified by the hex encoding of 16 random bytes of data.

Life time of individual isolated file systems is reference counted, using
`AddReference` and `RemoveReference` methods. For certain paths it is however
also possible to entirely reokve the file system without waiting for its reference
count to go to zero.

Today reference counts are increased/decreased implicitly by granting access to
certain file systems to certain renderer processes (i.e.
`content::ChildProcessSecurityPolicyImpl` calls `AddReference` when
permission is granted, and call `RemoveReference` when the process is destroyed
on all the file systems that renderer has access to). The File System Access API
will introduce its own way of adding and removing references to these file
systems.
