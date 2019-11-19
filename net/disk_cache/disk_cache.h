// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines the public interface of the disk cache. For more details see
// http://dev.chromium.org/developers/design-documents/network-stack/disk-cache

#ifndef NET_DISK_CACHE_DISK_CACHE_H_
#define NET_DISK_CACHE_DISK_CACHE_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/strings/string_split.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "net/base/cache_type.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_errors.h"
#include "net/base/net_export.h"
#include "net/base/request_priority.h"

namespace base {
class FilePath;

namespace trace_event {
class ProcessMemoryDump;
}

namespace android {
class ApplicationStatusListener;
}  // namespace android

}  // namespace base

namespace net {
class IOBuffer;
class NetLog;
}

namespace disk_cache {

class Entry;
class Backend;
class EntryResult;
using EntryResultCallback = base::OnceCallback<void(EntryResult)>;

// How to handle resetting the back-end cache from the previous session.
// See CreateCacheBackend() for its usage.
enum class ResetHandling { kReset, kResetOnError, kNeverReset };

// Returns an instance of a Backend of the given |type|. |path| points to a
// folder where the cached data will be stored (if appropriate). This cache
// instance must be the only object that will be reading or writing files to
// that folder (if another one exists, and |type| is not net::DISK_CACHE this
// operation will not complete until the previous duplicate gets destroyed and
// finishes all I/O). The returned object should be deleted when not needed
// anymore.
//
// If |reset_handling| is set to kResetOnError and there is a problem with the
// cache initialization, the files will be deleted and a new set will be
// created. If it's set to kReset, this will happen even if there isn't a
// problem with cache initialization. Finally, if it's set to kNeverReset, the
// cache creation will fail if there is a problem with cache initialization.
//
// |max_bytes| is the maximum size the cache can grow to. If zero is passed in
// as |max_bytes|, the cache will determine the value to use. The returned
// pointer can be nullptr if a fatal error is found. The actual return value of
// the function is a net error code. If this function returns ERR_IO_PENDING,
// the |callback| will be invoked when a backend is available or a fatal error
// condition is reached.  The pointer to receive the |backend| must remain valid
// until the operation completes (the callback is notified).
NET_EXPORT net::Error CreateCacheBackend(net::CacheType type,
                                         net::BackendType backend_type,
                                         const base::FilePath& path,
                                         int64_t max_bytes,
                                         ResetHandling reset_handling,
                                         net::NetLog* net_log,
                                         std::unique_ptr<Backend>* backend,
                                         net::CompletionOnceCallback callback);

#if defined(OS_ANDROID)
// Similar to the function above, but takes an |app_status_listener| which is
// used to listen for when the Android application status changes, so we can
// flush the cache to disk when the app goes to the background.
NET_EXPORT net::Error CreateCacheBackend(
    net::CacheType type,
    net::BackendType backend_type,
    const base::FilePath& path,
    int64_t max_bytes,
    ResetHandling reset_handling,
    net::NetLog* net_log,
    std::unique_ptr<Backend>* backend,
    net::CompletionOnceCallback callback,
    base::android::ApplicationStatusListener* app_status_listener);
#endif

// Variant of the above that calls |post_cleanup_callback| once all the I/O
// that was in flight has completed post-destruction. |post_cleanup_callback|
// will get invoked even if the creation fails. The invocation will always be
// via the event loop, and never direct.
//
// This is currently unsupported for |type| == net::DISK_CACHE.
//
// Note that this will not wait for |post_cleanup_callback| of a previous
// instance for |path| to run.
NET_EXPORT net::Error CreateCacheBackend(
    net::CacheType type,
    net::BackendType backend_type,
    const base::FilePath& path,
    int64_t max_bytes,
    ResetHandling reset_handling,
    net::NetLog* net_log,
    std::unique_ptr<Backend>* backend,
    base::OnceClosure post_cleanup_callback,
    net::CompletionOnceCallback callback);

// This will flush any internal threads used by backends created w/o an
// externally injected thread specified, so tests can be sure that all I/O
// has finished before inspecting the world.
NET_EXPORT void FlushCacheThreadForTesting();

// The root interface for a disk cache instance.
class NET_EXPORT Backend {
 public:
  using CompletionOnceCallback = net::CompletionOnceCallback;
  using Int64CompletionOnceCallback = net::Int64CompletionOnceCallback;
  using EntryResultCallback = disk_cache::EntryResultCallback;
  using EntryResult = disk_cache::EntryResult;

