// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <memory>
#include <set>
#include <utility>

#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/services/storage/public/cpp/buckets/constants.h"
#include "components/services/storage/public/mojom/cache_storage_control.mojom.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/cache_storage/cache_storage.h"
#include "content/browser/cache_storage/cache_storage_cache.h"
#include "content/browser/cache_storage/cache_storage_cache_handle.h"
#include "content/browser/cache_storage/cache_storage_histogram_utils.h"
#include "content/browser/cache_storage/cache_storage_manager.h"
#include "content/common/background_fetch/background_fetch_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_utils.h"
#include "crypto/symmetric_key.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"
#include "net/base/test_completion_callback.h"
#include "net/base/url_util.h"
#include "net/disk_cache/disk_cache.h"
#include "net/http/http_connection_info.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/blob_test_utils.h"
#include "storage/browser/test/fake_blob.h"
#include "storage/browser/test/mock_quota_manager_proxy.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "storage/common/quota/padding_key.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"
#include "url/origin.h"

using blink::FetchAPIRequestHeadersMap;
using blink::mojom::CacheStorageError;
using blink::mojom::CacheStorageVerboseErrorPtr;

namespace content {
namespace cache_storage_cache_unittest {

const char kTestData[] = "Hello World";
const char kCacheName[] = "test_cache";
const FetchAPIRequestHeadersMap kHeaders({{"a", "a"}, {"b", "b"}});

void SizeCallback(base::RunLoop* run_loop,
                  bool* callback_called,
                  int64_t* out_size,
                  int64_t size) {
  *callback_called = true;
  *out_size = size;
  if (run_loop)
    run_loop->Quit();
}

// A blob that never finishes writing to its pipe.
class SlowBlob : public storage::FakeBlob {
 public:
  explicit SlowBlob(base::OnceClosure quit_closure)
      : FakeBlob("foo"), quit_closure_(std::move(quit_closure)) {}

  void ReadAll(
      mojo::ScopedDataPipeProducerHandle producer_handle,
      mojo::PendingRemote<blink::mojom::BlobReaderClient> client) override {
    // Don't respond, forcing the consumer to wait forever.
    std::move(quit_closure_).Run();
  }

 private:
  base::OnceClosure quit_closure_;
};

// A disk_cache::Backend wrapper that can delay operations.
class DelayableBackend : public disk_cache::Backend {
 public:
  explicit DelayableBackend(std::unique_ptr<disk_cache::Backend> backend)
      : Backend(backend->GetCacheType()),
        backend_(std::move(backend)),
        delay_open_entry_(false) {}

  // disk_cache::Backend overrides
  int32_t GetEntryCount() const override { return backend_->GetEntryCount(); }
  EntryResult OpenEntry(const std::string& key,
                        net::RequestPriority request_priority,
                        EntryResultCallback callback) override {
    if (delay_open_entry_ && open_entry_callback_.is_null()) {
      open_entry_callback_ =
          base::BindOnce(&DelayableBackend::OpenEntryDelayedImpl,
                         base::Unretained(this), key, std::move(callback));
      if (open_entry_started_callback_)
        std::move(open_entry_started_callback_).Run();
      return EntryResult::MakeError(net::ERR_IO_PENDING);
    }
    return backend_->OpenEntry(key, request_priority, std::move(callback));
  }

  EntryResult CreateEntry(const std::string& key,
                          net::RequestPriority request_priority,
                          EntryResultCallback callback) override {
    return backend_->CreateEntry(key, request_priority, std::move(callback));
  }
  EntryResult OpenOrCreateEntry(const std::string& key,
                                net::RequestPriority request_priority,
                                EntryResultCallback callback) override {
    return backend_->OpenOrCreateEntry(key, request_priority,
                                       std::move(callback));
  }
  net::Error DoomEntry(const std::string& key,
                       net::RequestPriority request_priority,
                       CompletionOnceCallback callback) override {
    return backend_->DoomEntry(key, request_priority, std::move(callback));
  }
  net::Error DoomAllEntries(CompletionOnceCallback callback) override {
    return backend_->DoomAllEntries(std::move(callback));
  }
  net::Error DoomEntriesBetween(base::Time initial_time,
                                base::Time end_time,
                                CompletionOnceCallback callback) override {
    return backend_->DoomEntriesBetween(initial_time, end_time,
                                        std::move(callback));
  }
  net::Error DoomEntriesSince(base::Time initial_time,
                              CompletionOnceCallback callback) override {
    return backend_->DoomEntriesSince(initial_time, std::move(callback));
  }
  int64_t CalculateSizeOfAllEntries(
      Int64CompletionOnceCallback callback) override {
    return backend_->CalculateSizeOfAllEntries(std::move(callback));
  }
  std::unique_ptr<Iterator> CreateIterator() override {
    return backend_->CreateIterator();
  }
  void GetStats(base::StringPairs* stats) override {
    return backend_->GetStats(stats);
  }
  void OnExternalCacheHit(const std::string& key) override {
    return backend_->OnExternalCacheHit(key);
  }

  int64_t MaxFileSize() const override { return backend_->MaxFileSize(); }

  // Call to continue a delayed call to OpenEntry.
  bool OpenEntryContinue() {
    if (open_entry_callback_.is_null())
      return false;
    std::move(open_entry_callback_).Run();
    return true;
  }

  void set_delay_open_entry(bool value) { delay_open_entry_ = value; }

  void set_open_entry_started_callback(
      base::OnceClosure open_entry_started_callback) {
    open_entry_started_callback_ = std::move(open_entry_started_callback);
  }

 private:
  void OpenEntryDelayedImpl(const std::string& key,
                            EntryResultCallback callback) {
    auto split_callback = base::SplitOnceCallback(std::move(callback));
    EntryResult result =
        backend_->OpenEntry(key, net::HIGHEST, std::move(split_callback.first));
    if (result.net_error() != net::ERR_IO_PENDING)
      std::move(split_callback.second).Run(std::move(result));
  }

  std::unique_ptr<disk_cache::Backend> backend_;
  bool delay_open_entry_;
  base::OnceClosure open_entry_callback_;
  base::OnceClosure open_entry_started_callback_;
};

class FailableCacheEntry : public disk_cache::Entry {
 public:
  explicit FailableCacheEntry(disk_cache::Entry* entry) : entry_(entry) {}

  void Doom() override { entry_->Doom(); }
  void Close() override { entry_->Close(); }
  std::string GetKey() const override { return entry_->GetKey(); }
  base::Time GetLastUsed() const override { return entry_->GetLastUsed(); }
  base::Time GetLastModified() const override {
    return entry_->GetLastModified();
  }
  int32_t GetDataSize(int index) const override {
    return entry_->GetDataSize(index);
  }
  int ReadData(int index,
               int offset,
               IOBuffer* buf,
               int buf_len,
               CompletionOnceCallback callback) override {
    return entry_->ReadData(index, offset, buf, buf_len, std::move(callback));
  }
  int WriteData(int index,
                int offset,
                IOBuffer* buf,
                int buf_len,
                CompletionOnceCallback callback,
                bool truncate) override {
    std::move(callback).Run(net::ERR_FAILED);
    return net::ERR_IO_PENDING;
  }
  int ReadSparseData(int64_t offset,
                     IOBuffer* buf,
                     int buf_len,
                     CompletionOnceCallback callback) override {
    return entry_->ReadSparseData(offset, buf, buf_len, std::move(callback));
  }
  int WriteSparseData(int64_t offset,
                      IOBuffer* buf,
                      int buf_len,
                      CompletionOnceCallback callback) override {
    return entry_->WriteSparseData(offset, buf, buf_len, std::move(callback));
  }
  disk_cache::RangeResult GetAvailableRange(
      int64_t offset,
      int len,
      disk_cache::RangeResultCallback callback) override {
    return entry_->GetAvailableRange(offset, len, std::move(callback));
  }
  bool CouldBeSparse() const override { return entry_->CouldBeSparse(); }
  void CancelSparseIO() override { entry_->CancelSparseIO(); }
  net::Error ReadyForSparseIO(CompletionOnceCallback callback) override {
    return entry_->ReadyForSparseIO(std::move(callback));
  }
  void SetLastUsedTimeForTest(base::Time time) override {
    entry_->SetLastUsedTimeForTest(time);
  }

 private:
  const raw_ptr<disk_cache::Entry> entry_;
};

class FailableBackend : public disk_cache::Backend {
 public:
  enum class FailureStage { CREATE_ENTRY = 0, WRITE_HEADERS = 1 };
  explicit FailableBackend(std::unique_ptr<disk_cache::Backend> backend,
                           FailureStage stage)
      : Backend(backend->GetCacheType()),
        backend_(std::move(backend)),
        stage_(stage) {}

  // disk_cache::Backend overrides
  int32_t GetEntryCount() const override { return backend_->GetEntryCount(); }

  EntryResult OpenOrCreateEntry(const std::string& key,
                                net::RequestPriority request_priority,
                                EntryResultCallback callback) override {
    if (stage_ == FailureStage::CREATE_ENTRY) {
      return EntryResult::MakeError(net::ERR_FILE_NO_SPACE);
    } else if (stage_ == FailureStage::WRITE_HEADERS) {
      return backend_->OpenOrCreateEntry(
          key, request_priority,
          base::BindOnce(
              [](EntryResultCallback callback, disk_cache::EntryResult result) {
                FailableCacheEntry failable_entry(result.ReleaseEntry());
                EntryResult failable_result =
                    EntryResult::MakeCreated(&failable_entry);
                std::move(callback).Run(std::move(failable_result));
              },
              std::move(callback)));
    } else {
      return backend_->OpenOrCreateEntry(key, request_priority,
                                         std::move(callback));
    }
  }

  EntryResult OpenEntry(const std::string& key,
                        net::RequestPriority request_priority,
                        EntryResultCallback callback) override {
    return backend_->OpenEntry(key, request_priority, std::move(callback));
  }

  EntryResult CreateEntry(const std::string& key,
                          net::RequestPriority request_priority,
                          EntryResultCallback callback) override {
    return backend_->CreateEntry(key, request_priority, std::move(callback));
  }

  net::Error DoomEntry(const std::string& key,
                       net::RequestPriority request_priority,
                       CompletionOnceCallback callback) override {
    return backend_->DoomEntry(key, request_priority, std::move(callback));
  }

  net::Error DoomAllEntries(CompletionOnceCallback callback) override {
    return backend_->DoomAllEntries(std::move(callback));
  }

  net::Error DoomEntriesBetween(base::Time initial_time,
                                base::Time end_time,
                                CompletionOnceCallback callback) override {
    return backend_->DoomEntriesBetween(initial_time, end_time,
                                        std::move(callback));
  }
  net::Error DoomEntriesSince(base::Time initial_time,
                              CompletionOnceCallback callback) override {
    return backend_->DoomEntriesSince(initial_time, std::move(callback));
  }
  int64_t CalculateSizeOfAllEntries(
      Int64CompletionOnceCallback callback) override {
    return backend_->CalculateSizeOfAllEntries(std::move(callback));
  }
  std::unique_ptr<Iterator> CreateIterator() override {
    return backend_->CreateIterator();
  }
  void GetStats(base::StringPairs* stats) override {
    return backend_->GetStats(stats);
  }
  void OnExternalCacheHit(const std::string& key) override {
    return backend_->OnExternalCacheHit(key);
  }
  int64_t MaxFileSize() const override { return backend_->MaxFileSize(); }

 private:
  std::unique_ptr<disk_cache::Backend> backend_;
  FailureStage stage_;
};

std::string CopySideData(blink::mojom::Blob* actual_blob) {
  std::string output;
  base::RunLoop loop;
  actual_blob->ReadSideData(base::BindLambdaForTesting(
      [&](const std::optional<mojo_base::BigBuffer> data) {
        if (data)
          output.append(data->data(), data->data() + data->size());
        loop.Quit();
      }));
  loop.Run();
  return output;
}

bool ResponseMetadataEqual(const blink::mojom::FetchAPIResponse& expected,
                           const blink::mojom::FetchAPIResponse& actual) {
  EXPECT_EQ(expected.status_code, actual.status_code);
  if (expected.status_code != actual.status_code)
    return false;
  EXPECT_EQ(expected.status_text, actual.status_text);
  if (expected.status_text != actual.status_text)
    return false;
  EXPECT_EQ(expected.url_list.size(), actual.url_list.size());
  if (expected.url_list.size() != actual.url_list.size())
    return false;
  for (size_t i = 0; i < expected.url_list.size(); ++i) {
    EXPECT_EQ(expected.url_list[i], actual.url_list[i]);
    if (expected.url_list[i] != actual.url_list[i])
      return false;
  }
  EXPECT_EQ(!expected.blob, !actual.blob);
  if (!expected.blob != !actual.blob)
    return false;

  if (expected.blob) {
    if (expected.blob->size == 0) {
      EXPECT_STREQ("", actual.blob->uuid.c_str());
      if (!actual.blob->uuid.empty())
        return false;
    } else {
      EXPECT_STRNE("", actual.blob->uuid.c_str());
      if (actual.blob->uuid.empty())
        return false;
    }
  }

  EXPECT_EQ(expected.response_time, actual.response_time);
  if (expected.response_time != actual.response_time)
    return false;

  EXPECT_EQ(expected.cache_storage_cache_name, actual.cache_storage_cache_name);
  if (expected.cache_storage_cache_name != actual.cache_storage_cache_name)
    return false;

  EXPECT_EQ(expected.cors_exposed_header_names,
            actual.cors_exposed_header_names);
  if (expected.cors_exposed_header_names != actual.cors_exposed_header_names)
    return false;

  return true;
}

blink::mojom::FetchAPIResponsePtr SetCacheName(
    blink::mojom::FetchAPIResponsePtr response) {
  response->response_source =
      network::mojom::FetchResponseSource::kCacheStorage;
  response->cache_storage_cache_name = kCacheName;
  return response;
}

void OnBadMessage(std::string* result) {
  *result = "CSDH_UNEXPECTED_OPERATION";
}

// A CacheStorageCache that can optionally delay during backend creation.
class TestCacheStorageCache : public CacheStorageCache {
 public:
  TestCacheStorageCache(
      const storage::BucketLocator& bucket_locator,
      const std::string& cache_name,
      const base::FilePath& path,
      CacheStorage* cache_storage,
      const scoped_refptr<storage::QuotaManagerProxy>& quota_manager_proxy,
      scoped_refptr<BlobStorageContextWrapper> blob_storage_context)
      : CacheStorageCache(bucket_locator,
                          storage::mojom::CacheStorageOwner::kCacheAPI,
                          cache_name,
                          path,
                          cache_storage,
                          base::SingleThreadTaskRunner::GetCurrentDefault(),
                          quota_manager_proxy,
                          std::move(blob_storage_context),
                          0 /* cache_size */,
                          0 /* cache_padding */),
        delay_backend_creation_(false) {}

  TestCacheStorageCache(const TestCacheStorageCache&) = delete;
  TestCacheStorageCache& operator=(const TestCacheStorageCache&) = delete;

  ~TestCacheStorageCache() override { base::RunLoop().RunUntilIdle(); }

  void CreateBackend(ErrorCallback callback) override {
    backend_creation_callback_ = std::move(callback);
    if (delay_backend_creation_)
      return;
    ContinueCreateBackend();
  }

  void ContinueCreateBackend() {
    CacheStorageCache::CreateBackend(std::move(backend_creation_callback_));
  }

