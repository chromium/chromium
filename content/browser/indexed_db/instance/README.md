The root node of the class hierarchy for the code in this directory is
`BucketContext`. Start off by reading that class's documentation.

Each `BucketContext` and its owned objects are associated with a single
IndexedDB instance, such as that accessed via `navigator.indexedDB` or the
`indexedDB` for a [storage bucket](https://developer.chrome.com/docs/web-platform/storage-buckets).

Code in this directory mostly runs on the `SequencedTaskRunner` that is created
by the `IndexedDBContextImpl` for the `BucketContext` when the former
spawns the latter. This is sometimes referred to as the "bucket thread".

The only entry point into this directory that external code should access is
the `BucketContext`, but classes in this directory can use some thread-agnostic
code such as ../file_path_util.h.
