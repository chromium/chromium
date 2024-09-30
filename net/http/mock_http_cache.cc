// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/http/mock_http_cache.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "net/base/features.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/disk_cache_test_util.h"
#include "net/http/http_cache_writers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

// During testing, we are going to limit the size of a cache entry to this many
// bytes using DCHECKs in order to prevent a test from causing unbounded memory
// growth. In practice cache entry shouldn't come anywhere near this limit for
// tests that use the mock cache. If they do, that's likely a problem with the
// test. If a test requires using massive cache entries, they should use a real
// cache backend instead.
const int kMaxMockCacheEntrySize = 100 * 1000 * 1000;

// We can override the test mode for a given operation by setting this global
// variable.
int g_test_mode = 0;

int GetTestModeForEntry(const std::string& key) {
  GURL url(HttpCache::GetResourceURLFromHttpCacheKey(key));
  const MockTransaction* t = FindMockTransaction(url);
  DCHECK(t);
  return t->test_mode;
}

}  // namespace

//-----------------------------------------------------------------------------

struct MockDiskEntry::CallbackInfo {
  scoped_refptr<MockDiskEntry> entry;
  base::OnceClosure callback;
};

MockDiskEntry::MockDiskEntry(const std::string& key)
    : key_(key), max_file_size_(std::numeric_limits<int>::max()) {
  test_mode_ = GetTestModeForEntry(key);
}

void MockDiskEntry::Doom() {
  doomed_ = true;
}

void MockDiskEntry::Close() {
  Release();
}

std::string MockDiskEntry::GetKey() const {
  return key_;
}

base::Time MockDiskEntry::GetLastUsed() const {
  return base::Time::Now();
}

base::Time MockDiskEntry::GetLastModified() const {
  return base::Time::Now();
}

int32_t MockDiskEntry::GetDataSize(int index) const {
  DCHECK(index >= 0 && index < kNumCacheEntryDataIndices);
  return static_cast<int32_t>(data_[index].size());
}

int MockDiskEntry::ReadData(int index,
                            int offset,
                            IOBuffer* buf,
                            int buf_len,
                            CompletionOnceCallback callback) {
  DCHECK(index >= 0 && index < kNumCacheEntryDataIndices);
  DCHECK(!callback.is_null());

  if (fail_requests_ & FAIL_READ) {
    return ERR_CACHE_READ_FAILURE;
  }

  if (offset < 0 || offset > static_cast<int>(data_[index].size())) {
    return ERR_FAILED;
  }
  if (static_cast<size_t>(offset) == data_[index].size()) {
    return 0;
  }

  int num = std::min(buf_len, static_cast<int>(data_[index].size()) - offset);
  memcpy(buf->data(), &data_[index][offset], num);

  if (MockHttpCache::GetTestMode(test_mode_) & TEST_MODE_SYNC_CACHE_READ) {
    return num;
  }

  // Pause and resume.
  if (defer_op_ == DEFER_READ) {
    defer_op_ = DEFER_NONE;
    resume_callback_ = std::move(callback);
    resume_return_code_ = num;
    return ERR_IO_PENDING;
  }

  CallbackLater(std::move(callback), num);
  return ERR_IO_PENDING;
}

void MockDiskEntry::ResumeDiskEntryOperation() {
  DCHECK(!resume_callback_.is_null());
  CallbackLater(std::move(resume_callback_), resume_return_code_);
  resume_return_code_ = 0;
}