  void set_delay_backend_creation(bool delay) {
    delay_backend_creation_ = delay;
  }

  // Swap the existing backend with a delayable one. The backend must have been
  // created before calling this.
  DelayableBackend* UseDelayableBackend() {
    EXPECT_TRUE(backend_);
    DelayableBackend* delayable_backend =
        new DelayableBackend(std::move(backend_));
    backend_.reset(delayable_backend);
    return delayable_backend;
  }

  void UseFailableBackend(FailableBackend::FailureStage stage) {
    EXPECT_TRUE(backend_);
    auto failable_backend =
        std::make_unique<FailableBackend>(std::move(backend_), stage);
    backend_ = std::move(failable_backend);
  }

  void Init() { InitBackend(); }

  base::CheckedNumeric<uint64_t> GetRequiredSafeSpaceForRequest(
      const blink::mojom::FetchAPIRequestPtr& request) {
    return CalculateRequiredSafeSpaceForRequest(request);
  }

  base::CheckedNumeric<uint64_t> GetRequiredSafeSpaceForResponse(
      const blink::mojom::FetchAPIResponsePtr& response) {
    return CalculateRequiredSafeSpaceForResponse(response);
  }

 private:
  bool delay_backend_creation_;
  ErrorCallback backend_creation_callback_;
};

class MockCacheStorage : public CacheStorage {
 public:
  MockCacheStorage(
      const base::FilePath& origin_path,
      bool memory_only,
      base::SequencedTaskRunner* cache_task_runner,
      scoped_refptr<base::SequencedTaskRunner> scheduler_task_runner,
      scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy,
      scoped_refptr<BlobStorageContextWrapper> blob_storage_context,
      CacheStorageManager* cache_storage_manager,
      const storage::BucketLocator& bucket_locator,
      storage::mojom::CacheStorageOwner owner)
      : CacheStorage(origin_path,
                     memory_only,
                     cache_task_runner,
                     std::move(scheduler_task_runner),
                     std::move(quota_manager_proxy),
                     std::move(blob_storage_context),
                     cache_storage_manager,
                     bucket_locator,
                     owner) {}

  void CacheUnreferenced(CacheStorageCache* cache) override {
    // Normally the CacheStorage will attempt to delete the cache
    // from its map when the cache has become unreferenced.  Since we are
    // using detached cache objects we instead override to do nothing here.
  }
};

class CacheStorageCacheTest : public testing::Test {
 public:
  CacheStorageCacheTest()
      : task_environment_(BrowserTaskEnvironment::IO_MAINLOOP) {}

  void SetUp() override {
    ChromeBlobStorageContext* blob_storage_context =
        ChromeBlobStorageContext::GetFor(&browser_context_);
    // Wait for chrome_blob_storage_context to finish initializing.
    base::RunLoop().RunUntilIdle();

    mojo::PendingRemote<storage::mojom::BlobStorageContext> remote;
    blob_storage_context->BindMojoContext(
        remote.InitWithNewPipeAndPassReceiver());
    blob_storage_context_ =
        base::MakeRefCounted<BlobStorageContextWrapper>(std::move(remote));

    const bool is_incognito = MemoryOnly();
    if (!is_incognito) {
      ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
      temp_dir_path_ = temp_dir_.GetPath();
    }

    quota_policy_ = base::MakeRefCounted<storage::MockSpecialStoragePolicy>();
    mock_quota_manager_ = base::MakeRefCounted<storage::MockQuotaManager>(
        is_incognito, temp_dir_path_,
        base::SingleThreadTaskRunner::GetCurrentDefault(), quota_policy_);
    SetQuota(1024 * 1024 * 100);

    quota_manager_proxy_ = base::MakeRefCounted<storage::MockQuotaManagerProxy>(
        mock_quota_manager_.get(),
        base::SingleThreadTaskRunner::GetCurrentDefault());

    CreateRequests(blob_storage_context);

    response_time_ = base::Time::Now();
    for (int i = 0; i < 100; ++i)
      expected_blob_data_ += kTestData;

    blob_storage_context_->context()->RegisterFromMemory(
        blob_remote_.BindNewPipeAndPassReceiver(), expected_blob_uuid_,
        std::vector<uint8_t>(expected_blob_data_.begin(),
                             expected_blob_data_.end()));

    ASSERT_OK_AND_ASSIGN(auto bucket_locator,
                         GetOrCreateDefaultBucket(kTestUrl));
    // Use a mock CacheStorage object so we can use real
    // CacheStorageCacheHandle reference counting.  A CacheStorage
    // must be present to be notified when a cache becomes unreferenced.
    mock_cache_storage_ = std::make_unique<MockCacheStorage>(
        temp_dir_path_, MemoryOnly(),
        base::SingleThreadTaskRunner::GetCurrentDefault().get(),
        base::SingleThreadTaskRunner::GetCurrentDefault(), quota_manager_proxy_,
        blob_storage_context_, /* cache_storage_manager = */ nullptr,
        bucket_locator, storage::mojom::CacheStorageOwner::kCacheAPI);

    InitCache(mock_cache_storage_.get(), bucket_locator);
  }

  void TearDown() override {
    disk_cache::FlushCacheThreadForTesting();
    content::RunAllTasksUntilIdle();
  }

  storage::QuotaErrorOr<storage::BucketLocator> GetOrCreateDefaultBucket(
      const GURL& url) {
    const auto storage_key =
        blink::StorageKey::CreateFirstParty(url::Origin::Create(url));
    base::test::TestFuture<storage::QuotaErrorOr<storage::BucketInfo>> future;
    quota_manager_proxy_->UpdateOrCreateBucket(
        storage::BucketInitParams::ForDefaultBucket(storage_key),
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        future.GetCallback());
    return future.Take().transform(&storage::BucketInfo::ToBucketLocator);
  }

  GURL BodyUrl() const {
    GURL::Replacements replacements;
    replacements.SetPathStr("/body.html");
    return kTestUrl.ReplaceComponents(replacements);
  }

  GURL BodyUrlWithQuery() const {
    return net::AppendQueryParameter(BodyUrl(), "query", "test");
  }

  GURL BodyUrlWithRef(std::string ref = "ref") const {
    return net::AppendOrReplaceRef(BodyUrl(), ref);
  }

  GURL NoBodyUrl() const {
    GURL::Replacements replacements;
    replacements.SetPathStr("/no_body.html");
    return kTestUrl.ReplaceComponents(replacements);
  }

  void InitCache(CacheStorage* cache_storage,
                 const storage::BucketLocator& bucket_locator) {
    cache_ = std::make_unique<TestCacheStorageCache>(
        bucket_locator, kCacheName, temp_dir_path_, cache_storage,
        quota_manager_proxy_, blob_storage_context_);
    cache_->Init();
  }

  void CreateRequests(ChromeBlobStorageContext* blob_storage_context) {
    body_request_ = CreateFetchAPIRequest(BodyUrl(), "GET", kHeaders,
                                          blink::mojom::Referrer::New(), false);
    body_request_with_query_ =
        CreateFetchAPIRequest(BodyUrlWithQuery(), "GET", kHeaders,
                              blink::mojom::Referrer::New(), false);
    body_request_with_fragment_ =
        CreateFetchAPIRequest(BodyUrlWithRef(), "GET", kHeaders,
                              blink::mojom::Referrer::New(), false);
    body_request_with_different_fragment_ =
        CreateFetchAPIRequest(BodyUrlWithRef("ref2"), "GET", kHeaders,
                              blink::mojom::Referrer::New(), false);
    no_body_request_ = CreateFetchAPIRequest(
        NoBodyUrl(), "GET", kHeaders, blink::mojom::Referrer::New(), false);
    body_head_request_ = CreateFetchAPIRequest(
        BodyUrl(), "HEAD", kHeaders, blink::mojom::Referrer::New(), false);
  }

  blink::mojom::FetchAPIResponsePtr CreateBlobBodyResponse() {
    auto blob = blink::mojom::SerializedBlob::New();
    blob->uuid = expected_blob_uuid_;
    blob->size = expected_blob_data_.size();
    // Use cloned blob remote for all responses with blob body.
    blob_remote_->Clone(blob->blob.InitWithNewPipeAndPassReceiver());

    blink::mojom::FetchAPIResponsePtr response = CreateNoBodyResponse();
    response->url_list = {BodyUrl()};
    response->blob = std::move(blob);
    return response;
  }

  blink::mojom::FetchAPIResponsePtr CreateOpaqueResponse() {
    auto response = CreateBlobBodyResponse();
    response->response_type = network::mojom::FetchResponseType::kOpaque;
    response->response_time = base::Time::Now();

    // CacheStorage depends on fetch to provide the opaque response padding
    // value now.  We prepolute a padding value here to simulate that.
    response->padding = 10;

    return response;
  }

  blink::mojom::FetchAPIResponsePtr CreateBlobBodyResponseWithQuery() {
    blink::mojom::FetchAPIResponsePtr response = CreateBlobBodyResponse();
    response->url_list = {BodyUrlWithQuery()};
    response->cors_exposed_header_names = {"a"};
    return response;
  }

  blink::mojom::FetchAPIResponsePtr CreateNoBodyResponse() {
    return blink::mojom::FetchAPIResponse::New(
        std::vector<GURL>({NoBodyUrl()}), 200, "OK",
        network::mojom::FetchResponseType::kDefault, /*padding=*/0,
        network::mojom::FetchResponseSource::kUnspecified,
        base::flat_map<std::string, std::string>(kHeaders.cbegin(),
                                                 kHeaders.cend()),
        /*mime_type=*/std::nullopt, net::HttpRequestHeaders::kGetMethod,
        /*blob=*/nullptr, blink::mojom::ServiceWorkerResponseError::kUnknown,
        response_time_, /*cache_storage_cache_name=*/std::string(),
        /*cors_exposed_header_names=*/std::vector<std::string>(),
        /*side_data_blob=*/nullptr,
        /*side_data_blob_cache_put=*/nullptr,
        network::mojom::ParsedHeaders::New(), net::HttpConnectionInfo::kUNKNOWN,
        /*alpn_negotiated_protocol=*/"unknown",
        /*was_fetched_via_spdy=*/false, /*has_range_requested=*/false,
        /*auth_challenge_info=*/std::nullopt,
        /*request_include_credentials=*/true);
  }

  void CopySideDataToResponse(const std::string& uuid,
                              const std::string& data,
                              blink::mojom::FetchAPIResponse* response) {
    auto& blob = response->side_data_blob_for_cache_put;
    blob = blink::mojom::SerializedBlob::New();
    blob->uuid = uuid;
    blob->size = data.size();
    blob_storage_context_->context()->RegisterFromMemory(
        blob->blob.InitWithNewPipeAndPassReceiver(), uuid,
        std::vector<uint8_t>(data.begin(), data.end()));
  }

  blink::mojom::FetchAPIRequestPtr CopyFetchRequest(
      const blink::mojom::FetchAPIRequestPtr& request) {
    return CreateFetchAPIRequest(request->url, request->method,
                                 request->headers, request->referrer.Clone(),
                                 request->is_reload);
  }

  CacheStorageError BatchOperation(
      std::vector<blink::mojom::BatchOperationPtr> operations) {
    std::unique_ptr<base::RunLoop> loop(new base::RunLoop());

    cache_->BatchOperation(
        std::move(operations), /* trace_id = */ 0,
        base::BindOnce(&CacheStorageCacheTest::VerboseErrorTypeCallback,
                       base::Unretained(this), base::Unretained(loop.get())),
        base::BindOnce(&OnBadMessage, base::Unretained(&bad_message_reason_)));
    // TODO(jkarlin): These functions should use base::RunLoop().RunUntilIdle()
    // once the cache uses a passed in task runner instead of the CACHE thread.
    loop->Run();

    return callback_error_;
  }

  void CheckOpHistograms(base::HistogramTester& histogram_tester,
                         const char* op_name) {
    std::string base("ServiceWorkerCache.Cache.Scheduler.");
    histogram_tester.ExpectTotalCount(base + "OperationDuration2." + op_name,
                                      1);
    histogram_tester.ExpectTotalCount(base + "QueueDuration2." + op_name, 1);
    histogram_tester.ExpectTotalCount(base + "QueueLength." + op_name, 1);
  }

  bool Put(const blink::mojom::FetchAPIRequestPtr& request,
           blink::mojom::FetchAPIResponsePtr response) {
    base::HistogramTester histogram_tester;
    blink::mojom::BatchOperationPtr operation =
        blink::mojom::BatchOperation::New();
    operation->operation_type = blink::mojom::OperationType::kPut;
    operation->request = BackgroundFetchSettledFetch::CloneRequest(request);
    operation->response = std::move(response);

    std::vector<blink::mojom::BatchOperationPtr> operations;
    operations.emplace_back(std::move(operation));
    CacheStorageError error = BatchOperation(std::move(operations));
    if (callback_error_ == CacheStorageError::kSuccess)
      CheckOpHistograms(histogram_tester, "Put");
    return error == CacheStorageError::kSuccess;
  }

  bool Match(const blink::mojom::FetchAPIRequestPtr& request,
             blink::mojom::CacheQueryOptionsPtr match_options = nullptr) {
    base::HistogramTester histogram_tester;
    std::unique_ptr<base::RunLoop> loop(new base::RunLoop());

    cache_->Match(
        CopyFetchRequest(request), std::move(match_options),
        CacheStorageSchedulerPriority::kNormal, /* trace_id = */ 0,
        base::BindOnce(&CacheStorageCacheTest::ResponseAndErrorCallback,
                       base::Unretained(this), base::Unretained(loop.get())));
    loop->Run();
    if (callback_error_ == CacheStorageError::kSuccess)
      CheckOpHistograms(histogram_tester, "Match");
    return callback_error_ == CacheStorageError::kSuccess;
  }

  bool MatchAll(const blink::mojom::FetchAPIRequestPtr& request,
                blink::mojom::CacheQueryOptionsPtr match_options,
                std::vector<blink::mojom::FetchAPIResponsePtr>* responses) {
    base::HistogramTester histogram_tester;
    base::RunLoop loop;
    cache_->MatchAll(
        CopyFetchRequest(request), std::move(match_options),
        /* trace_id = */ 0,
        base::BindOnce(&CacheStorageCacheTest::ResponsesAndErrorCallback,
                       base::Unretained(this), loop.QuitClosure(), responses));
    loop.Run();
    if (callback_error_ == CacheStorageError::kSuccess)
      CheckOpHistograms(histogram_tester, "MatchAll");
    return callback_error_ == CacheStorageError::kSuccess;
  }

  bool GetAllMatchedEntries(
      std::vector<blink::mojom::CacheEntryPtr>* cache_entries) {
    base::RunLoop loop;
    cache_->GetAllMatchedEntries(
        nullptr /* request */, nullptr /* options */,
        /* trace_id = */ 0,
        base::BindOnce(&CacheStorageCacheTest::CacheEntriesAndErrorCallback,
                       base::Unretained(this), loop.QuitClosure(),
                       cache_entries));
    loop.Run();
    return callback_error_ == CacheStorageError::kSuccess;
  }

  bool MatchAll(std::vector<blink::mojom::FetchAPIResponsePtr>* responses) {
    return MatchAll(blink::mojom::FetchAPIRequest::New(), nullptr, responses);
  }

