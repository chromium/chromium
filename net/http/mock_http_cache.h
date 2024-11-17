// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a mock of the http cache and related testing classes. To be fair, it
// is not really a mock http cache given that it uses the real implementation of
// the http cache, but it has fake implementations of all required components,
// so it is useful for unit tests at the http layer.

#ifndef NET_HTTP_MOCK_HTTP_CACHE_H_
#define NET_HTTP_MOCK_HTTP_CACHE_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/strings/string_split.h"
#include "net/base/completion_once_callback.h"
#include "net/base/request_priority.h"
#include "net/disk_cache/disk_cache.h"
#include "net/http/http_cache.h"
#include "net/http/http_transaction_test_util.h"

namespace net {

//-----------------------------------------------------------------------------
// Mock disk cache (a very basic memory cache implementation).

class MockDiskEntry : public disk_cache::Entry,
                      public base::RefCounted<MockDiskEntry> {
 public:
  enum DeferOp {
    DEFER_NONE,
    DEFER_CREATE,
    DEFER_READ,
    DEFER_WRITE,
  };

  // Bit mask used for set_fail_requests().
  enum FailOp {
    FAIL_READ = 0x01,
    FAIL_WRITE = 0x02,
    FAIL_READ_SPARSE = 0x04,
    FAIL_WRITE_SPARSE = 0x08,
    FAIL_GET_AVAILABLE_RANGE = 0x10,
    FAIL_ALL = 0xFF
  };

  explicit MockDiskEntry(const std::string& key);

  bool is_doomed() const { return doomed_; }

  void Doom() override;
  void Close() override;
  std::string GetKey() const override;
  base::Time GetLastUsed() const override;
  base::Time GetLastModified() const override;
  int32_t GetDataSize(int index) const override;
  int ReadData(int index,
               int offset,
               IOBuffer* buf,
               int buf_len,
               CompletionOnceCallback callback) override;
  int WriteData(int index,
                int offset,
                IOBuffer* buf,
                int buf_len,
                CompletionOnceCallback callback,
                bool truncate) override;
  int ReadSparseData(int64_t offset,
                     IOBuffer* buf,
                     int buf_len,
                     CompletionOnceCallback callback) override;
  int WriteSparseData(int64_t offset,
                      IOBuffer* buf,
                      int buf_len,
                      CompletionOnceCallback callback) override;
  RangeResult GetAvailableRange(int64_t offset,
                                int len,
                                RangeResultCallback callback) override;
  bool CouldBeSparse() const override;
  void CancelSparseIO() override;
  Error ReadyForSparseIO(CompletionOnceCallback completion_callback) override;
  void SetLastUsedTimeForTest(base::Time time) override;

  uint8_t in_memory_data() const { return in_memory_data_; }
  void set_in_memory_data(uint8_t val) { in_memory_data_ = val; }

  // Fail subsequent requests, specified via FailOp bits.
  void set_fail_requests(int mask) { fail_requests_ = mask; }

  void set_fail_sparse_requests() { fail_sparse_requests_ = true; }

  // If |value| is true, don't deliver any completion callbacks until called
  // again with |value| set to false.  Caution: remember to enable callbacks
  // again or all subsequent tests will fail.
  static void IgnoreCallbacks(bool value);

  // Defers invoking the callback for the given operation. Calling code should
  // invoke ResumeDiskEntryOperation to resume.
  void SetDefer(DeferOp defer_op) { defer_op_ = defer_op; }

  // Resumes deferred cache operation by posting |resume_callback_| with
  // |resume_return_code_|.
  void ResumeDiskEntryOperation();

  // Sets the maximum length of a stream. This is only applied to stream 1.
  void set_max_file_size(int val) { max_file_size_ = val; }

 private:
  friend class base::RefCounted<MockDiskEntry>;
  struct CallbackInfo;

  ~MockDiskEntry() override;

  // Unlike the callbacks for MockHttpTransaction, we want this one to run even
  // if the consumer called Close on the MockDiskEntry.  We achieve that by
  // leveraging the fact that this class is reference counted.
  void CallbackLater(CompletionOnceCallback callback, int result);
  void CallbackLater(base::OnceClosure callback);

  void RunCallback(base::OnceClosure callback);

  // When |store| is true, stores the callback to be delivered later; otherwise
  // delivers any callback previously stored.
  static void StoreAndDeliverCallbacks(bool store,
                                       MockDiskEntry* entry,
                                       base::OnceClosure callback);

  static const int kNumCacheEntryDataIndices = 3;

  std::string key_;
  std::vector<char> data_[kNumCacheEntryDataIndices];
  uint8_t in_memory_data_ = 0;
  int test_mode_;
  int max_file_size_;
  bool doomed_ = false;
  bool sparse_ = false;
  int fail_requests_ = 0;
  bool fail_sparse_requests_ = false;
  bool busy_ = false;
  bool delayed_ = false;
  bool cancel_ = false;

  // Used for pause and restart.
  DeferOp defer_op_ = DEFER_NONE;
  CompletionOnceCallback resume_callback_;
  int resume_return_code_ = 0;

  static bool ignore_callbacks_;
};

class MockDiskCache : public disk_cache::Backend {
 public:
  MockDiskCache();
  ~MockDiskCache() override;

