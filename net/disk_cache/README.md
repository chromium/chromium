# disk_cache API

The disk_cache API provides for caches that can store multiple random-access
byte streams of data associated with a key on disk (or in memory).

There are two kinds of entries that can be stored: regular and sparse.

Regular entries contain up to 3 separate data streams.  Usually stream 0
would be used for some kind of primary small metadata (e.g. HTTP headers);
stream 1 would contain the main payload (e.g. HTTP body); and stream 2 would
optionally contain some auxiliary metadata that's needed only some of the time
(e.g. V8 compilation cache).  There is no requirement that these streams be used
in this way, but implementations may expect similar size and usage
characteristics.

Sparse entries have a stream 0 and a separate sparse stream that's accessed with
special methods that have `Sparse` in their names. It's an API misuse to try to
access streams 1 or 2 of sparse entries or to call `WriteSparseData` on entries
that have contents in those streams. Calling `SparseReadData` or
`GetAvailableRange` to check whether entries are sparse is, however, permitted.
An added entry becomes a regular entry once index 1 or 2 is written to, or it
becomes a sparse entry once the sparse stream is written to.  Once that is done,
it cannot change type and the access/modification restrictions relevant to the
type apply.  Type of an entry can always be determined using `SparseReadData` or
`GetAvailableRange`.

The sparse streams are named as such because they are permitted to have holes in
the byte ranges of contents they represent (and implementations may also drop
some pieces independently). For example, in the case of a regular entry,
starting with an empty entry, and performing `WriteData` on some stream at
offset = 1024, length = 1024, then another `WriteData` at offset = 3072,
length = 1024, results in the stream having length = 4096, and the areas not
written to filled in with zeroes.

In contrast, after the same sequence of `WriteSparseData` operations, the entry
will actually keep track that [1024, 2048) and [3072, 4096) are valid, and will
permit queries with `GetAvailableRange`, and only allow reads of the defined
ranges.

[`net/disk_cache/disk_cache.h`](/net/disk_cache/disk_cache.h) is the only
include you need if you just want to use this API.
`disk_cache::CreateCacheBackend()` is the first method you'll need to call.

[TOC]

## The implementations

### [disk_cache/blockfile directory](/net/disk_cache/blockfile/)

This implementation backs the HTTP cache on Windows. It tries to pack
many small entries caches typically have into "block" files, which can help
performance but introduces a lot of complexity and makes recovery from
corruption very tricky.

### [disk_cache/memory directory](/net/disk_cache/memory/)

This contains the in-memory-only implementation.  It's used for incognito
mode.

### [disk_cache/simple directory](/net/disk_cache/simple/)

This implementation backs the HTTP cache on Android, ChromeOS, and Linux,
macOS and is used to implement some features like CacheStorage on all platforms.
The design is centered around roughly having a single file per cache entry (more
precisely for streams 0 and 1), with a compact and simple in-memory index for
membership tests, which makes it very robust against failures, but also highly
sensitive to OS file system performance.