  bool Delete(const blink::mojom::FetchAPIRequestPtr& request,
              blink::mojom::CacheQueryOptionsPtr match_options = nullptr) {
    base::HistogramTester histogram_tester;
    blink::mojom::BatchOperationPtr operation =
        blink::mojom::BatchOperation::New();
    operation->operation_type = blink::mojom::OperationType::kDelete;
    operation->request = BackgroundFetchSettledFetch::CloneRequest(request);
    operation->match_options = std::move(match_options);

    std::vector<blink::mojom::BatchOperationPtr> operations;
    operations.emplace_back(std::move(operation));
    CacheStorageError error = BatchOperation(std::move(operations));
    if (callback_error_ == CacheStorageError::kSuccess)
      CheckOpHistograms(histogram_tester, "Delete");
    return error == CacheStorageError::kSuccess;
  }

  bool Keys(const blink::mojom::FetchAPIRequestPtr& request =
                blink::mojom::FetchAPIRequest::New(),
            blink::mojom::CacheQueryOptionsPtr match_options = nullptr) {
    base::HistogramTester histogram_tester;
    std::unique_ptr<base::RunLoop> loop(new base::RunLoop());

    cache_->Keys(
        CopyFetchRequest(request), std::move(match_options),
        /* trace_id = */ 0,
        base::BindOnce(&CacheStorageCacheTest::RequestsCallback,
                       base::Unretained(this), base::Unretained(loop.get())));
    loop->Run();
    if (callback_error_ == CacheStorageError::kSuccess)
      CheckOpHistograms(histogram_tester, "Keys");
    return callback_error_ == CacheStorageError::kSuccess;
  }

  bool Close() {
    base::HistogramTester histogram_tester;
    std::unique_ptr<base::RunLoop> loop(new base::RunLoop());

    cache_->Close(base::BindOnce(&CacheStorageCacheTest::CloseCallback,
                                 base::Unretained(this),
                                 base::Unretained(loop.get())));
    loop->Run();
    if (callback_error_ == CacheStorageError::kSuccess)
      CheckOpHistograms(histogram_tester, "Close");
    return callback_closed_;
  }

  bool WriteSideData(const GURL& url,
                     base::Time expected_response_time,
                     scoped_refptr<net::IOBuffer> buffer,
                     int buf_len) {
    base::HistogramTester histogram_tester;
    base::RunLoop run_loop;
    cache_->WriteSideData(
        base::BindOnce(&CacheStorageCacheTest::ErrorTypeCallback,
                       base::Unretained(this), base::Unretained(&run_loop)),
        url, expected_response_time, /* trace_id = */ 0, buffer, buf_len);
    run_loop.Run();
    if (callback_error_ == CacheStorageError::kSuccess)
      CheckOpHistograms(histogram_tester, "WriteSideData");
    return callback_error_ == CacheStorageError::kSuccess;
  }

  int64_t Size() {
    base::HistogramTester histogram_tester;
    // Storage notification happens after an operation completes. Let the any
    // notifications complete before calling Size.
    base::RunLoop().RunUntilIdle();

    base::RunLoop run_loop;
    bool callback_called = false;
    int64_t result = 0;
    cache_->Size(
        base::BindOnce(&SizeCallback, &run_loop, &callback_called, &result));
    run_loop.Run();
    EXPECT_TRUE(callback_called);
    if (callback_error_ == CacheStorageError::kSuccess)
      CheckOpHistograms(histogram_tester, "Size");
    return result;
  }

  int64_t GetSizeThenClose() {
    base::HistogramTester histogram_tester;
    base::RunLoop run_loop;
    bool callback_called = false;
    int64_t result = 0;
    cache_->GetSizeThenClose(
        base::BindOnce(&SizeCallback, &run_loop, &callback_called, &result));
    run_loop.Run();
    EXPECT_TRUE(callback_called);
    if (callback_error_ == CacheStorageError::kSuccess)
      CheckOpHistograms(histogram_tester, "SizeThenClose");
    return result;
  }

  void RequestsCallback(base::RunLoop* run_loop,
                        CacheStorageError error,
                        std::unique_ptr<CacheStorageCache::Requests> requests) {
    callback_error_ = error;
    callback_strings_.clear();
    if (requests) {
      for (const blink::mojom::FetchAPIRequestPtr& request : *requests)
        callback_strings_.push_back(request->url.spec());
    }
    if (run_loop)
      run_loop->Quit();
  }

  void VerboseErrorTypeCallback(base::RunLoop* run_loop,
                                CacheStorageVerboseErrorPtr error) {
    ErrorTypeCallback(run_loop, error->value);
    callback_message_ = error->message;
  }

  void ErrorTypeCallback(base::RunLoop* run_loop, CacheStorageError error) {
    callback_message_ = std::nullopt;
    callback_error_ = error;
    if (run_loop)
      run_loop->Quit();
  }

  void SequenceCallback(int sequence,
                        int* sequence_out,
                        base::RunLoop* run_loop,
                        CacheStorageVerboseErrorPtr error) {
    *sequence_out = sequence;
    callback_error_ = error->value;
    if (run_loop)
      run_loop->Quit();
  }

  void ResponseAndErrorCallback(base::RunLoop* run_loop,
                                CacheStorageError error,
                                blink::mojom::FetchAPIResponsePtr response) {
    callback_error_ = error;
    callback_response_ = std::move(response);

    if (run_loop)
      run_loop->Quit();
  }

  void ResponsesAndErrorCallback(
      base::OnceClosure quit_closure,
      std::vector<blink::mojom::FetchAPIResponsePtr>* responses_out,
      CacheStorageError error,
      std::vector<blink::mojom::FetchAPIResponsePtr> responses) {
    callback_error_ = error;
    *responses_out = std::move(responses);
    std::move(quit_closure).Run();
  }

  void CacheEntriesAndErrorCallback(
      base::OnceClosure quit_closure,
      std::vector<blink::mojom::CacheEntryPtr>* cache_entries_out,
      CacheStorageError error,
      std::vector<blink::mojom::CacheEntryPtr> cache_entries) {
    callback_error_ = error;
    *cache_entries_out = std::move(cache_entries);
    std::move(quit_closure).Run();
  }

  void CloseCallback(base::RunLoop* run_loop) {
    EXPECT_FALSE(callback_closed_);
    callback_closed_ = true;
    if (run_loop)
      run_loop->Quit();
  }

  bool TestResponseType(network::mojom::FetchResponseType response_type) {
    blink::mojom::FetchAPIResponsePtr body_response = CreateBlobBodyResponse();
    body_response->response_type = response_type;
    if (storage::ShouldPadResponseType(response_type))
      body_response->padding = 10;
    EXPECT_TRUE(Put(body_request_, std::move(body_response)));
    EXPECT_TRUE(Match(body_request_));
    EXPECT_TRUE(Delete(body_request_));
    return response_type == callback_response_->response_type;
  }

  void VerifyAllOpsFail() {
    EXPECT_FALSE(Put(no_body_request_, CreateNoBodyResponse()));
    EXPECT_FALSE(Match(no_body_request_));
    EXPECT_FALSE(Delete(body_request_));
    EXPECT_FALSE(Keys());
  }

  virtual bool MemoryOnly() { return false; }

  void SetQuota(uint64_t quota) {
    mock_quota_manager_->SetQuota(
        blink::StorageKey::CreateFirstParty(url::Origin::Create(kTestUrl)),
        blink::mojom::StorageType::kTemporary, quota);
  }

  void SetMaxQuerySizeBytes(size_t max_bytes) {
    cache_->max_query_size_bytes_ = max_bytes;
  }

  size_t EstimatedResponseSizeWithoutBlob(
      const blink::mojom::FetchAPIResponse& response) {
    return CacheStorageCache::EstimatedResponseSizeWithoutBlob(response);
  }

 protected:
  base::ScopedTempDir temp_dir_;
  BrowserTaskEnvironment task_environment_;
  TestBrowserContext browser_context_;
  scoped_refptr<storage::MockSpecialStoragePolicy> quota_policy_;
  scoped_refptr<storage::MockQuotaManager> mock_quota_manager_;
  scoped_refptr<storage::MockQuotaManagerProxy> quota_manager_proxy_;
  scoped_refptr<BlobStorageContextWrapper> blob_storage_context_;
  std::unique_ptr<MockCacheStorage> mock_cache_storage_;

  base::FilePath temp_dir_path_;
  std::unique_ptr<TestCacheStorageCache> cache_;

  blink::mojom::FetchAPIRequestPtr body_request_;
  blink::mojom::FetchAPIRequestPtr body_request_with_query_;
  blink::mojom::FetchAPIRequestPtr body_request_with_fragment_;
  blink::mojom::FetchAPIRequestPtr body_request_with_different_fragment_;
  blink::mojom::FetchAPIRequestPtr no_body_request_;
  blink::mojom::FetchAPIRequestPtr body_head_request_;
  std::string expected_blob_uuid_ = "blob-id:myblob";
  // Holds a Mojo connection to the BlobImpl with uuid |expected_blob_uuid_|.
  mojo::Remote<blink::mojom::Blob> blob_remote_;
  base::Time response_time_;
  std::string expected_blob_data_;

  CacheStorageError callback_error_ = CacheStorageError::kSuccess;
  std::optional<std::string> callback_message_ = std::nullopt;
  blink::mojom::FetchAPIResponsePtr callback_response_;
  std::vector<std::string> callback_strings_;
  std::string bad_message_reason_;
  bool callback_closed_ = false;