int MockDiskEntry::WriteData(int index,
                             int offset,
                             IOBuffer* buf,
                             int buf_len,
                             CompletionOnceCallback callback,
                             bool truncate) {
  DCHECK(index >= 0 && index < kNumCacheEntryDataIndices);
  DCHECK(!callback.is_null());
  DCHECK(truncate);

  if (fail_requests_ & FAIL_WRITE) {
    CallbackLater(std::move(callback), ERR_CACHE_READ_FAILURE);
    return ERR_IO_PENDING;
  }

  if (offset < 0 || offset > static_cast<int>(data_[index].size())) {
    return ERR_FAILED;
  }

  DCHECK_LT(offset + buf_len, kMaxMockCacheEntrySize);
  if (offset + buf_len > max_file_size_ && index == 1) {
    return ERR_FAILED;
  }

  data_[index].resize(offset + buf_len);
  if (buf_len) {
    memcpy(&data_[index][offset], buf->data(), buf_len);
  }

  if (MockHttpCache::GetTestMode(test_mode_) & TEST_MODE_SYNC_CACHE_WRITE) {
    return buf_len;
  }

  if (defer_op_ == DEFER_WRITE) {
    defer_op_ = DEFER_NONE;
    resume_callback_ = std::move(callback);
    resume_return_code_ = buf_len;
    return ERR_IO_PENDING;
  }

  CallbackLater(std::move(callback), buf_len);
  return ERR_IO_PENDING;
}

int MockDiskEntry::ReadSparseData(int64_t offset,
                                  IOBuffer* buf,
                                  int buf_len,
                                  CompletionOnceCallback callback) {
  DCHECK(!callback.is_null());
  if (fail_sparse_requests_) {
    return ERR_NOT_IMPLEMENTED;
  }
  if (!sparse_ || busy_ || cancel_) {
    return ERR_CACHE_OPERATION_NOT_SUPPORTED;
  }
  if (offset < 0) {
    return ERR_FAILED;
  }

  if (fail_requests_ & FAIL_READ_SPARSE) {
    return ERR_CACHE_READ_FAILURE;
  }

  DCHECK(offset < std::numeric_limits<int32_t>::max());
  int real_offset = static_cast<int>(offset);
  if (!buf_len) {
    return 0;
  }

  int num = std::min(static_cast<int>(data_[1].size()) - real_offset, buf_len);
  memcpy(buf->data(), &data_[1][real_offset], num);

  if (MockHttpCache::GetTestMode(test_mode_) & TEST_MODE_SYNC_CACHE_READ) {
    return num;
  }

  CallbackLater(std::move(callback), num);
  busy_ = true;
  delayed_ = false;
  return ERR_IO_PENDING;
}

int MockDiskEntry::WriteSparseData(int64_t offset,
                                   IOBuffer* buf,
                                   int buf_len,
                                   CompletionOnceCallback callback) {
  DCHECK(!callback.is_null());
  if (fail_sparse_requests_) {
    return ERR_NOT_IMPLEMENTED;
  }
  if (busy_ || cancel_) {
    return ERR_CACHE_OPERATION_NOT_SUPPORTED;
  }
  if (!sparse_) {
    if (data_[1].size()) {
      return ERR_CACHE_OPERATION_NOT_SUPPORTED;
    }
    sparse_ = true;
  }
  if (offset < 0) {
    return ERR_FAILED;
  }
  if (!buf_len) {
    return 0;
  }

  if (fail_requests_ & FAIL_WRITE_SPARSE) {
    return ERR_CACHE_READ_FAILURE;
  }

  DCHECK(offset < std::numeric_limits<int32_t>::max());
  int real_offset = static_cast<int>(offset);

  if (static_cast<int>(data_[1].size()) < real_offset + buf_len) {
    DCHECK_LT(real_offset + buf_len, kMaxMockCacheEntrySize);
    data_[1].resize(real_offset + buf_len);
  }

  memcpy(&data_[1][real_offset], buf->data(), buf_len);
  if (MockHttpCache::GetTestMode(test_mode_) & TEST_MODE_SYNC_CACHE_WRITE) {
    return buf_len;
  }

  CallbackLater(std::move(callback), buf_len);
  return ERR_IO_PENDING;
}

