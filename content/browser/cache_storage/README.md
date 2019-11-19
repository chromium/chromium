# Architecture (as of July 29th 2016)
This document describes the browser-process implementation of the [Cache
Storage specification](
https://slightlyoff.github.io/ServiceWorker/spec/service_worker/index.html).

As of June 2018, Chrome components can use the Cache Storage interface via
`CacheStorageManager` to store Request/Response key-value pairs. The concept of
`CacheStorageOwner` was added to distinguish and isolate the different
components.

## Major Classes and Ownership
### Ownership
Where '=>' represents ownership, '->' is a reference, and '~>' is a weak
reference.

##### `CacheStorageContextImpl`->`CacheStorageManager`=>`CacheStorage`=>`CacheStorageCache`
* A `CacheStorageManager` can own multiple `CacheStorage` objects.
* A `CacheStorage` can own multiple `CacheStorageCache` objects.

##### `StoragePartitionImpl`->`CacheStorageContextImpl`
* `StoragePartitionImpl` effectively owns the `CacheStorageContextImpl` in the
  sense that it calls `CacheStorageContextImpl::Shutdown()` on deletion which
  resets its `CacheStorageManager`.

##### `RenderProcessHost`->`CacheStorageDispatcherHost`->`CacheStorageContextImpl`

##### `CacheStorageDispatcherHost`=>`CacheStorageCacheHandle`~>`CacheStorageCache`
* The `CacheStorageDispatcherHost` holds onto handles for:
  * JavaScript references to cache objects

##### `CacheStorageDispatcherHost`=>`CacheStorageHandle`~>`CacheStorage`
* The `CacheStorageDispatcherHost` holds onto handles for:
  * JavaScript references to caches

##### `CacheStorageCacheDataHandle`=>`CacheStorageCacheHandle`~>`CacheStorageCache`
* `CacheStorageCacheDataHandle` is the blob data handle for a response body
  and it holds a `CacheStorageCacheHandle`.  It streams from the
  `disk_cache::Entry` response stream. It's necessary that the
  `disk_cache::Backend` (owned by `CacheStorageCache`) stays open so long as
  one of its `disk_cache::Entry`s is reachable. Otherwise, a new backend might
  open and clobber the entry.

##### `CacheStorageCache`=>`CacheStorageCacheHandle`~>`CacheStorageCache`
* The `CacheStorageCache` will hold a self-reference while executing an
  operation.  This self-reference is dropped between subsequent operations,
  so shutdown is possible when there are no external references even if there
  are more operations in the scheduler queue.

### CacheStorageDispatcherHost
1. Receives IPC messages from a render process and creates the appropriate
   `CacheStorageManager` or `CacheStorageCache` operation.
2. For each operation, holds a `CacheStorageCacheHandle` to keep the cache
   alive since the operation is asynchronous.
3. For each cache reference held by the render process, holds a
   `CacheStorageCacheHandle`.
4. For each CacheStorage reference held by the renderer process, holds a
   `CacheStorageHandle`.  This is used to inform the CacheStorage about
   whether its externally used so it can keep warmed cache objects alive
   to mitigate rapid opening/closing/opening churn.

### CacheStorageManager
1. Forwards calls to the appropriate `CacheStorage` for a given origin-owner
   pair, loading `CacheStorage`s on demand.
2. Handles `QuotaManager` and `BrowsingData` calls.

### CacheStorage
1. Manages the caches for a single origin-owner pair.
2. Handles creation/deletion of caches and updates the index on disk
   accordingly.
3. Manages operations that span multiple caches (e.g., `CacheStorage::Match`).
4. Backend-specific information is handled by `CacheStorage::CacheLoader`

### CacheStorageCache
1. Creates or opens a net::disk_cache (either `SimpleCache` or `MemoryCache`)
   on initialization.
2. Handles add/put/delete/match/keys calls.
3. Owned by `CacheStorage` and deleted either when `CacheStorage` deletes or
   when the last `CacheStorageCacheHandle` for the cache is gone.

### CacheStorageIndex
1. Manages an ordered collection of metadata
   (CacheStorageIndex::CacheStorageMetadata) for each CacheStorageCache owned
   by a given CacheStorage instance.
2. Is serialized by CacheStorage::CacheLoader (WriteIndex/LoadIndex) as a
   Protobuf file.

### CacheStorageCacheHandle
1. Holds a weak reference to a `CacheStorageCache`.
2. When the last `CacheStorageCacheHandle` to a `CacheStorageCache` is
   deleted, so to is the `CacheStorageCache`.
3. The `CacheStorageCache` may be deleted before the `CacheStorageCacheHandle`
   (on `CacheStorage` destruction), so it must be checked for validity before
   use.

### CacheStorageHandle
1. Holds a weak reference to a `CacheStorage`.
2. When the last `CacheStorageHandle` to a `CacheStorage` is
   deleted, internal state is cleaned up.  The `CacheStorage` object is not
   deleted, however.
3. The `CacheStorage` may be deleted before the `CacheStorageHandle`
   (on browser shutdown), so it must be checked for validity before use.

## Directory Structure
$PROFILE/Service Worker/CacheStorage/`origin`/`cache`/

Where `origin` is a hash of the origin and `cache` is a GUID generated at the
cache's creation time.

The reason a random directory is used for a cache is so that a cache can be
doomed and still used by old references while another cache with the same name
is created.

### Directory Contents
`CacheStorage` creates its own index file (index.txt), which contains a
mapping of cache names to its path on disk. On `CacheStorage` initialization,
directories not in the index are deleted.

Each `CacheStorageCache` has a `disk_cache::Backend` backend, which writes in
the `CacheStorageCache`'s directory.

## Layout of the disk_cache::Backend
A cache is represented by a `disk_cache::Backend`. The Request/Response pairs
referred to in the specification are stored as `disk_cache::Entry`s.  Each
`disk_cache::Entry` has three streams: one for storing a protobuf with the
request/response metadata (e.g., the headers, the request URL, and opacity
information), another for storing the response body, and a final stream for
storing any additional data (e.g., compiled JavaScript).

The entries are keyed by full URL. This has a few ramifications:
 1. Multiple vary responses for a single request URL are not supported.
 2. Operations that may require scanning multiple URLs (e.g., `ignoreSearch`)
    must scan every entry in the cache.

*The above could be fixed by changes to the backend or by introducing indirect
entries in the cache. The indirect entries would be for the query-stripped
request URL. It would point to entries to each query request/response pair and
for each vary request/response pair.*

## Threads
* CacheStorage classes live on the IO thread. Exceptions include:
  * `CacheStorageContextImpl` which is created on UI but otherwise runs and is
   deleted on IO.
  * `CacheStorageDispatcherHost` which is created on UI but otherwise runs and
    is deleted on IO.
* Index file manipulation and directory creation/deletion occurs on a
  `SequencedTaskRunner` assigned at `CacheStorageContextImpl` creation.
* The `disk_cache::Backend` lives on the IO thread and uses its own worker
  pool to implement async operations.

## Asynchronous Idioms in CacheStorage and CacheStorageCache
1. All async methods should asynchronously run their callbacks.
2. The async methods often include several asynchronous steps. Each step
   passes a continuation callback on to the next. The continuation includes
   all of the necessary state for the operation.
3. Callbacks are guaranteed to run so long as the object
   (`CacheStorageCacheCache` or `CacheStorage`) is still alive. Once the
   object is deleted, the callbacks are dropped. We don't worry about dropped
   callbacks on shutdown. If deleting prior to shutdown, one should `Close()`
   a `CacheStorage` or `CacheStorageCache` to ensure that all operations have
   completed before deleting it.

### Scheduling Operations
Operations are scheduled in a sequential scheduler (`CacheStorageScheduler`).
Each `CacheStorage` and `CacheStorageCache` has its own scheduler. If an
operation freezes, then the scheduler is frozen. If a `CacheStorage` call winds
up calling something from every `CacheStorageCache` (e.g.,
`CacheStorage::Match`), then one frozen `CacheStorageCache` can freeze the
`CacheStorage` as well. This has happened in the past (`Cache::Put` called
`QuotaManager` to determine how much room was available, which in turn called
`Cache::Size`). Be careful to avoid situations in which one operation triggers
a dependency on another operation from the same scheduler.

At the end of an operation, the scheduler needs to be kicked to start the next
operation. The idiom for this in CacheStorage/ is to wrap the operation's
callback with a function that will run the callback as well as advance the
scheduler. So long as the operation runs its wrapped callback the scheduler
will advance.

## Opaque Resource Size Obfuscation
Applications can cache cross-origin resources as per
[Cross-Origin Resources and CORS](https://www.w3.org/TR/service-workers-1/#cross-origin-resources).
Opaque responses are also cached, but in order to prevent "leaking" the size
of opaque responses their sizes are obfuscated. Random padding is added to the
actual size making it difficult for an attacker to ascertain the actual resource
size via quota APIs.

When Chromium starts, a new random padding key is generated and used
for all new caches created. This key is used by each cache to calculate padding
for opaque resources. Each cache's key is persisted to disk in the cache index file

Each cache maintains the total padding for all opaque resources within the
cache. This padding is added to the actual resource size when reporting sizes
to the quota manager.

The padding algorithm version is also written to each cache allowing for it
to be changed at a future date. CacheStorage will use the persisted key and
padding from the cache's index unless the padding algorithm has been changed,
one of values is missing, or deemed to be incorrect. In this situation the cache
is enumerated and the padding recalculated during open.