  const GURL kTestUrl{"http://example.com"};
};

class CacheStorageCacheTestP : public CacheStorageCacheTest,
                               public testing::WithParamInterface<bool> {
 public:
  bool MemoryOnly() override { return !GetParam(); }
};

TEST_P(CacheStorageCacheTestP, PutNoBody) {
  EXPECT_TRUE(Put(no_body_request_, CreateNoBodyResponse()));
}

TEST_P(CacheStorageCacheTestP, PutBody) {
  EXPECT_TRUE(Put(body_request_, CreateBlobBodyResponse()));
}

TEST_P(CacheStorageCacheTestP, PutBody_Multiple) {
  blink::mojom::BatchOperationPtr operation1 =
      blink::mojom::BatchOperation::New();
  operation1->operation_type = blink::mojom::OperationType::kPut;
  operation1->request =
      BackgroundFetchSettledFetch::CloneRequest(body_request_);
  operation1->request->url = GURL("http://example.com/1");
  operation1->response = CreateBlobBodyResponse();
  operation1->response->url_list.emplace_back("http://example.com/1");
  blink::mojom::FetchAPIRequestPtr request1 =
      BackgroundFetchSettledFetch::CloneRequest(operation1->request);

  blink::mojom::BatchOperationPtr operation2 =
      blink::mojom::BatchOperation::New();
  operation2->operation_type = blink::mojom::OperationType::kPut;
  operation2->request =
      BackgroundFetchSettledFetch::CloneRequest(body_request_);
  operation2->request->url = GURL("http://example.com/2");
  operation2->response = CreateBlobBodyResponse();
  operation2->response->url_list.emplace_back("http://example.com/2");
  blink::mojom::FetchAPIRequestPtr request2 =
      BackgroundFetchSettledFetch::CloneRequest(operation2->request);

  blink::mojom::BatchOperationPtr operation3 =
      blink::mojom::BatchOperation::New();
  operation3->operation_type = blink::mojom::OperationType::kPut;
  operation3->request =
      BackgroundFetchSettledFetch::CloneRequest(body_request_);
  operation3->request->url = GURL("http://example.com/3");
  operation3->response = CreateBlobBodyResponse();
  operation3->response->url_list.emplace_back("http://example.com/3");
  blink::mojom::FetchAPIRequestPtr request3 =
      BackgroundFetchSettledFetch::CloneRequest(operation3->request);

  std::vector<blink::mojom::BatchOperationPtr> operations;
  operations.push_back(std::move(operation1));
  operations.push_back(std::move(operation2));
  operations.push_back(std::move(operation3));

  EXPECT_EQ(CacheStorageError::kSuccess, BatchOperation(std::move(operations)));
  EXPECT_TRUE(Match(request1));
  EXPECT_TRUE(Match(request2));
  EXPECT_TRUE(Match(request3));
}

TEST_P(CacheStorageCacheTestP, MatchLimit) {
  EXPECT_TRUE(Put(no_body_request_, CreateNoBodyResponse()));
  EXPECT_TRUE(Match(no_body_request_));

  size_t max_size = CacheStorageCache::EstimatedStructSize(no_body_request_) +
                    EstimatedResponseSizeWithoutBlob(*callback_response_);
  SetMaxQuerySizeBytes(max_size);
  EXPECT_TRUE(Match(no_body_request_));

  SetMaxQuerySizeBytes(max_size - 1);
  EXPECT_FALSE(Match(no_body_request_));
  EXPECT_EQ(CacheStorageError::kErrorQueryTooLarge, callback_error_);
}

TEST_P(CacheStorageCacheTestP, MatchAllLimit) {
  EXPECT_TRUE(Put(body_request_, CreateNoBodyResponse()));
  EXPECT_TRUE(Put(body_request_with_query_, CreateNoBodyResponse()));
  EXPECT_TRUE(Match(body_request_));

  size_t body_request_size =
      CacheStorageCache::EstimatedStructSize(body_request_) +
      EstimatedResponseSizeWithoutBlob(*callback_response_);
  size_t query_request_size =
      CacheStorageCache::EstimatedStructSize(body_request_with_query_) +
      EstimatedResponseSizeWithoutBlob(*callback_response_);

  std::vector<blink::mojom::FetchAPIResponsePtr> responses;
  blink::mojom::CacheQueryOptionsPtr match_options =
      blink::mojom::CacheQueryOptions::New();

  // There is enough room for both requests and responses
  SetMaxQuerySizeBytes(body_request_size + query_request_size);
  EXPECT_TRUE(MatchAll(body_request_, match_options->Clone(), &responses));
  EXPECT_EQ(1u, responses.size());

  match_options->ignore_search = true;
  EXPECT_TRUE(MatchAll(body_request_, match_options->Clone(), &responses));
  EXPECT_EQ(2u, responses.size());

  // There is not enough room for both requests and responses
  SetMaxQuerySizeBytes(body_request_size);
  match_options->ignore_search = false;
  EXPECT_TRUE(MatchAll(body_request_, match_options->Clone(), &responses));
  EXPECT_EQ(1u, responses.size());

  match_options->ignore_search = true;
  EXPECT_FALSE(MatchAll(body_request_, match_options->Clone(), &responses));
  EXPECT_EQ(CacheStorageError::kErrorQueryTooLarge, callback_error_);
}

TEST_P(CacheStorageCacheTestP, MatchWithFragment) {
  // When putting in the cache a request with body and fragment, it
  // must be retrievable using a different fragment or without any fragment.
  EXPECT_TRUE(Put(body_request_with_fragment_, CreateNoBodyResponse()));
  EXPECT_TRUE(Match(body_request_with_different_fragment_));
  EXPECT_TRUE(Match(body_request_));
}

TEST_P(CacheStorageCacheTestP, KeysLimit) {
  EXPECT_TRUE(Put(no_body_request_, CreateNoBodyResponse()));
  EXPECT_TRUE(Put(body_request_, CreateBlobBodyResponse()));

  size_t max_size = CacheStorageCache::EstimatedStructSize(no_body_request_) +
                    CacheStorageCache::EstimatedStructSize(body_request_);
  SetMaxQuerySizeBytes(max_size);
  EXPECT_TRUE(Keys());

  SetMaxQuerySizeBytes(
      CacheStorageCache::EstimatedStructSize(no_body_request_));
  EXPECT_FALSE(Keys());
  EXPECT_EQ(CacheStorageError::kErrorQueryTooLarge, callback_error_);
}

// TODO(nhiroki): Add a test for the case where one of PUT operations fails.
// Currently there is no handy way to fail only one operation in a batch.
// This could be easily achieved after adding some security checks in the
// browser side (http://crbug.com/425505).

TEST_P(CacheStorageCacheTestP, ResponseURLDiffersFromRequestURL) {
  blink::mojom::FetchAPIResponsePtr no_body_response = CreateNoBodyResponse();
  no_body_response->url_list.clear();
  no_body_response->url_list.emplace_back("http://example.com/foobar");
  EXPECT_STRNE("http://example.com/foobar",
               no_body_request_->url.spec().c_str());
  EXPECT_TRUE(Put(no_body_request_, std::move(no_body_response)));
  EXPECT_TRUE(Match(no_body_request_));
  ASSERT_EQ(1u, callback_response_->url_list.size());
  EXPECT_STREQ("http://example.com/foobar",
               callback_response_->url_list[0].spec().c_str());
}

TEST_P(CacheStorageCacheTestP, ResponseURLEmpty) {
  blink::mojom::FetchAPIResponsePtr no_body_response = CreateNoBodyResponse();
  no_body_response->url_list.clear();
  EXPECT_STRNE("", no_body_request_->url.spec().c_str());
  EXPECT_TRUE(Put(no_body_request_, std::move(no_body_response)));
  EXPECT_TRUE(Match(no_body_request_));
  EXPECT_EQ(0u, callback_response_->url_list.size());
}

TEST_P(CacheStorageCacheTestP, PutBodyDropBlobRef) {
  blink::mojom::BatchOperationPtr operation =
      blink::mojom::BatchOperation::New();
  operation->operation_type = blink::mojom::OperationType::kPut;
  operation->request = BackgroundFetchSettledFetch::CloneRequest(body_request_);
  operation->response = CreateBlobBodyResponse();

  std::vector<blink::mojom::BatchOperationPtr> operations;
  operations.emplace_back(std::move(operation));
  std::unique_ptr<base::RunLoop> loop(new base::RunLoop());
  cache_->BatchOperation(
      std::move(operations), /* trace_id = */ 0,
      base::BindOnce(&CacheStorageCacheTestP::VerboseErrorTypeCallback,
                     base::Unretained(this), base::Unretained(loop.get())),
      CacheStorageCache::BadMessageCallback());

  // The handle should be held by the cache now so the deref here should be
  // okay.
  blob_remote_.reset();

  loop->Run();

  EXPECT_EQ(CacheStorageError::kSuccess, callback_error_);
}

TEST_P(CacheStorageCacheTestP, PutBadMessage) {
  base::HistogramTester histogram_tester;

  // Two unique puts that will collectively overflow unit64_t size of the
  // batch operation.
  blink::mojom::BatchOperationPtr operation1 =
      blink::mojom::BatchOperation::New(
          blink::mojom::OperationType::kPut,
          BackgroundFetchSettledFetch::CloneRequest(body_request_),
          CreateBlobBodyResponse(), nullptr /* match_options */);
  operation1->response->blob->size = std::numeric_limits<uint64_t>::max();
  blink::mojom::BatchOperationPtr operation2 =
      blink::mojom::BatchOperation::New(
          blink::mojom::OperationType::kPut,
          BackgroundFetchSettledFetch::CloneRequest(body_request_with_query_),
          CreateBlobBodyResponse(), nullptr /* match_options */);
  operation2->response->blob->size = std::numeric_limits<uint64_t>::max();

  std::vector<blink::mojom::BatchOperationPtr> operations;
  operations.push_back(std::move(operation1));
  operations.push_back(std::move(operation2));
  EXPECT_EQ(CacheStorageError::kErrorStorage,
            BatchOperation(std::move(operations)));
  histogram_tester.ExpectBucketCount("ServiceWorkerCache.ErrorStorageType",
                                     ErrorStorageType::kBatchInvalidSpace, 1);
  EXPECT_EQ("CSDH_UNEXPECTED_OPERATION", bad_message_reason_);

  EXPECT_FALSE(Match(body_request_));
}

TEST_P(CacheStorageCacheTestP, PutReplace) {
  EXPECT_TRUE(Put(body_request_, CreateNoBodyResponse()));
  EXPECT_TRUE(Match(body_request_));
  EXPECT_FALSE(callback_response_->blob);

  EXPECT_TRUE(Put(body_request_, CreateBlobBodyResponse()));
  EXPECT_TRUE(Match(body_request_));
  EXPECT_TRUE(callback_response_->blob);

  EXPECT_TRUE(Put(body_request_, CreateNoBodyResponse()));
  EXPECT_TRUE(Match(body_request_));
  EXPECT_FALSE(callback_response_->blob);
}

TEST_P(CacheStorageCacheTestP, PutReplaceInBatchFails) {
  blink::mojom::BatchOperationPtr operation1 =
      blink::mojom::BatchOperation::New();
  operation1->operation_type = blink::mojom::OperationType::kPut;
  operation1->request =
      BackgroundFetchSettledFetch::CloneRequest(body_request_);
  operation1->response = CreateNoBodyResponse();

  blink::mojom::BatchOperationPtr operation2 =
      blink::mojom::BatchOperation::New();
  operation2->operation_type = blink::mojom::OperationType::kPut;
  operation2->request =
      BackgroundFetchSettledFetch::CloneRequest(body_request_);
  operation2->response = CreateBlobBodyResponse();

  std::vector<blink::mojom::BatchOperationPtr> operations;
  operations.push_back(std::move(operation1));
  operations.push_back(std::move(operation2));

  EXPECT_EQ(CacheStorageError::kErrorDuplicateOperation,
            BatchOperation(std::move(operations)));

  // A duplicate operation error should provide an informative message
  // containing the URL of the duplicate request.
  ASSERT_TRUE(callback_message_);
  EXPECT_TRUE(base::Contains(callback_message_.value(), BodyUrl().spec()));

  // Neither operation should have completed.
  EXPECT_FALSE(Match(body_request_));
}

TEST_P(CacheStorageCacheTestP, MatchNoBody) {
  EXPECT_TRUE(Put(no_body_request_, CreateNoBodyResponse()));
  EXPECT_TRUE(Match(no_body_request_));
  EXPECT_TRUE(ResponseMetadataEqual(*SetCacheName(CreateNoBodyResponse()),
                                    *callback_response_));
  EXPECT_FALSE(callback_response_->blob);
}

TEST_P(CacheStorageCacheTestP, MatchBody) {
  EXPECT_TRUE(Put(body_request_, CreateBlobBodyResponse()));
  EXPECT_TRUE(Match(body_request_));
  EXPECT_TRUE(ResponseMetadataEqual(*SetCacheName(CreateBlobBodyResponse()),
                                    *callback_response_));
  mojo::Remote<blink::mojom::Blob> blob(
      std::move(callback_response_->blob->blob));
  EXPECT_EQ(expected_blob_data_, storage::BlobToString(blob.get()));
}

TEST_P(CacheStorageCacheTestP, MatchBodyHead) {
  EXPECT_TRUE(Put(body_request_, CreateBlobBodyResponse()));
  EXPECT_FALSE(Match(body_head_request_));
}

TEST_P(CacheStorageCacheTestP, MatchAll_Empty) {
  std::vector<blink::mojom::FetchAPIResponsePtr> responses;
  EXPECT_TRUE(MatchAll(&responses));
  EXPECT_TRUE(responses.empty());
}

TEST_P(CacheStorageCacheTestP, MatchAll_NoBody) {
  EXPECT_TRUE(Put(no_body_request_, CreateNoBodyResponse()));

  std::vector<blink::mojom::FetchAPIResponsePtr> responses;
  EXPECT_TRUE(MatchAll(&responses));

  ASSERT_EQ(1u, responses.size());
  EXPECT_TRUE(ResponseMetadataEqual(*SetCacheName(CreateNoBodyResponse()),
                                    *responses[0]));
  EXPECT_FALSE(responses[0]->blob);
}

TEST_P(CacheStorageCacheTestP, MatchAll_Body) {
  EXPECT_TRUE(Put(body_request_, CreateBlobBodyResponse()));

  std::vector<blink::mojom::FetchAPIResponsePtr> responses;
  EXPECT_TRUE(MatchAll(&responses));

  ASSERT_EQ(1u, responses.size());
  EXPECT_TRUE(ResponseMetadataEqual(*SetCacheName(CreateBlobBodyResponse()),
                                    *responses[0]));
  mojo::Remote<blink::mojom::Blob> blob(std::move(responses[0]->blob->blob));
  EXPECT_EQ(expected_blob_data_, storage::BlobToString(blob.get()));
}

TEST_P(CacheStorageCacheTestP, MatchAll_TwoResponsesThenOne) {
  EXPECT_TRUE(Put(no_body_request_, CreateNoBodyResponse()));
  EXPECT_TRUE(Put(body_request_, CreateBlobBodyResponse()));

  std::vector<blink::mojom::FetchAPIResponsePtr> responses;
  EXPECT_TRUE(MatchAll(&responses));
  ASSERT_EQ(2u, responses.size());
  EXPECT_TRUE(responses[1]->blob);

  EXPECT_TRUE(ResponseMetadataEqual(*SetCacheName(CreateNoBodyResponse()),
                                    *responses[0]));
  EXPECT_FALSE(responses[0]->blob);
  EXPECT_TRUE(ResponseMetadataEqual(*SetCacheName(CreateBlobBodyResponse()),
                                    *responses[1]));
  mojo::Remote<blink::mojom::Blob> blob(std::move(responses[1]->blob->blob));
  EXPECT_EQ(expected_blob_data_, storage::BlobToString(blob.get()));

  responses.clear();

  EXPECT_TRUE(Delete(body_request_));
  EXPECT_TRUE(MatchAll(&responses));

  ASSERT_EQ(1u, responses.size());
  EXPECT_TRUE(ResponseMetadataEqual(*SetCacheName(CreateNoBodyResponse()),
                                    *responses[0]));
  EXPECT_FALSE(responses[0]->blob);
}

TEST_P(CacheStorageCacheTestP, Match_IgnoreSearch) {
  EXPECT_TRUE(Put(body_request_with_query_, CreateBlobBodyResponseWithQuery()));

  EXPECT_FALSE(Match(body_request_));
  blink::mojom::CacheQueryOptionsPtr match_options =
      blink::mojom::CacheQueryOptions::New();
  match_options->ignore_search = true;
  EXPECT_TRUE(Match(body_request_, std::move(match_options)));
}

TEST_P(CacheStorageCacheTestP, Match_IgnoreMethod) {
  EXPECT_TRUE(Put(body_request_, CreateBlobBodyResponse()));

  blink::mojom::FetchAPIRequestPtr post_request =
      BackgroundFetchSettledFetch::CloneRequest(body_request_);
  post_request->method = "POST";
  EXPECT_FALSE(Match(post_request));

  blink::mojom::CacheQueryOptionsPtr match_options =
      blink::mojom::CacheQueryOptions::New();
  match_options->ignore_method = true;
  EXPECT_TRUE(Match(post_request, std::move(match_options)));
}

TEST_P(CacheStorageCacheTestP, Match_IgnoreVary) {
  body_request_->headers["vary_foo"] = "foo";
  blink::mojom::FetchAPIResponsePtr body_response = CreateBlobBodyResponse();
  body_response->headers["vary"] = "vary_foo";
  EXPECT_TRUE(Put(body_request_, std::move(body_response)));
  EXPECT_TRUE(Match(body_request_));

  body_request_->headers["vary_foo"] = "bar";
  EXPECT_FALSE(Match(body_request_));

  blink::mojom::CacheQueryOptionsPtr match_options =
      blink::mojom::CacheQueryOptions::New();
  match_options->ignore_vary = true;
  EXPECT_TRUE(Match(body_request_, std::move(match_options)));
}

TEST_P(CacheStorageCacheTestP, GetAllMatchedEntries_RequestsIncluded) {
  EXPECT_TRUE(Put(body_request_, CreateBlobBodyResponse()));

  std::vector<blink::mojom::CacheEntryPtr> cache_entries;
  EXPECT_TRUE(GetAllMatchedEntries(&cache_entries));

  ASSERT_EQ(1u, cache_entries.size());
  const auto& request = cache_entries[0]->request;
  EXPECT_EQ(request->url, body_request_->url);
  EXPECT_EQ(request->headers, body_request_->headers);
  EXPECT_EQ(request->method, body_request_->method);

  auto& response = cache_entries[0]->response;
  EXPECT_TRUE(ResponseMetadataEqual(*SetCacheName(CreateBlobBodyResponse()),
                                    *response));
  mojo::Remote<blink::mojom::Blob> blob(std::move(response->blob->blob));
  EXPECT_EQ(expected_blob_data_, storage::BlobToString(blob.get()));
}

TEST_P(CacheStorageCacheTestP, Keys_IgnoreSearch) {
  EXPECT_TRUE(Put(body_request_with_query_, CreateBlobBodyResponseWithQuery()));

  EXPECT_TRUE(Keys(body_request_));
  EXPECT_EQ(0u, callback_strings_.size());

  blink::mojom::CacheQueryOptionsPtr match_options =
      blink::mojom::CacheQueryOptions::New();
  match_options->ignore_search = true;
  EXPECT_TRUE(Keys(body_request_, std::move(match_options)));
  EXPECT_EQ(1u, callback_strings_.size());
}

TEST_P(CacheStorageCacheTestP, Keys_IgnoreMethod) {
  EXPECT_TRUE(Put(body_request_, CreateBlobBodyResponse()));

  blink::mojom::FetchAPIRequestPtr post_request =
      BackgroundFetchSettledFetch::CloneRequest(body_request_);
  post_request->method = "POST";
  EXPECT_TRUE(Keys(post_request));
  EXPECT_EQ(0u, callback_strings_.size());

  blink::mojom::CacheQueryOptionsPtr match_options =
      blink::mojom::CacheQueryOptions::New();
  match_options->ignore_method = true;
  EXPECT_TRUE(Keys(post_request, std::move(match_options)));
  EXPECT_EQ(1u, callback_strings_.size());
}

TEST_P(CacheStorageCacheTestP, Keys_IgnoreVary) {
  body_request_->headers["vary_foo"] = "foo";
  blink::mojom::FetchAPIResponsePtr body_response = CreateBlobBodyResponse();
  body_response->headers["vary"] = "vary_foo";
  EXPECT_TRUE(Put(body_request_, std::move(body_response)));
  EXPECT_TRUE(Keys(body_request_));
  EXPECT_EQ(1u, callback_strings_.size());

  body_request_->headers["vary_foo"] = "bar";
  EXPECT_TRUE(Keys(body_request_));
  EXPECT_EQ(0u, callback_strings_.size());

  blink::mojom::CacheQueryOptionsPtr match_options =
      blink::mojom::CacheQueryOptions::New();
  match_options->ignore_vary = true;
  EXPECT_TRUE(Keys(body_request_, std::move(match_options)));
  EXPECT_EQ(1u, callback_strings_.size());
}

TEST_P(CacheStorageCacheTestP, Delete_IgnoreSearch) {
  EXPECT_TRUE(Put(body_request_with_query_, CreateBlobBodyResponseWithQuery()));

  EXPECT_FALSE(Delete(body_request_));
  blink::mojom::CacheQueryOptionsPtr match_options =
      blink::mojom::CacheQueryOptions::New();
  match_options->ignore_search = true;
  EXPECT_TRUE(Delete(body_request_, std::move(match_options)));
}

TEST_P(CacheStorageCacheTestP, Delete_IgnoreMethod) {
  EXPECT_TRUE(Put(body_request_, CreateBlobBodyResponse()));

  blink::mojom::FetchAPIRequestPtr post_request =
      BackgroundFetchSettledFetch::CloneRequest(body_request_);
  post_request->method = "POST";
  EXPECT_FALSE(Delete(post_request));

  blink::mojom::CacheQueryOptionsPtr match_options =
      blink::mojom::CacheQueryOptions::New();
  match_options->ignore_method = true;
  EXPECT_TRUE(Delete(post_request, std::move(match_options)));
}

TEST_P(CacheStorageCacheTestP, Delete_IgnoreVary) {
  body_request_->headers["vary_foo"] = "foo";
  blink::mojom::FetchAPIResponsePtr body_response = CreateBlobBodyResponse();
  body_response->headers["vary"] = "vary_foo";
  EXPECT_TRUE(Put(body_request_, std::move(body_response)));

  body_request_->headers["vary_foo"] = "bar";
  EXPECT_FALSE(Delete(body_request_));

  blink::mojom::CacheQueryOptionsPtr match_options =
      blink::mojom::CacheQueryOptions::New();
  match_options->ignore_vary = true;
  EXPECT_TRUE(Delete(body_request_, std::move(match_options)));
}

TEST_P(CacheStorageCacheTestP, MatchAll_IgnoreMethod) {
  EXPECT_TRUE(Put(body_request_, CreateBlobBodyResponse()));

  blink::mojom::FetchAPIRequestPtr post_request =
      BackgroundFetchSettledFetch::CloneRequest(body_request_);
  post_request->method = "POST";
  std::vector<blink::mojom::FetchAPIResponsePtr> responses;

  EXPECT_TRUE(MatchAll(post_request, nullptr, &responses));
  EXPECT_EQ(0u, responses.size());

  blink::mojom::CacheQueryOptionsPtr match_options =
      blink::mojom::CacheQueryOptions::New();
  match_options->ignore_method = true;
  EXPECT_TRUE(MatchAll(post_request, std::move(match_options), &responses));
  EXPECT_EQ(1u, responses.size());
}

TEST_P(CacheStorageCacheTestP, MatchAll_IgnoreVary) {
  body_request_->headers["vary_foo"] = "foo";
  blink::mojom::FetchAPIResponsePtr body_response = CreateBlobBodyResponse();
  body_response->headers["vary"] = "vary_foo";
  EXPECT_TRUE(Put(body_request_, std::move(body_response)));
  std::vector<blink::mojom::FetchAPIResponsePtr> responses;

  EXPECT_TRUE(MatchAll(body_request_, nullptr, &responses));
  EXPECT_EQ(1u, responses.size());
  body_request_->headers["vary_foo"] = "bar";

  EXPECT_TRUE(MatchAll(body_request_, nullptr, &responses));
  EXPECT_EQ(0u, responses.size());

  blink::mojom::CacheQueryOptionsPtr match_options =
      blink::mojom::CacheQueryOptions::New();
  match_options->ignore_vary = true;
  EXPECT_TRUE(MatchAll(body_request_, std::move(match_options), &responses));
  EXPECT_EQ(1u, responses.size());
}

TEST_P(CacheStorageCacheTestP, MatchAll_IgnoreSearch) {
  EXPECT_TRUE(Put(body_request_, CreateBlobBodyResponse()));
  EXPECT_TRUE(Put(body_request_with_query_, CreateBlobBodyResponseWithQuery()));
  EXPECT_TRUE(Put(no_body_request_, CreateNoBodyResponse()));

  std::vector<blink::mojom::FetchAPIResponsePtr> responses;
  blink::mojom::CacheQueryOptionsPtr match_options =
      blink::mojom::CacheQueryOptions::New();
  match_options->ignore_search = true;
  EXPECT_TRUE(MatchAll(body_request_, std::move(match_options), &responses));

  ASSERT_EQ(2u, responses.size());

  // Order of returned responses is not guaranteed.
  std::set<std::string> matched_set;
  for (const blink::mojom::FetchAPIResponsePtr& response : responses) {
    ASSERT_EQ(1u, response->url_list.size());
    if (response->url_list[0] == BodyUrlWithQuery()) {
      EXPECT_TRUE(ResponseMetadataEqual(
          *SetCacheName(CreateBlobBodyResponseWithQuery()), *response));
      matched_set.insert(response->url_list[0].spec());
    } else if (response->url_list[0] == BodyUrl()) {
      EXPECT_TRUE(ResponseMetadataEqual(*SetCacheName(CreateBlobBodyResponse()),
                                        *response));
      matched_set.insert(response->url_list[0].spec());
    }
  }
  EXPECT_EQ(2u, matched_set.size());
}

TEST_P(CacheStorageCacheTestP, MatchAll_Head) {
  EXPECT_TRUE(Put(body_request_, CreateBlobBodyResponse()));

  std::vector<blink::mojom::FetchAPIResponsePtr> responses;
  blink::mojom::CacheQueryOptionsPtr match_options =
      blink::mojom::CacheQueryOptions::New();
  match_options->ignore_search = true;
  EXPECT_TRUE(MatchAll(body_head_request_, match_options->Clone(), &responses));
  EXPECT_TRUE(responses.empty());

  match_options->ignore_method = true;
  EXPECT_TRUE(MatchAll(body_head_request_, match_options->Clone(), &responses));
  ASSERT_EQ(1u, responses.size());
  EXPECT_TRUE(ResponseMetadataEqual(*SetCacheName(CreateBlobBodyResponse()),
                                    *responses[0]));
  mojo::Remote<blink::mojom::Blob> blob(std::move(responses[0]->blob->blob));
  EXPECT_EQ(expected_blob_data_, storage::BlobToString(blob.get()));
}

TEST_P(CacheStorageCacheTestP, Vary) {
  body_request_->headers["vary_foo"] = "foo";
  blink::mojom::FetchAPIResponsePtr body_response = CreateBlobBodyResponse();
  body_response->headers["vary"] = "vary_foo";
  EXPECT_TRUE(Put(body_request_, std::move(body_response)));
  EXPECT_TRUE(Match(body_request_));

  body_request_->headers["vary_foo"] = "bar";
  EXPECT_FALSE(Match(body_request_));

  body_request_->headers.erase(std::string("vary_foo"));
  EXPECT_FALSE(Match(body_request_));
}

TEST_P(CacheStorageCacheTestP, EmptyVary) {
  blink::mojom::FetchAPIResponsePtr body_response = CreateBlobBodyResponse();
  body_response->headers["vary"] = "";
  EXPECT_TRUE(Put(body_request_, std::move(body_response)));
  EXPECT_TRUE(Match(body_request_));

  body_request_->headers["zoo"] = "zoo";
  EXPECT_TRUE(Match(body_request_));
}

TEST_P(CacheStorageCacheTestP, NoVaryButDiffHeaders) {
  EXPECT_TRUE(Put(body_request_, CreateBlobBodyResponse()));
  EXPECT_TRUE(Match(body_request_));

  body_request_->headers["zoo"] = "zoo";
  EXPECT_TRUE(Match(body_request_));
}

TEST_P(CacheStorageCacheTestP, VaryMultiple) {
  body_request_->headers["vary_foo"] = "foo";
  body_request_->headers["vary_bar"] = "bar";
  blink::mojom::FetchAPIResponsePtr body_response = CreateBlobBodyResponse();
  body_response->headers["vary"] = " vary_foo    , vary_bar";
  EXPECT_TRUE(Put(body_request_, std::move(body_response)));
  EXPECT_TRUE(Match(body_request_));

  body_request_->headers["vary_bar"] = "foo";
  EXPECT_FALSE(Match(body_request_));

  body_request_->headers.erase(std::string("vary_bar"));
  EXPECT_FALSE(Match(body_request_));
}

TEST_P(CacheStorageCacheTestP, VaryNewHeader) {
  body_request_->headers["vary_foo"] = "foo";
  blink::mojom::FetchAPIResponsePtr body_response = CreateBlobBodyResponse();
  body_response->headers["vary"] = " vary_foo, vary_bar";
  EXPECT_TRUE(Put(body_request_, std::move(body_response)));
  EXPECT_TRUE(Match(body_request_));

  body_request_->headers["vary_bar"] = "bar";
  EXPECT_FALSE(Match(body_request_));
}

TEST_P(CacheStorageCacheTestP, VaryStar) {
  blink::mojom::FetchAPIResponsePtr body_response = CreateBlobBodyResponse();
  body_response->headers["vary"] = "*";
  EXPECT_TRUE(Put(body_request_, std::move(body_response)));
  EXPECT_FALSE(Match(body_request_));
}

TEST_P(CacheStorageCacheTestP, EmptyKeys) {
  EXPECT_TRUE(Keys());
  EXPECT_EQ(0u, callback_strings_.size());
}

TEST_P(CacheStorageCacheTestP, TwoKeys) {
  EXPECT_TRUE(Put(no_body_request_, CreateNoBodyResponse()));
  EXPECT_TRUE(Put(body_request_, CreateBlobBodyResponse()));
  EXPECT_TRUE(Keys());
  std::vector<std::string> expected_keys{no_body_request_->url.spec(),
                                         body_request_->url.spec()};
  EXPECT_EQ(expected_keys, callback_strings_);
}

TEST_P(CacheStorageCacheTestP, TwoKeysThenOne) {
  EXPECT_TRUE(Put(no_body_request_, CreateNoBodyResponse()));
  EXPECT_TRUE(Put(body_request_, CreateBlobBodyResponse()));
  EXPECT_TRUE(Keys());
  std::vector<std::string> expected_keys{no_body_request_->url.spec(),
                                         body_request_->url.spec()};
  EXPECT_EQ(expected_keys, callback_strings_);

  EXPECT_TRUE(Delete(body_request_));
  EXPECT_TRUE(Keys());
  std::vector<std::string> expected_keys2{no_body_request_->url.spec()};
  EXPECT_EQ(expected_keys2, callback_strings_);
}

TEST_P(CacheStorageCacheTestP, KeysWithIgnoreSearchTrue) {
  EXPECT_TRUE(Put(no_body_request_, CreateNoBodyResponse()));
  EXPECT_TRUE(Put(body_request_, CreateBlobBodyResponse()));
  EXPECT_TRUE(Put(body_request_with_query_, CreateBlobBodyResponseWithQuery()));

  blink::mojom::CacheQueryOptionsPtr match_options =
      blink::mojom::CacheQueryOptions::New();
  match_options->ignore_search = true;

  EXPECT_TRUE(Keys(body_request_with_query_, std::move(match_options)));
  std::vector<std::string> expected_keys = {
      body_request_->url.spec(), body_request_with_query_->url.spec()};
  EXPECT_EQ(expected_keys, callback_strings_);
}

TEST_P(CacheStorageCacheTestP, KeysWithIgnoreSearchFalse) {
  EXPECT_TRUE(Put(no_body_request_, CreateNoBodyResponse()));
  EXPECT_TRUE(Put(body_request_, CreateBlobBodyResponse()));
  EXPECT_TRUE(Put(body_request_with_query_, CreateBlobBodyResponseWithQuery()));

  // Default value of ignore_search is false.
  blink::mojom::CacheQueryOptionsPtr match_options =
      blink::mojom::CacheQueryOptions::New();
  EXPECT_EQ(match_options->ignore_search, false);

  EXPECT_TRUE(Keys(body_request_with_query_, std::move(match_options)));
  std::vector<std::string> expected_keys = {
      body_request_with_query_->url.spec()};
  EXPECT_EQ(expected_keys, callback_strings_);
}

TEST_P(CacheStorageCacheTestP, DeleteNoBody) {
  EXPECT_TRUE(Put(no_body_request_, CreateNoBodyResponse()));
  EXPECT_TRUE(Match(no_body_request_));
  EXPECT_TRUE(Delete(no_body_request_));
  EXPECT_FALSE(Match(no_body_request_));
  EXPECT_FALSE(Delete(no_body_request_));
  EXPECT_TRUE(Put(no_body_request_, CreateNoBodyResponse()));
  EXPECT_TRUE(Match(no_body_request_));
  EXPECT_TRUE(Delete(no_body_request_));
}

TEST_P(CacheStorageCacheTestP, DeleteBody) {
  EXPECT_TRUE(Put(body_request_, CreateBlobBodyResponse()));
  EXPECT_TRUE(Match(body_request_));
  EXPECT_TRUE(Delete(body_request_));
  EXPECT_FALSE(Match(body_request_));
  EXPECT_FALSE(Delete(body_request_));
  EXPECT_TRUE(Put(body_request_, CreateBlobBodyResponse()));
  EXPECT_TRUE(Match(body_request_));
  EXPECT_TRUE(Delete(body_request_));
}

TEST_P(CacheStorageCacheTestP, DeleteWithIgnoreSearchTrue) {
  EXPECT_TRUE(Put(no_body_request_, CreateNoBodyResponse()));
  EXPECT_TRUE(Put(body_request_, CreateBlobBodyResponse()));
  EXPECT_TRUE(Put(body_request_with_query_, CreateBlobBodyResponseWithQuery()));

  EXPECT_TRUE(Keys());
  std::vector<std::string> expected_keys{no_body_request_->url.spec(),
                                         body_request_->url.spec(),
                                         body_request_with_query_->url.spec()};
  EXPECT_EQ(expected_keys, callback_strings_);

  // The following delete operation will remove both of body_request_ and
  // body_request_with_query_ from cache storage.
  blink::mojom::CacheQueryOptionsPtr match_options =
      blink::mojom::CacheQueryOptions::New();
  match_options->ignore_search = true;
  EXPECT_TRUE(Delete(body_request_with_query_, std::move(match_options)));

  EXPECT_TRUE(Keys());
  expected_keys.clear();
  std::vector<std::string> expected_keys2{no_body_request_->url.spec()};
  EXPECT_EQ(expected_keys2, callback_strings_);
}

TEST_P(CacheStorageCacheTestP, DeleteWithIgnoreSearchFalse) {
  EXPECT_TRUE(Put(no_body_request_, CreateNoBodyResponse()));
  EXPECT_TRUE(Put(body_request_, CreateBlobBodyResponse()));
  EXPECT_TRUE(Put(body_request_with_query_, CreateBlobBodyResponseWithQuery()));

  EXPECT_TRUE(Keys());
  std::vector<std::string> expected_keys{no_body_request_->url.spec(),
                                         body_request_->url.spec(),
                                         body_request_with_query_->url.spec()};
  EXPECT_EQ(expected_keys, callback_strings_);

  // Default value of ignore_search is false.
  blink::mojom::CacheQueryOptionsPtr match_options =
      blink::mojom::CacheQueryOptions::New();
  EXPECT_EQ(match_options->ignore_search, false);

  EXPECT_TRUE(Delete(body_request_with_query_, std::move(match_options)));

  EXPECT_TRUE(Keys());
  std::vector<std::string> expected_keys2{no_body_request_->url.spec(),
                                          body_request_->url.spec()};
  EXPECT_EQ(expected_keys2, callback_strings_);
}

TEST_P(CacheStorageCacheTestP, QuickStressNoBody) {
  for (int i = 0; i < 100; ++i) {
    EXPECT_FALSE(Match(no_body_request_));
    EXPECT_TRUE(Put(no_body_request_, CreateNoBodyResponse()));
    EXPECT_TRUE(Match(no_body_request_));
    EXPECT_TRUE(Delete(no_body_request_));
  }
}

TEST_P(CacheStorageCacheTestP, QuickStressBody) {
  for (int i = 0; i < 100; ++i) {
    ASSERT_FALSE(Match(body_request_));
    ASSERT_TRUE(Put(body_request_, CreateBlobBodyResponse()));
    ASSERT_TRUE(Match(body_request_));
    ASSERT_TRUE(Delete(body_request_));
  }
}

TEST_P(CacheStorageCacheTestP, PutResponseType) {
  EXPECT_TRUE(TestResponseType(network::mojom::FetchResponseType::kBasic));
  EXPECT_TRUE(TestResponseType(network::mojom::FetchResponseType::kCors));
  EXPECT_TRUE(TestResponseType(network::mojom::FetchResponseType::kDefault));
  EXPECT_TRUE(TestResponseType(network::mojom::FetchResponseType::kError));
  EXPECT_TRUE(TestResponseType(network::mojom::FetchResponseType::kOpaque));
  EXPECT_TRUE(
      TestResponseType(network::mojom::FetchResponseType::kOpaqueRedirect));
}

TEST_P(CacheStorageCacheTestP, PutWithSideData) {
  blink::mojom::FetchAPIResponsePtr response = CreateBlobBodyResponse();

  const std::string expected_side_data = "SideData";
  CopySideDataToResponse("blob-id:mysideblob", expected_side_data,
                         response.get());
  EXPECT_TRUE(Put(body_request_, std::move(response)));

  EXPECT_TRUE(Match(body_request_));
  ASSERT_TRUE(callback_response_->blob);
  mojo::Remote<blink::mojom::Blob> blob(
      std::move(callback_response_->blob->blob));
  EXPECT_EQ(expected_blob_data_, storage::BlobToString(blob.get()));
  EXPECT_EQ(expected_side_data, CopySideData(blob.get()));
}

TEST_P(CacheStorageCacheTestP, PutWithSideData_QuotaExceeded) {
  blink::mojom::FetchAPIResponsePtr response = CreateBlobBodyResponse();
  base::CheckedNumeric<uint64_t> safe_expected_entry_size =
      cache_->GetRequiredSafeSpaceForRequest(body_request_) +
      cache_->GetRequiredSafeSpaceForResponse(response);
  SetQuota(uint64_t{safe_expected_entry_size.ValueOrDie()} - 1);
  const std::string expected_side_data = "SideData";
  CopySideDataToResponse("blob-id:mysideblob", expected_side_data,
                         response.get());
  // When the available space is not enough for the body, Put operation must
  // fail.
  EXPECT_FALSE(Put(body_request_, std::move(response)));
  EXPECT_EQ(CacheStorageError::kErrorQuotaExceeded, callback_error_);
}

TEST_P(CacheStorageCacheTestP, PutWithSideData_QuotaExceededSkipSideData) {
  blink::mojom::FetchAPIResponsePtr response = CreateBlobBodyResponse();
  base::CheckedNumeric<uint64_t> safe_expected_entry_size =
      cache_->GetRequiredSafeSpaceForRequest(body_request_) +
      cache_->GetRequiredSafeSpaceForResponse(response);
  SetQuota(safe_expected_entry_size.ValueOrDie());
  const std::string expected_side_data = "SideData";
  CopySideDataToResponse("blob-id:mysideblob", expected_side_data,
                         response.get());
  // When the available space is enough for the body but not enough for the side
  // data, Put operation must succeed.
  EXPECT_TRUE(Put(body_request_, std::move(response)));

  EXPECT_TRUE(Match(body_request_));
  ASSERT_TRUE(callback_response_->blob);
  mojo::Remote<blink::mojom::Blob> blob(
      std::move(callback_response_->blob->blob));
  EXPECT_EQ(expected_blob_data_, storage::BlobToString(blob.get()));
  // The side data should not be written.
  EXPECT_EQ("", CopySideData(blob.get()));
}

TEST_P(CacheStorageCacheTestP, PutWithSideData_BadMessage) {
  base::HistogramTester histogram_tester;
  blink::mojom::FetchAPIResponsePtr response = CreateBlobBodyResponse();

  const std::string expected_side_data = "SideData";
  CopySideDataToResponse("blob-id:mysideblob", expected_side_data,
                         response.get());

  blink::mojom::BatchOperationPtr operation =
      blink::mojom::BatchOperation::New();
  operation->operation_type = blink::mojom::OperationType::kPut;
  operation->request = BackgroundFetchSettledFetch::CloneRequest(body_request_);
  operation->response = std::move(response);
  operation->response->side_data_blob_for_cache_put->size =
      std::numeric_limits<uint64_t>::max();

  std::vector<blink::mojom::BatchOperationPtr> operations;
  operations.emplace_back(std::move(operation));
  EXPECT_EQ(CacheStorageError::kErrorStorage,
            BatchOperation(std::move(operations)));
  histogram_tester.ExpectBucketCount(
      "ServiceWorkerCache.ErrorStorageType",
      ErrorStorageType::kBatchDidGetUsageAndQuotaInvalidSpace, 1);
  EXPECT_EQ("CSDH_UNEXPECTED_OPERATION", bad_message_reason_);

  EXPECT_FALSE(Match(body_request_));
}

TEST_P(CacheStorageCacheTestP, WriteSideData) {
  base::Time response_time(base::Time::Now());
  blink::mojom::FetchAPIResponsePtr response = CreateBlobBodyResponse();
  response->response_time = response_time;
  EXPECT_TRUE(Put(body_request_, std::move(response)));

  const std::string expected_side_data1 = "SideDataSample";
  scoped_refptr<net::IOBuffer> buffer1 =
      base::MakeRefCounted<net::StringIOBuffer>(expected_side_data1);
  EXPECT_TRUE(WriteSideData(body_request_->url, response_time, buffer1,
                            expected_side_data1.length()));

  EXPECT_TRUE(Match(body_request_));
  ASSERT_TRUE(callback_response_->blob);
  mojo::Remote<blink::mojom::Blob> blob1(
      std::move(callback_response_->blob->blob));
  EXPECT_EQ(expected_blob_data_, storage::BlobToString(blob1.get()));
  EXPECT_EQ(expected_side_data1, CopySideData(blob1.get()));

  const std::string expected_side_data2 = "New data";
  scoped_refptr<net::IOBuffer> buffer2 =
      base::MakeRefCounted<net::StringIOBuffer>(expected_side_data2);
  EXPECT_TRUE(WriteSideData(body_request_->url, response_time, buffer2,
                            expected_side_data2.length()));
  EXPECT_TRUE(Match(body_request_));
  ASSERT_TRUE(callback_response_->blob);
  mojo::Remote<blink::mojom::Blob> blob2(
      std::move(callback_response_->blob->blob));
  EXPECT_EQ(expected_blob_data_, storage::BlobToString(blob2.get()));
  EXPECT_EQ(expected_side_data2, CopySideData(blob2.get()));

  ASSERT_TRUE(Delete(body_request_));
}

TEST_P(CacheStorageCacheTestP, WriteSideData_QuotaExceeded) {
  SetQuota(1024 * 1023);
  base::Time response_time(base::Time::Now());
  blink::mojom::FetchAPIResponsePtr response(CreateNoBodyResponse());
  response->response_time = response_time;
  EXPECT_TRUE(Put(no_body_request_, std::move(response)));

  const size_t kSize = 1024 * 1024;
  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  memset(buffer->data(), 0, kSize);
  EXPECT_FALSE(
      WriteSideData(no_body_request_->url, response_time, buffer, kSize));
  EXPECT_EQ(CacheStorageError::kErrorQuotaExceeded, callback_error_);
  ASSERT_TRUE(Delete(no_body_request_));
}

TEST_P(CacheStorageCacheTestP, WriteSideData_QuotaManagerModified) {
  base::Time response_time(base::Time::Now());
  blink::mojom::FetchAPIResponsePtr response(CreateNoBodyResponse());
  response->response_time = response_time;
  EXPECT_EQ(0, quota_manager_proxy_->notify_bucket_modified_count());
  EXPECT_TRUE(Put(no_body_request_, std::move(response)));
  // Storage notification happens after the operation returns, so continue the
  // event loop.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, quota_manager_proxy_->notify_bucket_modified_count());

  const size_t kSize = 10;
  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  memset(buffer->data(), 0, kSize);
  EXPECT_TRUE(
      WriteSideData(no_body_request_->url, response_time, buffer, kSize));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, quota_manager_proxy_->notify_bucket_modified_count());
  ASSERT_TRUE(Delete(no_body_request_));
}

TEST_P(CacheStorageCacheTestP, WriteSideData_DifferentTimeStamp) {
  base::Time response_time(base::Time::Now());
  blink::mojom::FetchAPIResponsePtr response(CreateNoBodyResponse());
  response->response_time = response_time;
  EXPECT_TRUE(Put(no_body_request_, std::move(response)));

  const size_t kSize = 10;
  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  memset(buffer->data(), 0, kSize);
  EXPECT_FALSE(WriteSideData(no_body_request_->url,
                             response_time + base::Seconds(1), buffer, kSize));
  EXPECT_EQ(CacheStorageError::kErrorNotFound, callback_error_);
  ASSERT_TRUE(Delete(no_body_request_));
}

TEST_P(CacheStorageCacheTestP, WriteSideData_NotFound) {
  const size_t kSize = 10;
  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  memset(buffer->data(), 0, kSize);
  EXPECT_FALSE(WriteSideData(GURL("http://www.example.com/not_exist"),
                             base::Time::Now(), buffer, kSize));
  EXPECT_EQ(CacheStorageError::kErrorNotFound, callback_error_);
}

TEST_F(CacheStorageCacheTest, CaselessServiceWorkerFetchRequestHeaders) {
  // CacheStorageCache depends on blink::mojom::FetchAPIRequest having caseless
  // headers so that it can quickly lookup vary headers.
  auto request = blink::mojom::FetchAPIRequest::New();
  request->url = GURL("http://www.example.com");
  request->method = "GET";
  request->referrer = blink::mojom::Referrer::New();
  request->is_reload = false;

  request->headers["content-type"] = "foo";
  request->headers["Content-Type"] = "bar";
  EXPECT_EQ("bar", request->headers["content-type"]);
}

TEST_P(CacheStorageCacheTestP, QuotaManagerModified) {
  EXPECT_EQ(0, quota_manager_proxy_->notify_bucket_modified_count());

  EXPECT_TRUE(Put(no_body_request_, CreateNoBodyResponse()));
  // Storage notification happens after the operation returns, so continue the
  // event loop.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, quota_manager_proxy_->notify_bucket_modified_count());
  int64_t sum_delta =
      quota_manager_proxy_->last_notified_bucket_delta().value_or(0);
  EXPECT_LT(0, sum_delta);

  EXPECT_TRUE(Put(body_request_, CreateBlobBodyResponse()));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, quota_manager_proxy_->notify_bucket_modified_count());
  EXPECT_LT(sum_delta, *quota_manager_proxy_->last_notified_bucket_delta());
  sum_delta += *quota_manager_proxy_->last_notified_bucket_delta();