  int32_t GetEntryCount() const override;
  EntryResult OpenOrCreateEntry(const std::string& key,
                                RequestPriority request_priority,
                                EntryResultCallback callback) override;
  EntryResult OpenEntry(const std::string& key,
                        RequestPriority request_priority,
                        EntryResultCallback callback) override;
  EntryResult CreateEntry(const std::string& key,
                          RequestPriority request_priority,
                          EntryResultCallback callback) override;
  Error DoomEntry(const std::string& key,
                  RequestPriority request_priority,
                  CompletionOnceCallback callback) override;
  Error DoomAllEntries(CompletionOnceCallback callback) override;
  Error DoomEntriesBetween(base::Time initial_time,
                           base::Time end_time,
                           CompletionOnceCallback callback) override;
  Error DoomEntriesSince(base::Time initial_time,
                         CompletionOnceCallback callback) override;
  int64_t CalculateSizeOfAllEntries(
      Int64CompletionOnceCallback callback) override;
  std::unique_ptr<Iterator> CreateIterator() override;
  void GetStats(base::StringPairs* stats) override;
  void OnExternalCacheHit(const std::string& key) override;
  uint8_t GetEntryInMemoryData(const std::string& key) override;
  void SetEntryInMemoryData(const std::string& key, uint8_t data) override;
  int64_t MaxFileSize() const override;

  // Returns number of times a cache entry was successfully opened.
  int open_count() const { return open_count_; }

  // Returns number of times a cache entry was successfully created.
  int create_count() const { return create_count_; }

  // Returns number of doomed entries.
  int doomed_count() const { return doomed_count_; }

  // Fail any subsequent CreateEntry, OpenEntry, and DoomEntry
  void set_fail_requests(bool value) { fail_requests_ = value; }

  // Return entries that fail some of their requests.
  // The value is formed as a bitmask of MockDiskEntry::FailOp.
  void set_soft_failures_mask(int value) { soft_failures_ = value; }

  // Returns entries that fail some of their requests, but only until
  // the entry is re-created. The value is formed as a bitmask of
  // MockDiskEntry::FailOp.
  void set_soft_failures_one_instance(int value) {
    soft_failures_one_instance_ = value;
  }

  // Makes sure that CreateEntry is not called twice for a given key.
  void set_double_create_check(bool value) { double_create_check_ = value; }

  // Determines whether to provide the GetEntryInMemoryData/SetEntryInMemoryData
  // interface.  Default is true.
  void set_support_in_memory_entry_data(bool value) {
    support_in_memory_entry_data_ = value;
  }

  // OpenEntry, CreateEntry, and DoomEntry immediately return with
  // ERR_IO_PENDING and will callback some time later with an error.
  void set_force_fail_callback_later(bool value) {
    force_fail_callback_later_ = value;
  }

  // Makes all requests for data ranges to fail as not implemented.
  void set_fail_sparse_requests() { fail_sparse_requests_ = true; }

  // Sets the limit on how big entry streams can get. Only stream 1 enforces
  // this, but MaxFileSize() will still report it.
  void set_max_file_size(int new_size) { max_file_size_ = new_size; }

  void ReleaseAll();

  // Returns true if a doomed entry exists with this key.
  bool IsDiskEntryDoomed(const std::string& key);

  // Defers invoking the callback for the given operation. Calling code should
  // invoke ResumeCacheOperation to resume.
  void SetDefer(MockDiskEntry::DeferOp defer_op) { defer_op_ = defer_op; }

  // Resume deferred cache operation by posting |resume_callback_| with
  // |resume_return_code_|.
  void ResumeCacheOperation();

  // Returns a reference to the disk entry with the given |key|.
  scoped_refptr<MockDiskEntry> GetDiskEntryRef(const std::string& key);

  // Returns a reference to the vector storing all keys for external cache hits.
  const std::vector<std::string>& GetExternalCacheHits() const;

 private:
  using EntryMap =
      std::map<std::string, raw_ptr<MockDiskEntry, CtnExperimental>>;
  class NotImplementedIterator;

  void CallbackLater(base::OnceClosure callback);

  EntryMap entries_;
  std::vector<std::string> external_cache_hits_;
  int open_count_ = 0;
  int create_count_ = 0;
  int doomed_count_ = 0;
  int max_file_size_;
  bool fail_requests_ = false;
  int soft_failures_ = 0;
  int soft_failures_one_instance_ = 0;
  bool double_create_check_ = true;
  bool fail_sparse_requests_ = false;
  bool support_in_memory_entry_data_ = true;
  bool force_fail_callback_later_ = false;

