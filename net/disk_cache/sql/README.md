# SQL disk cache backend

This directory contains an experimental SQL-based implementation of the disk
cache (`disk_cache::Backend`). Chromium's Disk Cache (defined in
`net/disk_cache/disk_cache.h`) provides four backend implementations
(blockfile, simple, memory, sql), each implementing `disk_cache::Backend` and
`disk_cache::Entry`.

For detailed design rationale, refer to the
[SQL Disk Cache Backend Design Doc](https://docs.google.com/document/d/1enmPPNr9aUY-_bBk5hNISsQEplk_Ud492eTkTF_r6u8/edit?usp=sharing).

It uses SQLite to store cache entries and is designed to be more robust and
performant than the default cache backends (block file backend on Windows and
simple cache backend on other OSes), especially in scenarios with a large
number of small entries.

The implementation is sharded to improve concurrency, with each shard managing
its own SQLite database file.

## Key Classes and Components

### Core Backend Logic

*   **`SqlBackendImpl`**: This is the main entry point and public-facing class
    for the SQL-based disk cache. It implements the `disk_cache::Backend`
    interface, handling requests from the network stack to open, create, doom,
    and enumerate cache entries. It owns and coordinates the
    `SqlPersistentStore`.

*   **`SqlEntryImpl`**: Implements the `disk_cache::Entry` interface,
    representing a single entry in the cache. It manages the data streams
    (header and body) and metadata (`last_used` time, key, etc.) for an entry.
    All I/O operations are passed to the `SqlBackendImpl`, which then delegates
    them to the persistence layer.

*   **`EntryDbHandle`**: A ref-counted object tracking the database resource ID
    (`ResId`) and state (`kInitial`, `kCreating`, `kCreated`, `kErrorOccurred`,
    `doomed`) of an entry. Enables speculative entry creation before database
    operations complete on background sequences.

### Persistence Layer

*   **`SqlPersistentStore`**: This class serves as the primary interface to the
    persistence layer, abstracting away the details of database sharding and
    asynchronous operations. It owns multiple `BackendShard` instances and
    distributes operations among them based on the cache entry's key.

*   **`SqlPersistentStore::BackendShard`**: Manages a single shard of the cache.
    Each shard has its own SQLite database file, a dedicated background task
    runner for database operations, and an in-memory index of its entries. It
    owns a `SequenceBound<SqlPersistentStore::Backend>`.

*   **`SqlPersistentStore::Backend`**: This class encapsulates all direct
    interactions with a single SQLite database. It runs entirely on a
    background sequence to avoid blocking the main network thread. Its
    responsibilities include:
    *   Executing all SQL queries (CREATE, READ, UPDATE, DELETE).
    *   Managing the database schema and transactions.
    *   Handling database initialization and error recovery.

### Data Structures and Utilities

*   **`CacheEntryKey`**: A memory-efficient wrapper for the cache key string.
    Since cache keys can be long and are stored in multiple in-memory data
    structures, this class uses a `scoped_refptr<base::RefCountedString>` to
    share the underlying string data, reducing memory overhead.

*   **`SqlPersistentStoreInMemoryIndex`**: An in-memory index that maps a hash
    of a `CacheEntryKey` to its `ResId` (the primary key in the database). This
    allows for fast, synchronous checks to see if an entry is likely to exist in
    the cache without needing to perform a slow, asynchronous database query. It
    is highly optimized for memory usage.

*   **`IndexedPairSet`**: A bidirectional container mapping primary key hashes
    to resource IDs and entry metadata.

*   **`ExclusiveOperationCoordinator`**: A synchronization primitive that
    serializes access to resources. It ensures that "exclusive" operations (like
    cache-wide eviction or cleanup) do not run concurrently with "normal"
    operation (like reading or writing a single entry). Normal operations on
    *different* cache keys can run in parallel.

*   **`EvictionCandidateAggregator`**: A thread-safe helper class used during
    cache eviction. Each shard independently generates a list of its least
    recently used entries as eviction candidates. This class aggregates these
    lists from all shards, performs a final sort, and selects the global set of
    entries to be evicted to bring the cache size back under its limit.

*   **`InFlightEntryModification`**: A mechanism to queue metadata updates
    (`last_used` time, headers, body size) for a cache entry that is not
    currently active (i.e., not held open as a `SqlEntryImpl` object). When an
    operation modifies an entry, it records the change as an in-flight
    modification. If the entry is opened again before the background database
    write completes, these queued modifications are applied to the entry's data
    as it is read from disk. This ensures that the in-memory representation of
    an entry is always consistent with pending operations, even with fully
    asynchronous database writes.

### Memory Management & Buffering

*   **`EntryWriteBuffer`**: Buffers sequential body writes before flushing to
    disk.

*   **`SqlWriteBufferMemoryMonitor`**: Limits aggregate memory used by write
    buffers across open entries.

*   **`SqlReadCacheMemoryMonitor` & `MonitoredVectorIOBuffer`**: Limits
    memory used by read-ahead cache buffers.

### Asynchronous Task Tracking & Helpers

*   **`SqlAsyncTaskManager` & `SqlAsyncTaskToken`**: Tracks pending background
    tasks for unit test synchronization.

*   **`SqlTrackedSequenceBound`**: Wraps `base::SequenceBound` to automatically
    track background tasks via `SqlAsyncTaskManager`.

### Shared Cache & Renderer-Accessible HTTP Cache

*   **`SqlSharedCacheManager`**: Top-level manager for isolated shared cache
    instances.

*   **`SqlSharedCache`**: Represents an isolated shared cache for a specific
    Network Isolation Key (NIK).

*   **`SqlSharedCacheHandle`**: Ref-counted handle keeping a `SqlSharedCache`
    alive.

*   **`SqlSharedCacheIndexDatabase`**: Manages `shared_index` DB mapping NIK
    strings to database IDs.

*   **`SqlSharedCacheIsolatedDatabase`**: Manages NIK-isolated SQLite databases
    (`shared_<db_id>`) using `SandboxedVfs`.

*   **`SqlSharedCacheIsolatedDatabaseReader`**: Renderer-side client reading
    cached responses directly from isolated databases without IPC.

### How It Works

1.  **Initialization**: `SqlBackendImpl` creates a `SqlPersistentStore`, which
    in turn creates a number of `BackendShard` instances (e.g., 3), each with
    its own background task runner. Each shard initializes its SQLite database.

2.  **Entry Operations (Create/Open)**:
    *   A request to open or create an entry arrives at `SqlBackendImpl`.
    *   The entry's key is hashed to determine which `BackendShard` is
        responsible for it.
    *   The operation is posted to the shard's background task runner.
    *   The `SqlPersistentStore::Backend` for that shard executes the necessary
        SQL commands to find or create the entry in its database.
    *   The result (a `SqlEntryImpl` or an error) is returned to the main thread
        via a callback.

3.  **Data I/O**: Reading and writing data to a `SqlEntryImpl` follows a similar
    pattern, with operations being posted to the appropriate shard's background
    task runner.

4.  **Eviction**:
    *   When the cache size exceeds a certain threshold, `SqlBackendImpl`
        initiates eviction.
    *   It posts an exclusive "start eviction" task to the `SqlPersistentStore`.
    *   Each shard queries its database for a list of its least recently used
        entries.
    *   The `EvictionCandidateAggregator` collects these lists, selects the
        entries to be removed, and sends the list of doomed entries back to each
        shard to be deleted from the database.

5.  **Coordination**: The `ExclusiveOperationCoordinator` ensures that
    operations like eviction, which affect the entire cache, do not conflict
    with ongoing reads and writes to individual entries. When an exclusive
    operation is requested, the coordinator waits for all active normal
    operations to complete, runs the exclusive operation, and then resumes
    queued normal operations.

## Database Schema

### `SqlPersistentStore::BackendShard` Databases (`sqldb0`, `sqldb1`, ...)

#### Tables

*   **`resources`**: Stores the main metadata for each cache entry.
    *   `res_id` (INTEGER, PRIMARY KEY AUTOINCREMENT): Unique ID for the
        resource.
    *   `last_used` (INTEGER): Timestamp for LRU.
    *   `body_end` (INTEGER): End offset of the body.
    *   `bytes_usage` (INTEGER): Total bytes consumed by the entry.
    *   `doomed` (INTEGER): Flag for entries pending deletion (0 for live, 1 for
        doomed).
    *   `check_sum` (INTEGER): The checksum `crc32(head + cache_key_hash)`.
    *   `cache_key_hash` (INTEGER): The hash of `cache_key`.
    *   `cache_key` (TEXT): The full cache key string.
    *   `head` (BLOB): Serialized response headers.
    *   `hints` (INTEGER): Bitmask for in-memory entry data hints.

*   **`blobs`**: Stores the data chunks of the cached body.
    *   `blob_id` (INTEGER, PRIMARY KEY AUTOINCREMENT): Unique ID for the blob.
    *   `res_id` (INTEGER): Foreign key to `resources.res_id`.
    *   `start` (INTEGER): Start offset of this blob chunk.
    *   `end` (INTEGER): End offset of this blob chunk.
    *   `check_sum` (INTEGER): The checksum `crc32(blob + cache_key_hash)`.
    *   `blob` (BLOB): The actual data chunk.

### `SqlSharedCacheIndexDatabase` (`shared_index`)

*   **`storages`**: Maps Network Isolation Keys to database IDs.
    *   `db_id` (INTEGER, PRIMARY KEY AUTOINCREMENT): Unique database ID.
    *   `isolation_key` (TEXT): Network Isolation Key string.

### `SqlSharedCacheIsolatedDatabase` (`shared_<db_id>`)

*   **`resources`**: Stores cached resources per NIK.
    *   `hash` (INTEGER): 32-bit hash of resource URL.
    *   `url` (TEXT): Full resource URL string.
    *   `headers` (BLOB): Serialized HTTP response headers.
    *   `body` (BLOB): Body data payload.
    *   `is_ready` (INTEGER): Flag indicating if resource is fully written.