  EXPECT_TRUE(Delete(body_request_));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(3, quota_manager_proxy_->notify_bucket_modified_count());
  sum_delta += *quota_manager_proxy_->last_notified_bucket_delta();

  EXPECT_TRUE(Delete(no_body_request_));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(4, quota_manager_proxy_->notify_bucket_modified_count());
  sum_delta += *quota_manager_proxy_->last_notified_bucket_delta();

  EXPECT_EQ(0, sum_delta);
}

TEST_P(CacheStorageCacheTestP, PutObeysQuotaLimits) {
  SetQuota(10);
  EXPECT_FALSE(Put(body_request_, CreateBlobBodyResponse()));
  EXPECT_EQ(CacheStorageError::kErrorQuotaExceeded, callback_error_);
}

TEST_P(CacheStorageCacheTestP, PutObeysBucketQuotaLimits) {
  SetQuota(1000000);
  EXPECT_TRUE(Put(body_request_, CreateBlobBodyResponse()));

  const auto storage_key =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(kTestUrl));
  base::test::TestFuture<storage::QuotaErrorOr<storage::BucketInfo>> future;
  storage::BucketInitParams bucket(storage_key, "inbox");
  bucket.quota = 15;
  quota_manager_proxy_->UpdateOrCreateBucket(
      bucket, base::SingleThreadTaskRunner::GetCurrentDefault(),
      future.GetCallback());
  ASSERT_OK_AND_ASSIGN(storage::BucketInfo value, future.Take());
  InitCache(nullptr, value.ToBucketLocator());

  EXPECT_FALSE(Put(body_request_, CreateBlobBodyResponse()));
  EXPECT_EQ(CacheStorageError::kErrorQuotaExceeded, callback_error_);
}