  class Iterator {
   public:
    virtual ~Iterator() {}

    // OpenNextEntry returns a result with net_error() |net::OK| and provided
    // entry if there is an entry to enumerate which it can return immediately.
    // It returns a result with net_error() |net::ERR_FAILED| at the end of
    // enumeration. If the function returns a result with net_error()
    // |net::ERR_IO_PENDING|, then the final result will be passed to the
    // provided |callback|, otherwise |callback| will not be called. If any
    // entry in the cache is modified during iteration, the result of this
    // function is thereafter undefined.
    //
    // Calling OpenNextEntry after the backend which created it is destroyed
    // may fail with |net::ERR_FAILED|; however it should not crash.
    //
    // Some cache backends make stronger guarantees about mutation during
    // iteration, see top comment in simple_backend_impl.h for details.
    virtual EntryResult OpenNextEntry(EntryResultCallback callback) = 0;
  };

  // If the backend is destroyed when there are operations in progress (any
  // callback that has not been invoked yet), this method cancels said
  // operations so the callbacks are not invoked, possibly leaving the work
  // half way (for instance, dooming just a few entries). Note that pending IO
  // for a given Entry (as opposed to the Backend) will still generate a
  // callback.
  // Warning: there is some inconsistency in details between different backends
  // on what will succeed and what will fail.  In particular the blockfile
  // backend will leak entries closed after backend deletion, while others
  // handle it properly.
  Backend(net::CacheType cache_type) : cache_type_(cache_type) {}
  virtual ~Backend() {}

  // Returns the type of this cache.
  net::CacheType GetCacheType() const { return cache_type_; }

  // Returns the number of entries in the cache.
  virtual int32_t GetEntryCount() const = 0;

  // Atomically attempts to open an existing entry based on |key| or, if none
  // already exists, to create a new entry. Returns an EntryResult object,
  // which contains 1) network error code; 2) if the error code is OK,
  // an owning pointer to either a preexisting or a newly created
  // entry; 3) a bool indicating if the entry was opened or not. When the entry
  // pointer is no longer needed, its Close() method should be called. If this
  // method return value has net_error() == ERR_IO_PENDING, the
  // |callback| will be invoked when the entry is available. The |priority| of
  // the entry determines its priority in the background worker pools.
  //
  // This method should be the preferred way to obtain an entry over using
  // OpenEntry() or CreateEntry() separately in order to simplify consumer
  // logic.
  virtual EntryResult OpenOrCreateEntry(const std::string& key,
                                        net::RequestPriority priority,
                                        EntryResultCallback callback) = 0;

  // Opens an existing entry, returning status code, and, if successful, an
  // entry pointer packaged up into an EntryResult. If return value's
  // net_error() is ERR_IO_PENDING, the |callback| will be invoked when the
  // entry is available. The |priority| of the entry determines its priority in
  // the background worker pools.
  virtual EntryResult OpenEntry(const std::string& key,
                                net::RequestPriority priority,
                                EntryResultCallback) = 0;

  // Creates a new entry, returning status code, and, if successful, and
  // an entry pointer packaged up into an EntryResult. If return value's
  // net_error() is ERR_IO_PENDING, the |callback| will be invoked when the
  // entry is available. The |priority| of the entry determines its priority in
  // the background worker pools.
  virtual EntryResult CreateEntry(const std::string& key,
                                  net::RequestPriority priority,
                                  EntryResultCallback callback) = 0;

  // Marks the entry, specified by the given key, for deletion. The return value
  // is a net error code. If this method returns ERR_IO_PENDING, the |callback|
  // will be invoked after the entry is doomed.
  virtual net::Error DoomEntry(const std::string& key,
                               net::RequestPriority priority,
                               CompletionOnceCallback callback) = 0;

  // Marks all entries for deletion. The return value is a net error code. If
  // this method returns ERR_IO_PENDING, the |callback| will be invoked when the
  // operation completes.
  virtual net::Error DoomAllEntries(CompletionOnceCallback callback) = 0;