disk_cache::RangeResult MockDiskEntry::GetAvailableRange(
    int64_t offset,
    int len,
    RangeResultCallback callback) {
  DCHECK(!callback.is_null());
  if (!sparse_ || busy_ || cancel_) {
    return RangeResult(ERR_CACHE_OPERATION_NOT_SUPPORTED);
  }
  if (offset < 0) {
    return RangeResult(ERR_FAILED);
  }

  if (fail_requests_ & FAIL_GET_AVAILABLE_RANGE) {
    return RangeResult(ERR_CACHE_READ_FAILURE);
  }

  RangeResult result;
  result.net_error = OK;
  result.start = offset;
  result.available_len = 0;
  DCHECK(offset < std::numeric_limits<int32_t>::max());
  int real_offset = static_cast<int>(offset);
  if (static_cast<int>(data_[1].size()) < real_offset) {
    return result;
  }

  int num = std::min(static_cast<int>(data_[1].size()) - real_offset, len);
  for (; num > 0; num--, real_offset++) {
    if (!result.available_len) {
      if (data_[1][real_offset]) {
        result.available_len++;
        result.start = real_offset;
      }
    } else {
      if (!data_[1][real_offset]) {
        break;
      }
      result.available_len++;
    }
  }
  if (MockHttpCache::GetTestMode(test_mode_) & TEST_MODE_SYNC_CACHE_WRITE) {
    return result;
  }

  CallbackLater(base::BindOnce(std::move(callback), result));
  return RangeResult(ERR_IO_PENDING);
}

bool MockDiskEntry::CouldBeSparse() const {
  if (fail_sparse_requests_) {
    return false;
  }
  return sparse_;
}

void MockDiskEntry::CancelSparseIO() {
  cancel_ = true;
}

Error MockDiskEntry::ReadyForSparseIO(CompletionOnceCallback callback) {
  if (fail_sparse_requests_) {
    return ERR_NOT_IMPLEMENTED;
  }
  if (!cancel_) {
    return OK;
  }

  cancel_ = false;
  DCHECK(!callback.is_null());
  if (MockHttpCache::GetTestMode(test_mode_) & TEST_MODE_SYNC_CACHE_READ) {
    return OK;
  }

  // The pending operation is already in the message loop (and hopefully
  // already in the second pass).  Just notify the caller that it finished.
  CallbackLater(std::move(callback), 0);
  return ERR_IO_PENDING;
}

void MockDiskEntry::SetLastUsedTimeForTest(base::Time time) {
  NOTREACHED_IN_MIGRATION();
}

// If |value| is true, don't deliver any completion callbacks until called
// again with |value| set to false.  Caution: remember to enable callbacks
// again or all subsequent tests will fail.
// Static.
void MockDiskEntry::IgnoreCallbacks(bool value) {
  if (ignore_callbacks_ == value) {
    return;
  }
  ignore_callbacks_ = value;
  if (!value) {
    StoreAndDeliverCallbacks(false, nullptr, base::OnceClosure());
  }
}

MockDiskEntry::~MockDiskEntry() = default;

// Unlike the callbacks for MockHttpTransaction, we want this one to run even
// if the consumer called Close on the MockDiskEntry.  We achieve that by
// leveraging the fact that this class is reference counted.
void MockDiskEntry::CallbackLater(base::OnceClosure callback) {
  if (ignore_callbacks_) {
    return StoreAndDeliverCallbacks(true, this, std::move(callback));
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&MockDiskEntry::RunCallback, this, std::move(callback)));
}

void MockDiskEntry::CallbackLater(CompletionOnceCallback callback, int result) {
  CallbackLater(base::BindOnce(std::move(callback), result));
}

void MockDiskEntry::RunCallback(base::OnceClosure callback) {
  if (busy_) {
    // This is kind of hacky, but controlling the behavior of just this entry
    // from a test is sort of complicated.  What we really want to do is
    // delay the delivery of a sparse IO operation a little more so that the
    // request start operation (async) will finish without seeing the end of
    // this operation (already posted to the message loop)... and without
    // just delaying for n mS (which may cause trouble with slow bots).  So
    // we re-post this operation (all async sparse IO operations will take two
    // trips through the message loop instead of one).
    if (!delayed_) {
      delayed_ = true;
      return CallbackLater(std::move(callback));
    }
  }
  busy_ = false;
  std::move(callback).Run();
}

// When |store| is true, stores the callback to be delivered later; otherwise
// delivers any callback previously stored.
// Static.
void MockDiskEntry::StoreAndDeliverCallbacks(bool store,
                                             MockDiskEntry* entry,
                                             base::OnceClosure callback) {
  static std::vector<CallbackInfo> callback_list;
  if (store) {
    CallbackInfo c = {entry, std::move(callback)};
    callback_list.push_back(std::move(c));
  } else {
    for (auto& callback_info : callback_list) {
      callback_info.entry->CallbackLater(std::move(callback_info.callback));
    }
    callback_list.clear();
  }
}