TEST_P(CacheStorageCacheTestP, PutObeysQuotaLimitsWithEmptyResponse) {
  blink::mojom::FetchAPIResponsePtr response = CreateBlobBodyResponse();
  base::CheckedNumeric<uint64_t> safe_expected_entry_size =
      cache_->GetRequiredSafeSpaceForRequest(body_request_) +
      cache_->GetRequiredSafeSpaceForResponse(response);
  SetQuota(safe_expected_entry_size.ValueOrDie());

  // The first Put will completely fill the quota, leaving no space for the
  // second operation, which will fail even with empty response, due to the size
  // of the headers.
  EXPECT_TRUE(Put(body_request_, std::move(response)));
  EXPECT_FALSE(Put(no_body_request_, CreateNoBodyResponse()));
  EXPECT_EQ(CacheStorageError::kErrorQuotaExceeded, callback_error_);
}

TEST_P(CacheStorageCacheTestP, PutSafeSpaceIsEnough) {
  blink::mojom::FetchAPIResponsePtr response = CreateBlobBodyResponse();
  base::CheckedNumeric<uint64_t> safe_expected_entry_size =
      cache_->GetRequiredSafeSpaceForRequest(body_request_) +
      cache_->GetRequiredSafeSpaceForResponse(response);
  SetQuota(safe_expected_entry_size.ValueOrDie());

  EXPECT_TRUE(Put(body_request_, std::move(response)));
}

TEST_P(CacheStorageCacheTestP, PutRequestUrlObeysQuotaLimits) {
  const GURL url("http://example.com/body.html");
  const GURL longerUrl("http://example.com/longer-body.html");
  blink::mojom::FetchAPIRequestPtr request = CreateFetchAPIRequest(
      url, "GET", kHeaders, blink::mojom::Referrer::New(), false);
  blink::mojom::FetchAPIResponsePtr response = CreateBlobBodyResponse();
  base::CheckedNumeric<uint64_t> safe_expected_entry_size =
      cache_->GetRequiredSafeSpaceForRequest(request) +
      cache_->GetRequiredSafeSpaceForResponse(response);
  SetQuota(safe_expected_entry_size.ValueOrDie());

  request->url = longerUrl;
  EXPECT_FALSE(Put(request, std::move(response)));
  EXPECT_EQ(CacheStorageError::kErrorQuotaExceeded, callback_error_);
}