  // Marks a range of entries for deletion. This supports unbounded deletes in
  // either direction by using null Time values for either argument. The return
  // value is a net error code. If this method returns ERR_IO_PENDING, the
  // |callback| will be invoked when the operation completes.
  // Entries with |initial_time| <= access time < |end_time| are deleted.
  virtual net::Error DoomEntriesBetween(base::Time initial_time,
                                        base::Time end_time,
                                        CompletionOnceCallback callback) = 0;

  // Marks all entries accessed since |initial_time| for deletion. The return
  // value is a net error code. If this method returns ERR_IO_PENDING, the
  // |callback| will be invoked when the operation completes.
  // Entries with |initial_time| <= access time are deleted.
  virtual net::Error DoomEntriesSince(base::Time initial_time,
                                      CompletionOnceCallback callback) = 0;

  // Calculate the total size of the cache. The return value is the size in
  // bytes or a net error code. If this method returns ERR_IO_PENDING,
  // the |callback| will be invoked when the operation completes.
  virtual int64_t CalculateSizeOfAllEntries(
      Int64CompletionOnceCallback callback) = 0;

  // Calculate the size of all cache entries accessed between |initial_time| and
  // |end_time|.
  // The return value is the size in bytes or a net error code. The default
  // implementation returns ERR_NOT_IMPLEMENTED and should only be overwritten
  // if there is an efficient way for the backend to determine the size for a
  // subset of the cache without reading the whole cache from disk.
  // If this method returns ERR_IO_PENDING, the |callback| will be invoked when
  // the operation completes.
  virtual int64_t CalculateSizeOfEntriesBetween(
      base::Time initial_time,
      base::Time end_time,
      Int64CompletionOnceCallback callback);

  // Returns an iterator which will enumerate all entries of the cache in an
  // undefined order.
  virtual std::unique_ptr<Iterator> CreateIterator() = 0;

  // Return a list of cache statistics.
  virtual void GetStats(base::StringPairs* stats) = 0;

  // Called whenever an external cache in the system reuses the resource
  // referred to by |key|.
  virtual void OnExternalCacheHit(const std::string& key) = 0;

  // Returns the estimate of dynamically allocated memory in bytes.
  virtual size_t DumpMemoryStats(
      base::trace_event::ProcessMemoryDump* pmd,
      const std::string& parent_absolute_name) const = 0;

  // Backends can optionally permit one to store, probabilistically, up to a
  // byte associated with a key of an existing entry in memory.

  // GetEntryInMemoryData has the following behavior:
  // - If the data is not available at this time for any reason, returns 0.
  // - Otherwise, returns a value that was with very high probability
  //   given to SetEntryInMemoryData(|key|) (and with a very low probability
  //   to a different key that collides in the in-memory index).
  //
  // Due to the probability of collisions, including those that can be induced
  // by hostile 3rd parties, this interface should not be used to make decisions
  // that affect correctness (especially security).
  virtual uint8_t GetEntryInMemoryData(const std::string& key);
  virtual void SetEntryInMemoryData(const std::string& key, uint8_t data);

  // Returns the maximum length an individual stream can have.
  virtual int64_t MaxFileSize() const = 0;

 private:
  const net::CacheType cache_type_;
};

// This interface represents an entry in the disk cache.
class NET_EXPORT Entry {
 public:
  typedef net::CompletionOnceCallback CompletionOnceCallback;
  typedef net::IOBuffer IOBuffer;

  // Marks this cache entry for deletion.
  virtual void Doom() = 0;

  // Releases this entry. Calling this method does not cancel pending IO
  // operations on this entry. Even after the last reference to this object has
  // been released, pending completion callbacks may be invoked.
  virtual void Close() = 0;

  // Returns the key associated with this cache entry.
  virtual std::string GetKey() const = 0;

  // Returns the time when this cache entry was last used.
  virtual base::Time GetLastUsed() const = 0;

  // Returns the time when this cache entry was last modified.
  virtual base::Time GetLastModified() const = 0;

  // Returns the size of the cache data with the given index.
  virtual int32_t GetDataSize(int index) const = 0;