// Statics.
bool MockDiskEntry::ignore_callbacks_ = false;

//-----------------------------------------------------------------------------

MockDiskCache::MockDiskCache()
    : Backend(DISK_CACHE), max_file_size_(std::numeric_limits<int>::max()) {}

MockDiskCache::~MockDiskCache() {
  ReleaseAll();
}

int32_t MockDiskCache::GetEntryCount() const {
  return static_cast<int32_t>(entries_.size());
}

disk_cache::EntryResult MockDiskCache::OpenOrCreateEntry(
    const std::string& key,
    RequestPriority request_priority,
    EntryResultCallback callback) {
  DCHECK(!callback.is_null());

  if (force_fail_callback_later_) {
    CallbackLater(base::BindOnce(
        std::move(callback),
        EntryResult::MakeError(ERR_CACHE_OPEN_OR_CREATE_FAILURE)));
    return EntryResult::MakeError(ERR_IO_PENDING);
  }

  if (fail_requests_) {
    return EntryResult::MakeError(ERR_CACHE_OPEN_OR_CREATE_FAILURE);
  }

  EntryResult result;

  // First try opening the entry.
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  result = OpenEntry(key, request_priority, std::move(split_callback.first));
  if (result.net_error() == OK || result.net_error() == ERR_IO_PENDING) {
    return result;
  }

  // Unable to open, try creating the entry.
  result = CreateEntry(key, request_priority, std::move(split_callback.second));
  if (result.net_error() == OK || result.net_error() == ERR_IO_PENDING) {
    return result;
  }

  return EntryResult::MakeError(ERR_CACHE_OPEN_OR_CREATE_FAILURE);
}

disk_cache::EntryResult MockDiskCache::OpenEntry(
    const std::string& key,
    RequestPriority request_priority,
    EntryResultCallback callback) {
  DCHECK(!callback.is_null());
  if (force_fail_callback_later_) {
    CallbackLater(base::BindOnce(
        std::move(callback), EntryResult::MakeError(ERR_CACHE_OPEN_FAILURE)));
    return EntryResult::MakeError(ERR_IO_PENDING);
  }

  if (fail_requests_) {
    return EntryResult::MakeError(ERR_CACHE_OPEN_FAILURE);
  }

  auto it = entries_.find(key);
  if (it == entries_.end()) {
    return EntryResult::MakeError(ERR_CACHE_OPEN_FAILURE);
  }

  if (it->second->is_doomed()) {
    it->second->Release();
    entries_.erase(it);
    return EntryResult::MakeError(ERR_CACHE_OPEN_FAILURE);
  }

  open_count_++;

  MockDiskEntry* entry = it->second;
  entry->AddRef();

  if (soft_failures_ || soft_failures_one_instance_) {
    entry->set_fail_requests(soft_failures_ | soft_failures_one_instance_);
    soft_failures_one_instance_ = 0;
  }

  entry->set_max_file_size(max_file_size_);

  EntryResult result = EntryResult::MakeOpened(entry);
  if (GetTestModeForEntry(key) & TEST_MODE_SYNC_CACHE_START) {
    return result;
  }

  CallbackLater(base::BindOnce(std::move(callback), std::move(result)));
  return EntryResult::MakeError(ERR_IO_PENDING);
}