TEST_P(CacheStorageCacheTestP, PutRequestMethodObeysQuotaLimits) {
  blink::mojom::FetchAPIRequestPtr request = CreateFetchAPIRequest(
      BodyUrl(), "GET", kHeaders, blink::mojom::Referrer::New(), false);
  blink::mojom::FetchAPIResponsePtr response = CreateBlobBodyResponse();
  base::CheckedNumeric<uint64_t> safe_expected_entry_size =
      cache_->GetRequiredSafeSpaceForRequest(request) +
      cache_->GetRequiredSafeSpaceForResponse(response);
  SetQuota(safe_expected_entry_size.ValueOrDie());

  request->method = "LongerMethodThanGet";
  EXPECT_FALSE(Put(request, std::move(response)));
  EXPECT_EQ(CacheStorageError::kErrorQuotaExceeded, callback_error_);
}

TEST_P(CacheStorageCacheTestP, PutRequestHeadersObeyQuotaLimits) {
  blink::mojom::FetchAPIRequestPtr request = CreateFetchAPIRequest(
      BodyUrl(), "GET", kHeaders, blink::mojom::Referrer::New(), false);
  blink::mojom::FetchAPIResponsePtr response = CreateBlobBodyResponse();
  base::CheckedNumeric<uint64_t> safe_expected_entry_size =
      cache_->GetRequiredSafeSpaceForRequest(request) +
      cache_->GetRequiredSafeSpaceForResponse(response);
  SetQuota(safe_expected_entry_size.ValueOrDie());

  request->headers["New-Header"] = "foo";
  EXPECT_FALSE(Put(request, std::move(response)));
  EXPECT_EQ(CacheStorageError::kErrorQuotaExceeded, callback_error_);
}

TEST_P(CacheStorageCacheTestP, PutResponseStatusObeysQuotaLimits) {
  blink::mojom::FetchAPIResponsePtr response = CreateBlobBodyResponse();
  base::CheckedNumeric<uint64_t> safe_expected_entry_size =
      cache_->GetRequiredSafeSpaceForRequest(body_request_) +
      cache_->GetRequiredSafeSpaceForResponse(response);
  SetQuota(safe_expected_entry_size.ValueOrDie());

  response->status_text = "LongerThanOK";
  EXPECT_FALSE(Put(body_request_, std::move(response)));
  EXPECT_EQ(CacheStorageError::kErrorQuotaExceeded, callback_error_);
}

TEST_P(CacheStorageCacheTestP, PutResponseBlobObeysQuotaLimits) {
  blink::mojom::FetchAPIResponsePtr response = CreateBlobBodyResponse();
  base::CheckedNumeric<uint64_t> safe_expected_entry_size =
      cache_->GetRequiredSafeSpaceForRequest(body_request_) +
      cache_->GetRequiredSafeSpaceForResponse(response);
  SetQuota(safe_expected_entry_size.ValueOrDie());

  response->blob->size += 1;
  EXPECT_FALSE(Put(body_request_, std::move(response)));
  EXPECT_EQ(CacheStorageError::kErrorQuotaExceeded, callback_error_);
}

TEST_P(CacheStorageCacheTestP, PutResponseHeadersObeyQuotaLimits) {
  blink::mojom::FetchAPIResponsePtr response = CreateBlobBodyResponse();
  base::CheckedNumeric<uint64_t> safe_expected_entry_size =
      cache_->GetRequiredSafeSpaceForRequest(body_request_) +
      cache_->GetRequiredSafeSpaceForResponse(response);
  SetQuota(safe_expected_entry_size.ValueOrDie());

  response->headers["New-Header"] = "foo";
  EXPECT_FALSE(Put(body_request_, std::move(response)));
  EXPECT_EQ(CacheStorageError::kErrorQuotaExceeded, callback_error_);
}

TEST_P(CacheStorageCacheTestP, PutResponseCorsHeadersObeyQuotaLimits) {
  blink::mojom::FetchAPIResponsePtr response = CreateBlobBodyResponse();
  base::CheckedNumeric<uint64_t> safe_expected_entry_size =
      cache_->GetRequiredSafeSpaceForRequest(body_request_) +
      cache_->GetRequiredSafeSpaceForResponse(response);
  SetQuota(safe_expected_entry_size.ValueOrDie());

  response->cors_exposed_header_names.push_back("AnotherOne");
  EXPECT_FALSE(Put(body_request_, std::move(response)));
  EXPECT_EQ(CacheStorageError::kErrorQuotaExceeded, callback_error_);
}

TEST_P(CacheStorageCacheTestP, PutResponseUrlListObeysQuotaLimits) {
  blink::mojom::FetchAPIResponsePtr response = CreateBlobBodyResponse();
  base::CheckedNumeric<uint64_t> safe_expected_entry_size =
      cache_->GetRequiredSafeSpaceForRequest(body_request_) +
      cache_->GetRequiredSafeSpaceForResponse(response);
  SetQuota(safe_expected_entry_size.ValueOrDie());

  response->url_list.emplace_back("http://example.com/another-url");
  EXPECT_FALSE(Put(body_request_, std::move(response)));
  EXPECT_EQ(CacheStorageError::kErrorQuotaExceeded, callback_error_);
}

TEST_P(CacheStorageCacheTestP, PutObeysQuotaLimitsWithEmptyResponseTinyQuota) {
  SetQuota(1);
  EXPECT_FALSE(Put(body_request_, CreateNoBodyResponse()));
  EXPECT_EQ(CacheStorageError::kErrorQuotaExceeded, callback_error_);
}

TEST_P(CacheStorageCacheTestP, Size) {
  EXPECT_EQ(0, Size());
  EXPECT_TRUE(Put(no_body_request_, CreateNoBodyResponse()));
  EXPECT_LT(0, Size());
  int64_t no_body_size = Size();

  EXPECT_TRUE(Delete(no_body_request_));
  EXPECT_EQ(0, Size());

  EXPECT_TRUE(Put(body_request_, CreateBlobBodyResponse()));
  EXPECT_LT(no_body_size, Size());

  EXPECT_TRUE(Delete(body_request_));
  EXPECT_EQ(0, Size());
}

TEST_F(CacheStorageCacheTest, VerifyOpaqueSizePadding) {
  base::Time response_time(base::Time::Now());

  blink::mojom::FetchAPIRequestPtr non_opaque_request =
      BackgroundFetchSettledFetch::CloneRequest(body_request_);
  non_opaque_request->url = GURL("http://example.com/no-pad.html");
  blink::mojom::FetchAPIResponsePtr non_opaque_response =
      CreateBlobBodyResponse();
  non_opaque_response->response_time = response_time;
  EXPECT_TRUE(Put(non_opaque_request, std::move(non_opaque_response)));
  int64_t unpadded_no_data_cache_size = Size();

  // Now write some side data to that cache.
  const std::string expected_side_data(2048, 'X');
  scoped_refptr<net::IOBuffer> side_data_buffer =
      base::MakeRefCounted<net::StringIOBuffer>(expected_side_data);
  EXPECT_TRUE(WriteSideData(non_opaque_request->url, response_time,
                            side_data_buffer, expected_side_data.length()));
  int64_t unpadded_total_resource_size = Size();
  int64_t unpadded_side_data_size =
      unpadded_total_resource_size - unpadded_no_data_cache_size;
  EXPECT_EQ(expected_side_data.size(),
            static_cast<size_t>(unpadded_side_data_size));
  blink::mojom::FetchAPIResponsePtr non_opaque_response_clone =
      CreateBlobBodyResponse();
  non_opaque_response_clone->response_time = response_time;

  // Now write an identically sized opaque response.
  blink::mojom::FetchAPIRequestPtr opaque_request =
      BackgroundFetchSettledFetch::CloneRequest(non_opaque_request);
  opaque_request->url = GURL("http://example.com/opaque.html");
  // Same URL length means same cache sizes (ignoring padding).
  EXPECT_EQ(opaque_request->url.spec().length(),
            non_opaque_request->url.spec().length());
  blink::mojom::FetchAPIResponsePtr opaque_response(CreateOpaqueResponse());
  opaque_response->response_time = response_time;

  EXPECT_TRUE(Put(opaque_request, std::move(opaque_response)));
  int64_t size_after_opaque_put = Size();
  int64_t opaque_padding = size_after_opaque_put -
                           2 * unpadded_no_data_cache_size -
                           unpadded_side_data_size;
  ASSERT_GT(opaque_padding, 0);

  // Now write side data and expect to see the padding change.
  EXPECT_TRUE(WriteSideData(opaque_request->url, response_time,
                            side_data_buffer, expected_side_data.length()));
  int64_t current_padding = Size() - 2 * unpadded_total_resource_size;
  EXPECT_NE(opaque_padding, current_padding);

  // Now reset opaque side data back to zero.
  const std::string expected_side_data2;
  scoped_refptr<net::IOBuffer> buffer2 =
      base::MakeRefCounted<net::StringIOBuffer>(expected_side_data2);
  EXPECT_TRUE(WriteSideData(opaque_request->url, response_time, buffer2,
                            expected_side_data2.length()));
  EXPECT_EQ(size_after_opaque_put, Size());

  // And delete the opaque response entirely.
  EXPECT_TRUE(Delete(opaque_request));
  EXPECT_EQ(unpadded_total_resource_size, Size());
}

TEST_F(CacheStorageCacheTest, TestDifferentOpaqueSideDataSizes) {
  blink::mojom::FetchAPIRequestPtr request =
      BackgroundFetchSettledFetch::CloneRequest(body_request_);
  blink::mojom::FetchAPIResponsePtr response(CreateOpaqueResponse());

  auto response_time = response->response_time;

  EXPECT_TRUE(Put(request, std::move(response)));
  int64_t opaque_cache_size_no_side_data = Size();

  const std::string small_side_data(1024, 'X');
  scoped_refptr<net::IOBuffer> buffer1 =
      base::MakeRefCounted<net::StringIOBuffer>(small_side_data);
  EXPECT_TRUE(WriteSideData(request->url, response_time, buffer1,
                            small_side_data.length()));
  int64_t opaque_cache_size_with_side_data = Size();
  EXPECT_NE(opaque_cache_size_with_side_data, opaque_cache_size_no_side_data);

  // Write side data of a different size. The padding should change.
  const std::string large_side_data(2048, 'X');
  EXPECT_NE(large_side_data.length(), small_side_data.length());
  scoped_refptr<net::IOBuffer> buffer2 =
      base::MakeRefCounted<net::StringIOBuffer>(large_side_data);
  EXPECT_TRUE(WriteSideData(request->url, response_time, buffer2,
                            large_side_data.length()));
  int side_data_delta = large_side_data.length() - small_side_data.length();
  EXPECT_NE(opaque_cache_size_with_side_data + side_data_delta, Size());
}

TEST_F(CacheStorageCacheTest, TestDoubleOpaquePut) {
  blink::mojom::FetchAPIRequestPtr request =
      BackgroundFetchSettledFetch::CloneRequest(body_request_);

  base::Time response_time(base::Time::Now());

  blink::mojom::FetchAPIResponsePtr response(CreateOpaqueResponse());
  response->response_time = response_time;
  EXPECT_TRUE(Put(request, std::move(response)));
  int64_t size_after_first_put = Size();

  blink::mojom::FetchAPIRequestPtr request2 =
      BackgroundFetchSettledFetch::CloneRequest(body_request_);
  blink::mojom::FetchAPIResponsePtr response2(CreateOpaqueResponse());
  response2->response_time = response_time;
  EXPECT_TRUE(Put(request2, std::move(response2)));

  EXPECT_EQ(size_after_first_put, Size());
}

TEST_P(CacheStorageCacheTestP, GetSizeThenClose) {
  // Create the backend and put something in it.
  EXPECT_TRUE(Put(body_request_, CreateBlobBodyResponse()));
  // Get a reference to the response in the cache.
  EXPECT_TRUE(Match(body_request_));
  mojo::Remote<blink::mojom::Blob> blob(
      std::move(callback_response_->blob->blob));
  callback_response_ = nullptr;

  int64_t cache_size = Size();
  EXPECT_EQ(cache_size, GetSizeThenClose());
  VerifyAllOpsFail();

  // Reading blob should fail.
  EXPECT_EQ("", storage::BlobToString(blob.get()));
}

TEST_P(CacheStorageCacheTestP, OpsFailOnClosedBackend) {
  // Create the backend and put something in it.
  EXPECT_TRUE(Put(body_request_, CreateBlobBodyResponse()));
  EXPECT_TRUE(Close());
  VerifyAllOpsFail();
}

// Shutdown the cache in the middle of its writing the response body. Upon
// restarting, that response shouldn't be available. See crbug.com/617683.
TEST_P(CacheStorageCacheTestP, UnfinishedPutsShouldNotBeReusable) {
  // Create a response with a blob that takes forever to write its bytes to the
  // mojo pipe. Guaranteeing that the response isn't finished writing by the
  // time we close the backend.
  base::RunLoop run_loop;
  auto blob = blink::mojom::SerializedBlob::New();
  blob->uuid = "mock blob";
  blob->size = 100;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<SlowBlob>(run_loop.QuitClosure()),
      blob->blob.InitWithNewPipeAndPassReceiver());
  blink::mojom::FetchAPIResponsePtr response = CreateNoBodyResponse();
  response->url_list = {BodyUrl()};
  response->blob = std::move(blob);

  blink::mojom::BatchOperationPtr operation =
      blink::mojom::BatchOperation::New();
  operation->operation_type = blink::mojom::OperationType::kPut;
  operation->request = BackgroundFetchSettledFetch::CloneRequest(body_request_);
  operation->response = std::move(response);
  std::vector<blink::mojom::BatchOperationPtr> operations;
  operations.emplace_back(std::move(operation));

  // Start the put operation and let it run until the blob is supposed to write
  // to its pipe.
  cache_->BatchOperation(std::move(operations), /* trace_id = */ 0,
                         base::DoNothing(), base::DoNothing());
  run_loop.Run();

  // Shut down the cache. Doing so causes the write to cease, and the entry
  // should be erased.
  cache_ = nullptr;
  base::RunLoop().RunUntilIdle();

  // Create a new Cache in the same space.
  ASSERT_OK_AND_ASSIGN(auto bucket, GetOrCreateDefaultBucket(kTestUrl));
  InitCache(nullptr, std::move(bucket));

  // Now attempt to read the same response from the cache. It should fail.
  EXPECT_FALSE(Match(body_request_));
}

TEST_P(CacheStorageCacheTestP, BlobReferenceDelaysClose) {
  // Create the backend and put something in it.
  EXPECT_TRUE(Put(body_request_, CreateBlobBodyResponse()));
  // Get a reference to the response in the cache.
  EXPECT_TRUE(Match(body_request_));
  mojo::Remote<blink::mojom::Blob> blob(
      std::move(callback_response_->blob->blob));
  callback_response_ = nullptr;

  base::RunLoop loop;
  cache_->Close(base::BindOnce(&CacheStorageCacheTest::CloseCallback,
                               base::Unretained(this),
                               base::Unretained(&loop)));
  task_environment_.RunUntilIdle();
  // If MemoryOnly closing does succeed right away.
  EXPECT_EQ(MemoryOnly(), callback_closed_);

  // Reading blob should succeed.
  EXPECT_EQ(expected_blob_data_, storage::BlobToString(blob.get()));
  blob.reset();

  loop.Run();
  EXPECT_TRUE(callback_closed_);
}