  // Copies cached data into the given buffer of length |buf_len|. Returns the
  // number of bytes read or a network error code. If this function returns
  // ERR_IO_PENDING, the completion callback will be called on the current
  // thread when the operation completes, and a reference to |buf| will be
  // retained until the callback is called. Note that as long as the function
  // does not complete immediately, the callback will always be invoked, even
  // after Close has been called; in other words, the caller may close this
  // entry without having to wait for all the callbacks, and still rely on the
  // cleanup performed from the callback code.
  virtual int ReadData(int index,
                       int offset,
                       IOBuffer* buf,
                       int buf_len,
                       CompletionOnceCallback callback) = 0;

  // Copies data from the given buffer of length |buf_len| into the cache.
  // Returns the number of bytes written or a network error code. If this
  // function returns ERR_IO_PENDING, the completion callback will be called
  // on the current thread when the operation completes, and a reference to
  // |buf| will be retained until the callback is called. Note that as long as
  // the function does not complete immediately, the callback will always be
  // invoked, even after Close has been called; in other words, the caller may
  // close this entry without having to wait for all the callbacks, and still
  // rely on the cleanup performed from the callback code.
  // If truncate is true, this call will truncate the stored data at the end of
  // what we are writing here.
  virtual int WriteData(int index,
                        int offset,
                        IOBuffer* buf,
                        int buf_len,
                        CompletionOnceCallback callback,
                        bool truncate) = 0;

  // Sparse entries support:
  //
  // A Backend implementation can support sparse entries, so the cache keeps
  // track of which parts of the entry have been written before. The backend
  // will never return data that was not written previously, so reading from
  // such region will return 0 bytes read (or actually the number of bytes read
  // before reaching that region).
  //
  // There are only two streams for sparse entries: a regular control stream
  // (index 0) that must be accessed through the regular API (ReadData and
  // WriteData), and one sparse stream that must me accessed through the sparse-
  // aware API that follows. Calling a non-sparse aware method with an index
  // argument other than 0 is a mistake that results in implementation specific
  // behavior. Using a sparse-aware method with an entry that was not stored
  // using the same API, or with a backend that doesn't support sparse entries
  // will return ERR_CACHE_OPERATION_NOT_SUPPORTED.
  //
  // The storage granularity of the implementation should be at least 1 KB. In
  // other words, storing less than 1 KB may result in an implementation
  // dropping the data completely, and writing at offsets not aligned with 1 KB,
  // or with lengths not a multiple of 1 KB may result in the first or last part
  // of the data being discarded. However, two consecutive writes should not
  // result in a hole in between the two parts as long as they are sequential
  // (the second one starts where the first one ended), and there is no other
  // write between them.
  //
  // The Backend implementation is free to evict any range from the cache at any
  // moment, so in practice, the previously stated granularity of 1 KB is not
  // as bad as it sounds.
  //
  // The sparse methods don't support multiple simultaneous IO operations to the
  // same physical entry, so in practice a single object should be instantiated
  // for a given key at any given time. Once an operation has been issued, the
  // caller should wait until it completes before starting another one. This
  // requirement includes the case when an entry is closed while some operation
  // is in progress and another object is instantiated; any IO operation will
  // fail while the previous operation is still in-flight. In order to deal with
  // this requirement, the caller could either wait until the operation
  // completes before closing the entry, or call CancelSparseIO() before closing
  // the entry, and call ReadyForSparseIO() on the new entry and wait for the
  // callback before issuing new operations.

  // Behaves like ReadData() except that this method is used to access sparse
  // entries.
  virtual int ReadSparseData(int64_t offset,
                             IOBuffer* buf,
                             int buf_len,
                             CompletionOnceCallback callback) = 0;

  // Behaves like WriteData() except that this method is used to access sparse
  // entries. |truncate| is not part of this interface because a sparse entry
  // is not expected to be reused with new data. To delete the old data and
  // start again, or to reduce the total size of the stream data (which implies
  // that the content has changed), the whole entry should be doomed and
  // re-created.
  virtual int WriteSparseData(int64_t offset,
                              IOBuffer* buf,
                              int buf_len,
                              CompletionOnceCallback callback) = 0;

  // Returns information about the currently stored portion of a sparse entry.
  // |offset| and |len| describe a particular range that should be scanned to
  // find out if it is stored or not. |start| will contain the offset of the
  // first byte that is stored within this range, and the return value is the
  // minimum number of consecutive stored bytes. Note that it is possible that
  // this entry has stored more than the returned value. This method returns a
  // net error code whenever the request cannot be completed successfully. If
  // this method returns ERR_IO_PENDING, the |callback| will be invoked when the
  // operation completes, and |start| must remain valid until that point.
  virtual int GetAvailableRange(int64_t offset,
                                int len,
                                int64_t* start,
                                CompletionOnceCallback callback) = 0;