disk_cache::EntryResult MockDiskCache::CreateEntry(
    const std::string& key,
    RequestPriority request_priority,
    EntryResultCallback callback) {
  DCHECK(!callback.is_null());
  if (force_fail_callback_later_) {
    CallbackLater(base::BindOnce(
        std::move(callback), EntryResult::MakeError(ERR_CACHE_CREATE_FAILURE)));
    return EntryResult::MakeError(ERR_IO_PENDING);
  }

  if (fail_requests_) {
    return EntryResult::MakeError(ERR_CACHE_CREATE_FAILURE);
  }

  auto it = entries_.find(key);
  if (it != entries_.end()) {
    if (!it->second->is_doomed()) {
      if (double_create_check_) {
        NOTREACHED_IN_MIGRATION();
      } else {
        return EntryResult::MakeError(ERR_CACHE_CREATE_FAILURE);
      }
    }
    it->second->Release();
    entries_.erase(it);
  }

  create_count_++;

  MockDiskEntry* new_entry = new MockDiskEntry(key);

  new_entry->AddRef();
  entries_[key] = new_entry;

  new_entry->AddRef();

  if (soft_failures_ || soft_failures_one_instance_) {
    new_entry->set_fail_requests(soft_failures_ | soft_failures_one_instance_);
    soft_failures_one_instance_ = 0;
  }

  if (fail_sparse_requests_) {
    new_entry->set_fail_sparse_requests();
  }

  new_entry->set_max_file_size(max_file_size_);

  EntryResult result = EntryResult::MakeCreated(new_entry);
  if (GetTestModeForEntry(key) & TEST_MODE_SYNC_CACHE_START) {
    return result;
  }

  // Pause and resume.
  if (defer_op_ == MockDiskEntry::DEFER_CREATE) {
    defer_op_ = MockDiskEntry::DEFER_NONE;
    resume_callback_ = base::BindOnce(std::move(callback), std::move(result));
    return EntryResult::MakeError(ERR_IO_PENDING);
  }

  CallbackLater(base::BindOnce(std::move(callback), std::move(result)));
  return EntryResult::MakeError(ERR_IO_PENDING);
}

Error MockDiskCache::DoomEntry(const std::string& key,
                               RequestPriority request_priority,
                               CompletionOnceCallback callback) {
  DCHECK(!callback.is_null());
  if (force_fail_callback_later_) {
    CallbackLater(base::BindOnce(std::move(callback), ERR_CACHE_DOOM_FAILURE));
    return ERR_IO_PENDING;
  }

  if (fail_requests_) {
    return ERR_CACHE_DOOM_FAILURE;
  }

  auto it = entries_.find(key);
  if (it != entries_.end()) {
    it->second->Release();
    entries_.erase(it);
    doomed_count_++;
  }

  if (GetTestModeForEntry(key) & TEST_MODE_SYNC_CACHE_START) {
    return OK;
  }

  CallbackLater(base::BindOnce(std::move(callback), OK));
  return ERR_IO_PENDING;
}

Error MockDiskCache::DoomAllEntries(CompletionOnceCallback callback) {
  return ERR_NOT_IMPLEMENTED;
}

Error MockDiskCache::DoomEntriesBetween(const base::Time initial_time,
                                        const base::Time end_time,
                                        CompletionOnceCallback callback) {
  return ERR_NOT_IMPLEMENTED;
}

Error MockDiskCache::DoomEntriesSince(const base::Time initial_time,
                                      CompletionOnceCallback callback) {
  return ERR_NOT_IMPLEMENTED;
}

int64_t MockDiskCache::CalculateSizeOfAllEntries(
    Int64CompletionOnceCallback callback) {
  return ERR_NOT_IMPLEMENTED;
}

class MockDiskCache::NotImplementedIterator : public Iterator {
 public:
  EntryResult OpenNextEntry(EntryResultCallback callback) override {
    return EntryResult::MakeError(ERR_NOT_IMPLEMENTED);
  }
};

std::unique_ptr<disk_cache::Backend::Iterator> MockDiskCache::CreateIterator() {
  return std::make_unique<NotImplementedIterator>();
}

void MockDiskCache::GetStats(base::StringPairs* stats) {}

void MockDiskCache::OnExternalCacheHit(const std::string& key) {
  external_cache_hits_.push_back(key);
}

uint8_t MockDiskCache::GetEntryInMemoryData(const std::string& key) {
  if (!support_in_memory_entry_data_) {
    return 0;
  }

  auto it = entries_.find(key);
  if (it != entries_.end()) {
    return it->second->in_memory_data();
  }
  return 0;
}

void MockDiskCache::SetEntryInMemoryData(const std::string& key, uint8_t data) {
  auto it = entries_.find(key);
  if (it != entries_.end()) {
    it->second->set_in_memory_data(data);
  }
}