  // Used for pause and restart.
  MockDiskEntry::DeferOp defer_op_ = MockDiskEntry::DEFER_NONE;
  base::OnceClosure resume_callback_;
};

class MockBackendFactory : public HttpCache::BackendFactory {
 public:
  disk_cache::BackendResult CreateBackend(
      NetLog* net_log,
      disk_cache::BackendResultCallback callback) override;
};

class MockHttpCache {
 public:
  MockHttpCache();
  explicit MockHttpCache(
      std::unique_ptr<HttpCache::BackendFactory> disk_cache_factory);

  HttpCache* http_cache() { return &http_cache_; }

  MockNetworkLayer* network_layer() {
    return static_cast<MockNetworkLayer*>(http_cache_.network_layer());
  }
  disk_cache::Backend* backend();
  MockDiskCache* disk_cache();

  // Wrapper around http_cache()->CreateTransaction(DEFAULT_PRIORITY...)
  int CreateTransaction(std::unique_ptr<HttpTransaction>* trans);

  // Wrapper to simulate cache lock timeout for new transactions.
  void SimulateCacheLockTimeout();

  // Wrapper to simulate cache lock timeout for new transactions.
  void SimulateCacheLockTimeoutAfterHeaders();

  // Wrapper to fail request conditionalization for new transactions.
  void FailConditionalizations();

  // Helper function for reading response info from the disk cache.
  static bool ReadResponseInfo(disk_cache::Entry* disk_entry,
                               HttpResponseInfo* response_info,
                               bool* response_truncated);

  // Helper function for writing response info into the disk cache.
  static bool WriteResponseInfo(disk_cache::Entry* disk_entry,
                                const HttpResponseInfo* response_info,
                                bool skip_transient_headers,
                                bool response_truncated);

  // Helper function to synchronously open a backend entry.
  bool OpenBackendEntry(const std::string& key, disk_cache::Entry** entry);

  // Helper function to synchronously create a backend entry.
  bool CreateBackendEntry(const std::string& key,
                          disk_cache::Entry** entry,
                          NetLog* net_log);

  // Returns the test mode after considering the global override.
  static int GetTestMode(int test_mode);

  // Overrides the test mode for a given operation. Remember to reset it after
  // the test! (by setting test_mode to zero).
  static void SetTestMode(int test_mode);

  // Functions to test the state of ActiveEntry.
  bool IsWriterPresent(const std::string& key);
  bool IsHeadersTransactionPresent(const std::string& key);
  int GetCountReaders(const std::string& key);
  int GetCountAddToEntryQueue(const std::string& key);
  int GetCountDoneHeadersQueue(const std::string& key);
  int GetCountWriterTransactions(const std::string& key);

  base::WeakPtr<HttpCache> GetWeakPtr();

 private:
  HttpCache http_cache_;
};

// This version of the disk cache doesn't invoke CreateEntry callbacks.
class MockDiskCacheNoCB : public MockDiskCache {
  EntryResult CreateEntry(const std::string& key,
                          RequestPriority request_priority,
                          EntryResultCallback callback) override;
};

class MockBackendNoCbFactory : public HttpCache::BackendFactory {
 public:
  disk_cache::BackendResult CreateBackend(
      NetLog* net_log,
      disk_cache::BackendResultCallback callback) override;
};

// This backend factory allows us to control the backend instantiation.
class MockBlockingBackendFactory : public HttpCache::BackendFactory {
 public:
  MockBlockingBackendFactory();
  ~MockBlockingBackendFactory() override;

  disk_cache::BackendResult CreateBackend(
      NetLog* net_log,
      disk_cache::BackendResultCallback callback) override;

  // Completes the backend creation. Any blocked call will be notified via the
  // provided callback.
  void FinishCreation();

  void set_fail(bool fail) { fail_ = fail; }

  disk_cache::BackendResultCallback ReleaseCallback() {
    return std::move(callback_);
  }

 private:
  disk_cache::BackendResult MakeResult();

  disk_cache::BackendResultCallback callback_;
  bool block_ = true;
  bool fail_ = false;
};

struct GetBackendResultIsPendingHelper {
  bool operator()(const HttpCache::GetBackendResult& result) const {
    return result.first == net::ERR_IO_PENDING;
  }
};
using TestGetBackendCompletionCallbackBase =
    net::internal::TestCompletionCallbackTemplate<
        HttpCache::GetBackendResult,
        GetBackendResultIsPendingHelper>;

class TestGetBackendCompletionCallback
    : public TestGetBackendCompletionCallbackBase {
 public:
  TestGetBackendCompletionCallback() = default;

  HttpCache::GetBackendCallback callback() {
    return base::BindOnce(&TestGetBackendCompletionCallback::SetResult,
                          base::Unretained(this));
  }
};

}  // namespace net

#endif  // NET_HTTP_MOCK_HTTP_CACHE_H_