TEST_P(CacheStorageCacheTestP, VerifySerialScheduling) {
  // Start two operations, the first one is delayed but the second isn't. The
  // second should wait for the first.
  EXPECT_TRUE(Keys());  // Opens the backend.
  DelayableBackend* delayable_backend = cache_->UseDelayableBackend();
  delayable_backend->set_delay_open_entry(true);

  base::RunLoop open_started_loop;
  delayable_backend->set_open_entry_started_callback(
      open_started_loop.QuitClosure());

  int sequence_out = -1;

  blink::mojom::BatchOperationPtr operation1 =
      blink::mojom::BatchOperation::New();
  operation1->operation_type = blink::mojom::OperationType::kPut;
  operation1->request =
      BackgroundFetchSettledFetch::CloneRequest(body_request_);
  operation1->response = CreateBlobBodyResponse();

  std::unique_ptr<base::RunLoop> close_loop1(new base::RunLoop());
  std::vector<blink::mojom::BatchOperationPtr> operations1;
  operations1.emplace_back(std::move(operation1));
  cache_->BatchOperation(
      std::move(operations1), /* trace_id = */ 0,
      base::BindOnce(&CacheStorageCacheTest::SequenceCallback,
                     base::Unretained(this), 1, &sequence_out,
                     close_loop1.get()),
      CacheStorageCache::BadMessageCallback());

  // Wait until the first operation attempts to open the entry and becomes
  // delayed.
  open_started_loop.Run();

  blink::mojom::BatchOperationPtr operation2 =
      blink::mojom::BatchOperation::New();
  operation2->operation_type = blink::mojom::OperationType::kPut;
  operation2->request =
      BackgroundFetchSettledFetch::CloneRequest(body_request_);
  operation2->response = CreateBlobBodyResponse();

  delayable_backend->set_delay_open_entry(false);
  std::unique_ptr<base::RunLoop> close_loop2(new base::RunLoop());
  std::vector<blink::mojom::BatchOperationPtr> operations2;
  operations2.emplace_back(std::move(operation2));
  cache_->BatchOperation(
      std::move(operations2), /* trace_id = */ 0,
      base::BindOnce(&CacheStorageCacheTest::SequenceCallback,
                     base::Unretained(this), 2, &sequence_out,
                     close_loop2.get()),
      CacheStorageCache::BadMessageCallback());

  // The second put operation should wait for the first to complete.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(-1, sequence_out);

  EXPECT_TRUE(delayable_backend->OpenEntryContinue());
  close_loop1->Run();
  EXPECT_EQ(1, sequence_out);
  close_loop2->Run();
  EXPECT_EQ(2, sequence_out);
}

#if BUILDFLAG(IS_WIN)
// TODO(crbug.com/41443751): Flaky on Windows.
#define MAYBE_KeysWithManyCacheEntries DISABLED_KeysWithManyCacheEntries
#else
#define MAYBE_KeysWithManyCacheEntries KeysWithManyCacheEntries
#endif
TEST_P(CacheStorageCacheTestP, MAYBE_KeysWithManyCacheEntries) {
  // Use a smaller list in disk mode to reduce test runtime.
  const int kNumEntries = MemoryOnly() ? 1000 : 250;

  std::vector<std::string> expected_keys;
  for (int i = 0; i < kNumEntries; ++i) {
    GURL url =
        net::AppendQueryParameter(NoBodyUrl(), "n", base::NumberToString(i));
    expected_keys.push_back(url.spec());
    blink::mojom::FetchAPIRequestPtr request = CreateFetchAPIRequest(
        url, "GET", kHeaders, blink::mojom::Referrer::New(), false);
    EXPECT_TRUE(Put(request, CreateNoBodyResponse()));
  }

  EXPECT_TRUE(Keys());
  EXPECT_EQ(expected_keys.size(), callback_strings_.size());
  EXPECT_EQ(expected_keys, callback_strings_);
}

TEST_P(CacheStorageCacheTestP, SelfRefsDuringMatch) {
  EXPECT_TRUE(Put(body_request_, CreateBlobBodyResponse()));

  // When there are no operations outstanding and we're not holding an
  // explicit reference the cache should consider itself unreferenced.
  EXPECT_TRUE(cache_->IsUnreferenced());

  DelayableBackend* delayable_backend = cache_->UseDelayableBackend();
  delayable_backend->set_delay_open_entry(true);

  std::unique_ptr<base::RunLoop> loop(new base::RunLoop());
  cache_->Match(CopyFetchRequest(body_request_), /* match_options = */ nullptr,
                CacheStorageSchedulerPriority::kNormal, /* trace_id = */ 0,
                base::BindOnce(&CacheStorageCacheTest::ResponseAndErrorCallback,
                               base::Unretained(this), loop.get()));

  // Blocks on opening the cache entry.
  base::RunLoop().RunUntilIdle();

  // Since an operation is outstanding the cache should consider itself
  // referenced.
  EXPECT_FALSE(cache_->IsUnreferenced());

  // Allow the operation to continue.
  EXPECT_TRUE(delayable_backend->OpenEntryContinue());
  loop->Run();

  // The operation should succeed.
  EXPECT_EQ(CacheStorageError::kSuccess, callback_error_);
}

TEST_P(CacheStorageCacheTestP, SelfRefsDuringMatchAll) {
  EXPECT_TRUE(Put(body_request_, CreateBlobBodyResponse()));

  // When there are no operations outstanding and we're not holding an
  // explicit reference the cache should consider itself unreferenced.
  EXPECT_TRUE(cache_->IsUnreferenced());

  DelayableBackend* delayable_backend = cache_->UseDelayableBackend();
  delayable_backend->set_delay_open_entry(true);

  std::vector<blink::mojom::FetchAPIResponsePtr> responses;

  std::unique_ptr<base::RunLoop> loop(new base::RunLoop());
  cache_->MatchAll(
      CopyFetchRequest(body_request_), /* match_options = */ nullptr,
      /* trace_id = */ 0,
      base::BindOnce(&CacheStorageCacheTest::ResponsesAndErrorCallback,
                     base::Unretained(this), loop->QuitClosure(), &responses));

  // Blocks on opening the cache entry.
  base::RunLoop().RunUntilIdle();

  // Since an operation is outstanding the cache should consider itself
  // referenced.
  EXPECT_FALSE(cache_->IsUnreferenced());

  // Allow the operation to continue.
  EXPECT_TRUE(delayable_backend->OpenEntryContinue());
  loop->Run();

  // The operation should succeed.
  EXPECT_EQ(CacheStorageError::kSuccess, callback_error_);
  EXPECT_EQ(1u, responses.size());
}

TEST_P(CacheStorageCacheTestP, SelfRefsDuringWriteSideData) {
  base::Time response_time(base::Time::Now());
  blink::mojom::FetchAPIResponsePtr response = CreateBlobBodyResponse();
  response->response_time = response_time;
  EXPECT_TRUE(Put(body_request_, std::move(response)));

  // When there are no operations outstanding and we're not holding an
  // explicit reference the cache should consider itself unreferenced.
  EXPECT_TRUE(cache_->IsUnreferenced());

  DelayableBackend* delayable_backend = cache_->UseDelayableBackend();
  delayable_backend->set_delay_open_entry(true);

  const std::string expected_side_data = "SideDataSample";
  scoped_refptr<net::IOBuffer> buffer =
      base::MakeRefCounted<net::StringIOBuffer>(expected_side_data);

  std::unique_ptr<base::RunLoop> loop(new base::RunLoop());
  cache_->WriteSideData(
      base::BindOnce(&CacheStorageCacheTest::ErrorTypeCallback,
                     base::Unretained(this), base::Unretained(loop.get())),
      BodyUrl(), response_time, /* trace_id = */ 0, buffer,
      expected_side_data.length());

  // Blocks on opening the cache entry.
  base::RunLoop().RunUntilIdle();

  // Since an operation is outstanding the cache should consider itself
  // referenced.
  EXPECT_FALSE(cache_->IsUnreferenced());

  // Allow the operation to continue.
  EXPECT_TRUE(delayable_backend->OpenEntryContinue());
  loop->Run();

  // The operation should succeed.
  EXPECT_EQ(CacheStorageError::kSuccess, callback_error_);
}

TEST_P(CacheStorageCacheTestP, SelfRefsDuringBatchOperation) {
  // Open the backend
  EXPECT_TRUE(Keys());

  blink::mojom::BatchOperationPtr operation =
      blink::mojom::BatchOperation::New();
  operation->operation_type = blink::mojom::OperationType::kPut;
  operation->request = BackgroundFetchSettledFetch::CloneRequest(body_request_);
  operation->request->url = GURL("http://example.com/1");
  operation->response = CreateBlobBodyResponse();
  operation->response->url_list.emplace_back("http://example.com/1");

  std::vector<blink::mojom::BatchOperationPtr> operations;
  operations.push_back(std::move(operation));

  // When there are no operations outstanding and we're not holding an
  // explicit reference the cache should consider itself unreferenced.
  EXPECT_TRUE(cache_->IsUnreferenced());

  DelayableBackend* delayable_backend = cache_->UseDelayableBackend();
  delayable_backend->set_delay_open_entry(true);

  std::unique_ptr<base::RunLoop> loop(new base::RunLoop());
  cache_->BatchOperation(
      std::move(operations), /* trace_id = */ 0,
      base::BindOnce(&CacheStorageCacheTest::VerboseErrorTypeCallback,
                     base::Unretained(this), base::Unretained(loop.get())),
      base::BindOnce(&OnBadMessage, base::Unretained(&bad_message_reason_)));

  // Blocks on opening the cache entry.
  base::RunLoop().RunUntilIdle();

  // Since an operation is outstanding the cache should consider itself
  // referenced.
  EXPECT_FALSE(cache_->IsUnreferenced());

  // Allow the operation to continue.
  EXPECT_TRUE(delayable_backend->OpenEntryContinue());
  loop->Run();

  // The operation should succeed.
  EXPECT_EQ(CacheStorageError::kSuccess, callback_error_);
}

TEST_P(CacheStorageCacheTestP, SelfRefsDuringKeys) {
  EXPECT_TRUE(Put(body_request_, CreateBlobBodyResponse()));

  // When there are no operations outstanding and we're not holding an
  // explicit reference the cache should consider itself unreferenced.
  EXPECT_TRUE(cache_->IsUnreferenced());

  DelayableBackend* delayable_backend = cache_->UseDelayableBackend();
  delayable_backend->set_delay_open_entry(true);

  std::unique_ptr<base::RunLoop> loop(new base::RunLoop());
  cache_->Keys(
      BackgroundFetchSettledFetch::CloneRequest(body_request_),
      /* match_options = */ nullptr,
      /* trace_id = */ 0,
      base::BindOnce(&CacheStorageCacheTest::RequestsCallback,
                     base::Unretained(this), base::Unretained(loop.get())));

  // Blocks on opening the cache entry.
  base::RunLoop().RunUntilIdle();

  // Since an operation is outstanding the cache should consider itself
  // referenced.
  EXPECT_FALSE(cache_->IsUnreferenced());

  // Allow the operation to continue.
  EXPECT_TRUE(delayable_backend->OpenEntryContinue());
  loop->Run();

  // The operation should succeed.
  EXPECT_EQ(CacheStorageError::kSuccess, callback_error_);
}

TEST_P(CacheStorageCacheTestP, SelfRefsDuringPut) {
  // Open the backend
  EXPECT_TRUE(Keys());

  // When there are no operations outstanding and we're not holding an
  // explicit reference the cache should consider itself unreferenced.
  EXPECT_TRUE(cache_->IsUnreferenced());

  DelayableBackend* delayable_backend = cache_->UseDelayableBackend();
  delayable_backend->set_delay_open_entry(true);

  std::unique_ptr<base::RunLoop> loop(new base::RunLoop());
  cache_->Put(
      BackgroundFetchSettledFetch::CloneRequest(body_request_),
      CreateBlobBodyResponse(), /* trace_id = */ 0,
      base::BindOnce(&CacheStorageCacheTest::ErrorTypeCallback,
                     base::Unretained(this), base::Unretained(loop.get())));

  // Blocks on opening the cache entry.
  base::RunLoop().RunUntilIdle();

  // Since an operation is outstanding the cache should consider itself
  // referenced.
  EXPECT_FALSE(cache_->IsUnreferenced());

  // Allow the operation to continue.
  EXPECT_TRUE(delayable_backend->OpenEntryContinue());
  loop->Run();

  // The operation should succeed.
  EXPECT_EQ(CacheStorageError::kSuccess, callback_error_);
}

TEST_P(CacheStorageCacheTestP, PutFailCreateEntry) {
  // Open the backend.
  EXPECT_TRUE(Keys());
  std::unique_ptr<base::RunLoop> run_loop;
  cache_->UseFailableBackend(FailableBackend::FailureStage::CREATE_ENTRY);
  cache_->Put(
      BackgroundFetchSettledFetch::CloneRequest(body_request_),
      CreateBlobBodyResponse(), /* trace_id = */ 0,
      base::BindOnce(&CacheStorageCacheTest::ErrorTypeCallback,
                     base::Unretained(this), base::Unretained(run_loop.get())));

  // Blocks on opening the cache entry.
  base::RunLoop().RunUntilIdle();

  // The operation should fail.
  EXPECT_EQ(CacheStorageError::kErrorExists, callback_error_);

  // QuotaManager should have been notified of write failures.
  ASSERT_EQ(1U, mock_quota_manager_->write_error_tracker().size());
  EXPECT_EQ(1,
            /*error_count*/ mock_quota_manager_->write_error_tracker()
                .begin()
                ->second);

  EXPECT_FALSE(Put(body_request_, CreateBlobBodyResponse()));
  ASSERT_EQ(1U, mock_quota_manager_->write_error_tracker().size());
  EXPECT_EQ(2,
            /*error_count*/ mock_quota_manager_->write_error_tracker()
                .begin()
                ->second);
}

TEST_P(CacheStorageCacheTestP, PutFailWriteHeaders) {
  // Only interested in quota being notified for disk write errors
  // as opposed to errors from a memory only scenario.
  if (MemoryOnly()) {
    return;
  }

  // Open the backend.
  EXPECT_TRUE(Keys());

  std::unique_ptr<base::RunLoop> run_loop;
  cache_->UseFailableBackend(FailableBackend::FailureStage::WRITE_HEADERS);

  EXPECT_FALSE(Put(body_request_, CreateBlobBodyResponse()));
  // Blocks on opening the cache entry.
  base::RunLoop().RunUntilIdle();

  // The operation should fail.
  EXPECT_EQ(CacheStorageError::kErrorStorage, callback_error_);

  // QuotaManager should have been notified of write failures.
  ASSERT_EQ(1U, mock_quota_manager_->write_error_tracker().size());
  EXPECT_EQ(1,
            /*error_count*/ mock_quota_manager_->write_error_tracker()
                .begin()
                ->second);

  EXPECT_FALSE(Put(body_request_, CreateBlobBodyResponse()));
  ASSERT_EQ(1U, mock_quota_manager_->write_error_tracker().size());
  EXPECT_EQ(2,
            /*error_count*/ mock_quota_manager_->write_error_tracker()
                .begin()
                ->second);
}

INSTANTIATE_TEST_SUITE_P(CacheStorageCacheTest,
                         CacheStorageCacheTestP,
                         ::testing::Values(false, true));
}  // namespace cache_storage_cache_unittest
}  // namespace content