int64_t MockDiskCache::MaxFileSize() const {
  return max_file_size_;
}

void MockDiskCache::ReleaseAll() {
  for (auto entry : entries_) {
    entry.second->Release();
  }
  entries_.clear();
}

void MockDiskCache::CallbackLater(base::OnceClosure callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, std::move(callback));
}

bool MockDiskCache::IsDiskEntryDoomed(const std::string& key) {
  auto it = entries_.find(key);
  if (it != entries_.end()) {
    return it->second->is_doomed();
  }

  return false;
}

void MockDiskCache::ResumeCacheOperation() {
  DCHECK(!resume_callback_.is_null());
  CallbackLater(std::move(resume_callback_));
}

scoped_refptr<MockDiskEntry> MockDiskCache::GetDiskEntryRef(
    const std::string& key) {
  auto it = entries_.find(key);
  if (it == entries_.end()) {
    return nullptr;
  }
  return it->second.get();
}

const std::vector<std::string>& MockDiskCache::GetExternalCacheHits() const {
  return external_cache_hits_;
}

//-----------------------------------------------------------------------------

disk_cache::BackendResult MockBackendFactory::CreateBackend(
    NetLog* net_log,
    disk_cache::BackendResultCallback callback) {
  return disk_cache::BackendResult::Make(std::make_unique<MockDiskCache>());
}

//-----------------------------------------------------------------------------

MockHttpCache::MockHttpCache()
    : MockHttpCache(std::make_unique<MockBackendFactory>()) {}

MockHttpCache::MockHttpCache(
    std::unique_ptr<HttpCache::BackendFactory> disk_cache_factory)
    : http_cache_(std::make_unique<MockNetworkLayer>(),
                  std::move(disk_cache_factory)) {}

disk_cache::Backend* MockHttpCache::backend() {
  TestGetBackendCompletionCallback cb;
  HttpCache::GetBackendResult result = http_cache_.GetBackend(cb.callback());
  result = cb.GetResult(result);
  return (result.first == OK) ? result.second : nullptr;
}

MockDiskCache* MockHttpCache::disk_cache() {
  return static_cast<MockDiskCache*>(backend());
}

int MockHttpCache::CreateTransaction(std::unique_ptr<HttpTransaction>* trans) {
  return http_cache_.CreateTransaction(DEFAULT_PRIORITY, trans);
}

void MockHttpCache::SimulateCacheLockTimeout() {
  http_cache_.SimulateCacheLockTimeoutForTesting();
}

void MockHttpCache::SimulateCacheLockTimeoutAfterHeaders() {
  http_cache_.SimulateCacheLockTimeoutAfterHeadersForTesting();
}

void MockHttpCache::FailConditionalizations() {
  http_cache_.FailConditionalizationForTest();
}

bool MockHttpCache::ReadResponseInfo(disk_cache::Entry* disk_entry,
                                     HttpResponseInfo* response_info,
                                     bool* response_truncated) {
  int size = disk_entry->GetDataSize(0);

  TestCompletionCallback cb;
  auto buffer = base::MakeRefCounted<IOBufferWithSize>(size);
  int rv = disk_entry->ReadData(0, 0, buffer.get(), size, cb.callback());
  rv = cb.GetResult(rv);
  EXPECT_EQ(size, rv);

  return HttpCache::ParseResponseInfo(buffer->span(), response_info,
                                      response_truncated);
}

bool MockHttpCache::WriteResponseInfo(disk_cache::Entry* disk_entry,
                                      const HttpResponseInfo* response_info,
                                      bool skip_transient_headers,
                                      bool response_truncated) {
  base::Pickle pickle;
  response_info->Persist(&pickle, skip_transient_headers, response_truncated);

  TestCompletionCallback cb;
  int len = static_cast<int>(pickle.size());
  auto data = base::MakeRefCounted<WrappedIOBuffer>(pickle);

  int rv = disk_entry->WriteData(0, 0, data.get(), len, cb.callback(), true);
  rv = cb.GetResult(rv);
  return (rv == len);
}