  // Returns true if this entry could be a sparse entry or false otherwise. This
  // is a quick test that may return true even if the entry is not really
  // sparse. This method doesn't modify the state of this entry (it will not
  // create sparse tracking data). GetAvailableRange or ReadSparseData can be
  // used to perform a definitive test of whether an existing entry is sparse or
  // not, but that method may modify the current state of the entry (making it
  // sparse, for instance). The purpose of this method is to test an existing
  // entry, but without generating actual IO to perform a thorough check.
  virtual bool CouldBeSparse() const = 0;

  // Cancels any pending sparse IO operation (if any). The completion callback
  // of the operation in question will still be called when the operation
  // finishes, but the operation will finish sooner when this method is used.
  virtual void CancelSparseIO() = 0;

  // Returns OK if this entry can be used immediately. If that is not the
  // case, returns ERR_IO_PENDING and invokes the provided callback when this
  // entry is ready to use. This method always returns OK for non-sparse
  // entries, and returns ERR_IO_PENDING when a previous operation was cancelled
  // (by calling CancelSparseIO), but the cache is still busy with it. If there
  // is a pending operation that has not been cancelled, this method will return
  // OK although another IO operation cannot be issued at this time; in this
  // case the caller should just wait for the regular callback to be invoked
  // instead of using this method to provide another callback.
  //
  // Note that CancelSparseIO may have been called on another instance of this
  // object that refers to the same physical disk entry.
  // Note: This method is deprecated.
  virtual net::Error ReadyForSparseIO(CompletionOnceCallback callback) = 0;

  // Used in tests to set the last used time. Note that backend might have
  // limited precision. Also note that this call may modify the last modified
  // time.
  virtual void SetLastUsedTimeForTest(base::Time time) = 0;

 protected:
  virtual ~Entry() {}
};

struct EntryDeleter {
  void operator()(Entry* entry) {
    // Note that |entry| is ref-counted.
    entry->Close();
  }
};

// Automatically closes an entry when it goes out of scope.
// Warning: Be careful. Automatically closing may not be the desired behavior
// when writing to an entry. You may wish to doom first (e.g., in case writing
// hasn't yet completed but the browser is shutting down).
typedef std::unique_ptr<Entry, EntryDeleter> ScopedEntryPtr;

// Represents the result of an entry open or create operation.
// This is a move-only, owning type, which will close the entry it owns unless
// it's released from it via ReleaseEntry (or it's moved away from).
class NET_EXPORT EntryResult {
 public:
  EntryResult();
  ~EntryResult();
  EntryResult(EntryResult&&);
  EntryResult& operator=(EntryResult&&);

  EntryResult(const EntryResult&) = delete;
  EntryResult& operator=(const EntryResult&) = delete;

  // Creates an entry result representing successfully opened (pre-existing)
  // cache entry. |new_entry| must be non-null.
  static EntryResult MakeOpened(Entry* new_entry);

  // Creates an entry result representing successfully created (new)
  // cache entry. |new_entry| must be non-null.
  static EntryResult MakeCreated(Entry* new_entry);

  // Creates an entry result representing an error. Status must not be net::OK.
  static EntryResult MakeError(net::Error status);

  // Relinquishes ownership of the entry, and returns a pointer to it.
  // Will return nullptr if there is no such entry.
  // WARNING: clears net_error() to ERR_FAILED, opened() to false.
  Entry* ReleaseEntry();

  // ReleaseEntry() will return a non-null pointer if and only if this is
  // net::OK before the call to it.
  net::Error net_error() const { return net_error_; }

  // Returns true if an existing entry was opened rather than a new one created.
  // Implies net_error() == net::OK and non-null entry.
  bool opened() const { return opened_; }

 private:
  // Invariant to keep: |entry_| != nullptr iff |net_error_| == net::OK;
  // |opened_| set only if entry is set.
  net::Error net_error_ = net::ERR_FAILED;
  bool opened_ = false;
  ScopedEntryPtr entry_;
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_DISK_CACHE_H_