bool MockHttpCache::OpenBackendEntry(const std::string& key,
                                     disk_cache::Entry** entry) {
  TestEntryResultCompletionCallback cb;
  disk_cache::EntryResult result =
      backend()->OpenEntry(key, HIGHEST, cb.callback());
  result = cb.GetResult(std::move(result));
  if (result.net_error() == OK) {
    *entry = result.ReleaseEntry();
    return true;
  } else {
    return false;
  }
}

bool MockHttpCache::CreateBackendEntry(const std::string& key,
                                       disk_cache::Entry** entry,
                                       NetLog* net_log) {
  TestEntryResultCompletionCallback cb;
  disk_cache::EntryResult result =
      backend()->CreateEntry(key, HIGHEST, cb.callback());
  result = cb.GetResult(std::move(result));
  if (result.net_error() == OK) {
    *entry = result.ReleaseEntry();
    return true;
  } else {
    return false;
  }
}

// Static.
int MockHttpCache::GetTestMode(int test_mode) {
  if (!g_test_mode) {
    return test_mode;
  }

  return g_test_mode;
}

// Static.
void MockHttpCache::SetTestMode(int test_mode) {
  g_test_mode = test_mode;
}

bool MockHttpCache::IsWriterPresent(const std::string& key) {
  auto entry = http_cache_.GetActiveEntry(key);
  return entry && entry->HasWriters() && !entry->writers()->IsEmpty();
}

bool MockHttpCache::IsHeadersTransactionPresent(const std::string& key) {
  auto entry = http_cache_.GetActiveEntry(key);
  return entry && entry->headers_transaction();
}

int MockHttpCache::GetCountReaders(const std::string& key) {
  auto entry = http_cache_.GetActiveEntry(key);
  return entry ? entry->readers().size() : 0;
}

int MockHttpCache::GetCountAddToEntryQueue(const std::string& key) {
  auto entry = http_cache_.GetActiveEntry(key);
  return entry ? entry->add_to_entry_queue().size() : 0;
}

int MockHttpCache::GetCountDoneHeadersQueue(const std::string& key) {
  auto entry = http_cache_.GetActiveEntry(key);
  return entry ? entry->done_headers_queue().size() : 0;
}

int MockHttpCache::GetCountWriterTransactions(const std::string& key) {
  auto entry = http_cache_.GetActiveEntry(key);
  return entry && entry->writers() ? entry->writers()->GetTransactionsCount()
                                   : 0;
}

base::WeakPtr<HttpCache> MockHttpCache::GetWeakPtr() {
  return http_cache_.GetWeakPtr();
}

//-----------------------------------------------------------------------------

disk_cache::EntryResult MockDiskCacheNoCB::CreateEntry(
    const std::string& key,
    RequestPriority request_priority,
    EntryResultCallback callback) {
  return EntryResult::MakeError(ERR_IO_PENDING);
}

//-----------------------------------------------------------------------------

disk_cache::BackendResult MockBackendNoCbFactory::CreateBackend(
    NetLog* net_log,
    disk_cache::BackendResultCallback callback) {
  return disk_cache::BackendResult::Make(std::make_unique<MockDiskCacheNoCB>());
}

//-----------------------------------------------------------------------------

MockBlockingBackendFactory::MockBlockingBackendFactory() = default;
MockBlockingBackendFactory::~MockBlockingBackendFactory() = default;

disk_cache::BackendResult MockBlockingBackendFactory::CreateBackend(
    NetLog* net_log,
    disk_cache::BackendResultCallback callback) {
  if (!block_) {
    return MakeResult();
  }

  callback_ = std::move(callback);
  return disk_cache::BackendResult::MakeError(ERR_IO_PENDING);
}

void MockBlockingBackendFactory::FinishCreation() {
  block_ = false;
  if (!callback_.is_null()) {
    // Running the callback might delete |this|.
    std::move(callback_).Run(MakeResult());
  }
}

disk_cache::BackendResult MockBlockingBackendFactory::MakeResult() {
  if (fail_) {
    return disk_cache::BackendResult::MakeError(ERR_FAILED);
  } else {
    return disk_cache::BackendResult::Make(std::make_unique<MockDiskCache>());
  }
}

}  // namespace net
