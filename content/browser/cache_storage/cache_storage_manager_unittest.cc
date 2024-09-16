// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cache_storage/cache_storage_manager.h"

#include <stddef.h>
#include <stdint.h>

#include <cstdint>
#include <list>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "components/services/storage/public/cpp/buckets/bucket_info.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "components/services/storage/public/cpp/buckets/constants.h"
#include "components/services/storage/public/cpp/constants.h"
#include "components/services/storage/public/cpp/quota_error_or.h"
#include "components/services/storage/public/mojom/cache_storage_control.mojom.h"
#include "components/services/storage/public/mojom/storage_usage_info.mojom.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/cache_storage/cache_storage.h"
#include "content/browser/cache_storage/cache_storage.pb.h"
#include "content/browser/cache_storage/cache_storage_cache_handle.h"
#include "content/browser/cache_storage/cache_storage_context_impl.h"
#include "content/browser/cache_storage/cache_storage_quota_client.h"
#include "content/browser/cache_storage/cache_storage_scheduler.h"
#include "content/common/background_fetch/background_fetch_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/features.h"
#include "net/base/schemeful_site.h"
#include "net/disk_cache/disk_cache.h"
#include "net/http/http_connection_info.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/quota/quota_client_type.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/fake_blob.h"
#include "storage/browser/test/mock_quota_manager_proxy.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "storage/common/quota/padding_key.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/cache_storage/cache_storage.mojom.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

using blink::mojom::CacheStorageError;
using blink::mojom::CacheStorageVerboseErrorPtr;
using network::mojom::FetchResponseType;

namespace content {
namespace cache_storage_manager_unittest {

enum class TestStorage {
  kDisk,
  kMemory,
};

enum class StorageKeyAndBucketTestCase {
  kFirstPartyDefault,
  kFirstPartyNamed,
  kFirstPartyDefaultPartitionEnabled,
  kFirstPartyNamedPartitionEnabled,
  kThirdPartyDefault,
  kThirdPartyNamed,
};

using blink::mojom::StorageType;
using ResponseHeaderMap = base::flat_map<std::string, std::string>;

class DelayedBlob : public storage::FakeBlob {
 public:
  DelayedBlob(mojo::PendingReceiver<blink::mojom::Blob> receiver,
              std::string data,
              base::OnceClosure read_closure)
      : FakeBlob("foo"),
        receiver_(this, std::move(receiver)),
        data_(std::move(data)),
        read_closure_(std::move(read_closure)) {}

  void Resume() {
    paused_ = false;
    MaybeComplete();
  }

  void ReadAll(
      mojo::ScopedDataPipeProducerHandle producer_handle,
      mojo::PendingRemote<blink::mojom::BlobReaderClient> client) override {
    client_.Bind(std::move(client));
    producer_handle_ = std::move(producer_handle);

    client_->OnCalculatedSize(data_.length(), data_.length());

    // This should always succeed immediately because we size the pipe to
    // hold the entire blob for tiny data lengths.
    size_t actually_written_bytes = 0;
    producer_handle_->WriteData(base::as_byte_span(data_),
                                MOJO_WRITE_DATA_FLAG_NONE,
                                actually_written_bytes);
    ASSERT_EQ(data_.length(), actually_written_bytes);

    // Signal that ReadAll() was called.
    std::move(read_closure_).Run();

    MaybeComplete();
  }

 private:
  void MaybeComplete() {
    if (paused_ || !client_) {
      return;
    }
    client_->OnComplete(net::OK, data_.length());
    client_.reset();
    producer_handle_.reset();
  }

  mojo::Receiver<blink::mojom::Blob> receiver_;
  std::string data_;
  base::OnceClosure read_closure_;
  mojo::Remote<blink::mojom::BlobReaderClient> client_;
  mojo::ScopedDataPipeProducerHandle producer_handle_;
  bool paused_ = true;
};

// Scheduler implementation that will invoke a callback after the
// next operation has been started.
class CallbackScheduler : public CacheStorageScheduler {
 public:
  explicit CallbackScheduler(base::OnceClosure callback)
      : CacheStorageScheduler(
            CacheStorageSchedulerClient::kCache,
            base::SingleThreadTaskRunner::GetCurrentDefault()),
        callback_(std::move(callback)) {}

 protected:
  void DispatchOperationTask(base::OnceClosure task) override {
    auto wrapped = base::BindOnce(&CallbackScheduler::ExecuteTask,
                                  base::Unretained(this), std::move(task));
    CacheStorageScheduler::DispatchOperationTask(std::move(wrapped));
  }

 private:
  void ExecuteTask(base::OnceClosure task) {
    std::move(task).Run();
    if (callback_) {
      std::move(callback_).Run();
    }
  }

  base::OnceClosure callback_;
};

class MockCacheStorageQuotaManagerProxy
    : public storage::MockQuotaManagerProxy {
 public:
  MockCacheStorageQuotaManagerProxy(storage::MockQuotaManager* quota_manager,
                                    base::SingleThreadTaskRunner* task_runner)
      : MockQuotaManagerProxy(quota_manager, task_runner) {}

  void RegisterClient(
      mojo::PendingRemote<storage::mojom::QuotaClient> client,
      storage::QuotaClientType client_type,
      const base::flat_set<blink::mojom::StorageType>& storage_types) override {
    registered_clients_.emplace_back(std::move(client));
  }

 private:
  ~MockCacheStorageQuotaManagerProxy() override = default;

  std::vector<mojo::Remote<storage::mojom::QuotaClient>> registered_clients_;
};

bool IsIndexFileCurrent(const base::FilePath& cache_dir) {
  base::File::Info info;
  const base::FilePath index_path =
      cache_dir.AppendASCII(CacheStorage::kIndexFileName);
  if (!GetFileInfo(index_path, &info)) {
    return false;
  }
  base::Time index_last_modified = info.last_modified;

  base::FileEnumerator enumerator(cache_dir, false,
                                  base::FileEnumerator::DIRECTORIES);
  for (base::FilePath file_path = enumerator.Next(); !file_path.empty();
       file_path = enumerator.Next()) {
    if (!GetFileInfo(file_path, &info)) {
      return false;
    }
    if (index_last_modified <= info.last_modified) {
      return false;
    }
  }

  return true;
}

class TestCacheStorageObserver : public storage::mojom::CacheStorageObserver {
 public:
  explicit TestCacheStorageObserver(
      mojo::PendingReceiver<storage::mojom::CacheStorageObserver> observer)
      : receiver_(this, std::move(observer)),
        loop_(std::make_unique<base::RunLoop>()) {}

  void OnCacheListChanged(
      const storage::BucketLocator& bucket_locator) override {
    ++notify_list_changed_count;
    loop_->Quit();
  }

  void OnCacheContentChanged(const storage::BucketLocator& bucket_locator,
                             const std::string& cache_name) override {
    ++notify_content_changed_count;
    loop_->Quit();
  }

  void Wait() {
    loop_->Run();
    loop_ = std::make_unique<base::RunLoop>();
  }

  base::OnceClosure callback;
  int notify_list_changed_count = 0;
  int notify_content_changed_count = 0;

  mojo::Receiver<storage::mojom::CacheStorageObserver> receiver_;
  std::unique_ptr<base::RunLoop> loop_;
};

class CacheStorageManagerTest : public testing::Test {
 public:
  CacheStorageManagerTest()
      : task_environment_(BrowserTaskEnvironment::IO_MAINLOOP),
        blob_storage_context_(nullptr),
        storage_key1_(blink::StorageKey::CreateFromStringForTesting(
            "http://example1.com")),
        storage_key2_(blink::StorageKey::CreateFromStringForTesting(
            "http://example2.com")) {}

  CacheStorageManagerTest(const CacheStorageManagerTest&) = delete;
  CacheStorageManagerTest& operator=(const CacheStorageManagerTest&) = delete;

  void SetUp() override {
    base::FilePath temp_dir_path;
    if (!MemoryOnly()) {
      ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    }

    CreateStorageManager();
  }

  void TearDown() override {
    DestroyStorageManager();
    disk_cache::FlushCacheThreadForTesting();
    content::RunAllTasksUntilIdle();
  }

  virtual bool MemoryOnly() { return false; }

  void BoolCallback(base::RunLoop* run_loop, bool value) {
    callback_bool_ = value;
    run_loop->Quit();
  }

  void ErrorCallback(base::RunLoop* run_loop, CacheStorageError error) {
    callback_error_ = error;
    callback_bool_ = error == CacheStorageError::kSuccess;
    run_loop->Quit();
  }

  void BoolAndErrorCallback(base::RunLoop* run_loop,
                            bool value,
                            CacheStorageError error) {
    callback_bool_ = value;
    callback_error_ = error;
    run_loop->Quit();
  }

  void CacheAndErrorCallback(base::RunLoop* run_loop,
                             CacheStorageCacheHandle cache_handle,
                             CacheStorageError error) {
    callback_cache_handle_ = std::move(cache_handle);
    callback_error_ = error;
    run_loop->Quit();
  }

  void CacheMetadataCallback(base::RunLoop* run_loop,
                             std::vector<std::string> cache_names) {
    cache_names_ = std::move(cache_names);
    run_loop->Quit();
  }

  const std::string& GetFirstIndexName() const { return cache_names_.front(); }

  std::vector<std::string> GetIndexNames() const { return cache_names_; }

  void CachePutCallback(base::RunLoop* run_loop,
                        CacheStorageVerboseErrorPtr error) {
    callback_error_ = error->value;
    run_loop->Quit();
  }

  void CacheDeleteCallback(base::RunLoop* run_loop,
                           CacheStorageVerboseErrorPtr error) {
    callback_error_ = error->value;
    run_loop->Quit();
  }

  void CacheMatchCallback(base::RunLoop* run_loop,
                          CacheStorageError error,
                          blink::mojom::FetchAPIResponsePtr response) {
    callback_error_ = error;
    callback_cache_handle_response_ = std::move(response);
    run_loop->Quit();
  }

  void CacheMatchAllCallback(base::RunLoop* run_loop,
                             CacheStorageError error,
                             std::vector<blink::mojom::FetchAPIResponsePtr>) {
    callback_error_ = error;
    run_loop->Quit();
  }

  std::unique_ptr<TestCacheStorageObserver> CreateObserver() {
    DCHECK(cache_manager_);
    mojo::PendingRemote<storage::mojom::CacheStorageObserver> remote;
    auto observer = std::make_unique<TestCacheStorageObserver>(
        remote.InitWithNewPipeAndPassReceiver());
    cache_manager_->AddObserver(std::move(remote));
    return observer;
  }

  void CreateStorageManager() {
    ChromeBlobStorageContext* blob_storage_context(
        ChromeBlobStorageContext::GetFor(&browser_context_));
    // Wait for ChromeBlobStorageContext to finish initializing.
    base::RunLoop().RunUntilIdle();

    mojo::PendingRemote<storage::mojom::BlobStorageContext> remote;
    blob_storage_context->BindMojoContext(
        remote.InitWithNewPipeAndPassReceiver());
    blob_storage_context_ =
        base::MakeRefCounted<BlobStorageContextWrapper>(std::move(remote));

    base::FilePath temp_dir_path;
    if (!MemoryOnly()) {
      temp_dir_path = temp_dir_.GetPath();
    }

    quota_policy_ = base::MakeRefCounted<storage::MockSpecialStoragePolicy>();
    mock_quota_manager_ = base::MakeRefCounted<storage::MockQuotaManager>(
        MemoryOnly(), temp_dir_path,
        base::SingleThreadTaskRunner::GetCurrentDefault().get(),
        quota_policy_.get());
    mock_quota_manager_->SetQuota(storage_key1_, StorageType::kTemporary,
                                  1024 * 1024 * 100);
    mock_quota_manager_->SetQuota(storage_key2_, StorageType::kTemporary,
                                  1024 * 1024 * 100);

    quota_manager_proxy_ =
        base::MakeRefCounted<MockCacheStorageQuotaManagerProxy>(
            mock_quota_manager_.get(),
            base::SingleThreadTaskRunner::GetCurrentDefault().get());

    // These must be instantiated after `quota_manager_proxy_` has been
    // initialized.
    ASSERT_OK_AND_ASSIGN(
        bucket_locator1_,
        GetOrCreateBucket(storage_key1_, storage::kDefaultBucketName));
    ASSERT_OK_AND_ASSIGN(
        bucket_locator2_,
        GetOrCreateBucket(storage_key2_, storage::kDefaultBucketName));

    cache_manager_ = CacheStorageManager::Create(
        temp_dir_path, base::SingleThreadTaskRunner::GetCurrentDefault(),
        base::SingleThreadTaskRunner::GetCurrentDefault(), quota_manager_proxy_,
        blob_storage_context_, nullptr);
  }

  void RecreateStorageManager() {
    DCHECK(cache_manager_);
    auto* legacy_manager =
        static_cast<CacheStorageManager*>(cache_manager_.get());
    cache_manager_ = CacheStorageManager::CreateForTesting(legacy_manager);
  }

  bool FlushCacheStorageIndex(const storage::BucketLocator& bucket_locator) {
    callback_bool_ = false;
    base::RunLoop loop;
    auto* impl = CacheStorage::From(CacheStorageForBucket(bucket_locator));
    bool write_was_scheduled = impl->InitiateScheduledIndexWriteForTest(
        base::BindOnce(&CacheStorageManagerTest::BoolCallback,
                       base::Unretained(this), &loop));
    loop.Run();
    DCHECK(callback_bool_);
    return write_was_scheduled;
  }

  void DestroyStorageManager() {
    callback_cache_handle_ = CacheStorageCacheHandle();
    callback_bool_ = false;
    callback_cache_handle_response_ = nullptr;
    cache_names_.clear();

    base::RunLoop().RunUntilIdle();
    quota_manager_proxy_ = nullptr;

    blob_storage_context_ = nullptr;

    quota_policy_ = nullptr;
    mock_quota_manager_ = nullptr;
    // Note: After the MockQuotaManager goes away, the buckets get destroyed as
    // well. We won't clear `bucket_locator1_` or `bucket_locator2_`, though,
    // for cases where it's useful to compare the bucket locator values to
    // values written to the Cache Storage index files.

    cache_manager_ = nullptr;
  }

  void CheckOpHistograms(base::HistogramTester& histogram_tester,
                         const char* op_name) {
    std::string base("ServiceWorkerCache.CacheStorage.Scheduler.");
    histogram_tester.ExpectTotalCount(base + "OperationDuration2." + op_name,
                                      1);
    histogram_tester.ExpectTotalCount(base + "QueueDuration2." + op_name, 1);
    histogram_tester.ExpectTotalCount(base + "QueueLength." + op_name, 1);
  }

  bool Open(const storage::BucketLocator& bucket_locator,
            const std::string& cache_name,
            storage::mojom::CacheStorageOwner owner =
                storage::mojom::CacheStorageOwner::kCacheAPI) {
    base::HistogramTester histogram_tester;
    base::RunLoop loop;
    CacheStorageHandle cache_storage =
        cache_manager_->OpenCacheStorage(bucket_locator, owner);
    cache_storage.value()->OpenCache(
        cache_name, /* trace_id = */ 0,
        base::BindOnce(&CacheStorageManagerTest::CacheAndErrorCallback,
                       base::Unretained(this), base::Unretained(&loop)));
    loop.Run();

    bool error = callback_error_ != CacheStorageError::kSuccess;
    if (error) {
      EXPECT_FALSE(callback_cache_handle_.value());
    } else {
      EXPECT_TRUE(callback_cache_handle_.value());
      CheckOpHistograms(histogram_tester, "Open");
    }
    return !error;
  }

  bool Has(const storage::BucketLocator& bucket_locator,
           const std::string& cache_name,
           storage::mojom::CacheStorageOwner owner =
               storage::mojom::CacheStorageOwner::kCacheAPI) {
    base::HistogramTester histogram_tester;
    base::RunLoop loop;
    CacheStorageHandle cache_storage =
        cache_manager_->OpenCacheStorage(bucket_locator, owner);
    cache_storage.value()->HasCache(
        cache_name, /* trace_id = */ 0,
        base::BindOnce(&CacheStorageManagerTest::BoolAndErrorCallback,
                       base::Unretained(this), base::Unretained(&loop)));
    loop.Run();
    CheckOpHistograms(histogram_tester, "Has");
    return callback_bool_;
  }

  bool Delete(const storage::BucketLocator& bucket_locator,
              const std::string& cache_name,
              storage::mojom::CacheStorageOwner owner =
                  storage::mojom::CacheStorageOwner::kCacheAPI) {
    base::HistogramTester histogram_tester;
    base::RunLoop loop;
    CacheStorageHandle cache_storage =
        cache_manager_->OpenCacheStorage(bucket_locator, owner);
    cache_storage.value()->DoomCache(
        cache_name, /* trace_id = */ 0,
        base::BindOnce(&CacheStorageManagerTest::ErrorCallback,
                       base::Unretained(this), base::Unretained(&loop)));
    loop.Run();
    CheckOpHistograms(histogram_tester, "Delete");
    return callback_bool_;
  }

  size_t Keys(const storage::BucketLocator& bucket_locator,
              storage::mojom::CacheStorageOwner owner =
                  storage::mojom::CacheStorageOwner::kCacheAPI) {
    base::HistogramTester histogram_tester;
    base::RunLoop loop;
    CacheStorageHandle cache_storage =
        cache_manager_->OpenCacheStorage(bucket_locator, owner);
    cache_storage.value()->EnumerateCaches(
        /* trace_id = */ 0,
        base::BindOnce(&CacheStorageManagerTest::CacheMetadataCallback,
                       base::Unretained(this), base::Unretained(&loop)));
    loop.Run();
    CheckOpHistograms(histogram_tester, "Keys");
    return cache_names_.size();
  }

  bool StorageMatch(const storage::BucketLocator& bucket_locator,
                    const std::string& cache_name,
                    const GURL& url,
                    blink::mojom::CacheQueryOptionsPtr match_options = nullptr,
                    storage::mojom::CacheStorageOwner owner =
                        storage::mojom::CacheStorageOwner::kCacheAPI) {
    auto request = blink::mojom::FetchAPIRequest::New();
    request->url = url;
    return StorageMatchWithRequest(bucket_locator, cache_name,
                                   std::move(request), std::move(match_options),
                                   owner);
  }

  bool StorageMatchWithRequest(
      const storage::BucketLocator& bucket_locator,
      const std::string& cache_name,
      blink::mojom::FetchAPIRequestPtr request,
      blink::mojom::CacheQueryOptionsPtr match_options = nullptr,
      storage::mojom::CacheStorageOwner owner =
          storage::mojom::CacheStorageOwner::kCacheAPI) {
    base::HistogramTester histogram_tester;
    base::RunLoop loop;
    CacheStorageHandle cache_storage =
        cache_manager_->OpenCacheStorage(bucket_locator, owner);
    cache_storage.value()->MatchCache(
        cache_name, std::move(request), std::move(match_options),
        CacheStorageSchedulerPriority::kNormal, /* trace_id = */ 0,
        base::BindOnce(&CacheStorageManagerTest::CacheMatchCallback,
                       base::Unretained(this), base::Unretained(&loop)));
    loop.Run();
    if (callback_error_ == CacheStorageError::kSuccess) {
      CheckOpHistograms(histogram_tester, "Match");
    }
    return callback_error_ == CacheStorageError::kSuccess;
  }

  bool StorageMatchAll(
      const storage::BucketLocator& bucket_locator,
      const GURL& url,
      blink::mojom::CacheQueryOptionsPtr match_options = nullptr) {
    auto request = blink::mojom::FetchAPIRequest::New();
    request->url = url;
    return StorageMatchAllWithRequest(bucket_locator, std::move(request),
                                      std::move(match_options));
  }

  bool StorageMatchAllWithRequest(
      const storage::BucketLocator& bucket_locator,
      blink::mojom::FetchAPIRequestPtr request,
      blink::mojom::CacheQueryOptionsPtr match_options = nullptr,
      storage::mojom::CacheStorageOwner owner =
          storage::mojom::CacheStorageOwner::kCacheAPI) {
    base::HistogramTester histogram_tester;
    base::RunLoop loop;
    CacheStorageHandle cache_storage =
        cache_manager_->OpenCacheStorage(bucket_locator, owner);
    cache_storage.value()->MatchAllCaches(
        std::move(request), std::move(match_options),
        CacheStorageSchedulerPriority::kNormal, /* trace_id = */ 0,
        base::BindOnce(&CacheStorageManagerTest::CacheMatchCallback,
                       base::Unretained(this), base::Unretained(&loop)));
    loop.Run();
    if (callback_error_ == CacheStorageError::kSuccess) {
      CheckOpHistograms(histogram_tester, "MatchAll");
    }
    return callback_error_ == CacheStorageError::kSuccess;
  }

  bool Write(const storage::BucketLocator& bucket_locator,
             storage::mojom::CacheStorageOwner owner,
             const std::string& cache_name,
             const std::string& request_url) {
    auto request = blink::mojom::FetchAPIRequest::New();
    request->url = GURL(request_url);

    base::RunLoop loop;
    CacheStorageHandle cache_storage =
        cache_manager_->OpenCacheStorage(bucket_locator, owner);
    cache_storage.value()->WriteToCache(
        cache_name, std::move(request), blink::mojom::FetchAPIResponse::New(),
        /* trace_id = */ 0,
        base::BindOnce(&CacheStorageManagerTest::ErrorCallback,
                       base::Unretained(this), base::Unretained(&loop)));
    loop.Run();
    return callback_bool_;
  }

  bool CachePut(CacheStorageCache* cache,
                const GURL& url,
                FetchResponseType response_type = FetchResponseType::kDefault) {
    auto request = blink::mojom::FetchAPIRequest::New();
    request->url = url;

    return CachePutWithStatusCode(cache, std::move(request), 200,
                                  response_type);
  }

  bool CachePutWithRequestAndHeaders(
      CacheStorageCache* cache,
      blink::mojom::FetchAPIRequestPtr request,
      ResponseHeaderMap response_headers,
      FetchResponseType response_type = FetchResponseType::kDefault) {
    return CachePutWithStatusCode(cache, std::move(request), 200, response_type,
                                  std::move(response_headers));
  }

  bool CachePutWithStatusCode(
      CacheStorageCache* cache,
      blink::mojom::FetchAPIRequestPtr request,
      int status_code,
      FetchResponseType response_type = FetchResponseType::kDefault,
      ResponseHeaderMap response_headers = ResponseHeaderMap()) {
    std::string blob_uuid = base::Uuid::GenerateRandomV4().AsLowercaseString();

    auto blob = blink::mojom::SerializedBlob::New();
    blob->uuid = blob_uuid;
    blob->size = request->url.spec().size();
    auto& str = request->url.spec();
    blob_storage_context_->context()->RegisterFromMemory(
        blob->blob.InitWithNewPipeAndPassReceiver(), blob_uuid,
        std::vector<uint8_t>(str.begin(), str.end()));

    base::RunLoop loop;
    CachePutWithStatusCodeAndBlobInternal(cache, std::move(request),
                                          status_code, std::move(blob), &loop,
                                          response_type, response_headers);
    loop.Run();

    return callback_error_ == CacheStorageError::kSuccess;
  }

  void CachePutWithStatusCodeAndBlobInternal(
      CacheStorageCache* cache,
      blink::mojom::FetchAPIRequestPtr request,
      int status_code,
      blink::mojom::SerializedBlobPtr blob,
      base::RunLoop* loop,
      FetchResponseType response_type = FetchResponseType::kDefault,
      ResponseHeaderMap response_headers = ResponseHeaderMap()) {
    // CacheStorage depends on fetch to provide the opaque response padding
    // value now.  We prepolute a padding value here to simulate that.
    int64_t padding = response_type == FetchResponseType::kOpaque ? 10 : 0;

    auto response = blink::mojom::FetchAPIResponse::New(
        std::vector<GURL>({request->url}), status_code, "OK", response_type,
        padding, network::mojom::FetchResponseSource::kUnspecified,
        response_headers, /*mime_type=*/std::nullopt,
        net::HttpRequestHeaders::kGetMethod, std::move(blob),
        blink::mojom::ServiceWorkerResponseError::kUnknown, base::Time(),
        /*cache_storage_cache_name=*/std::string(),
        /*cors_exposed_header_names=*/std::vector<std::string>(),
        /*side_data_blob=*/nullptr,
        /*side_data_blob_for_cache_put=*/nullptr,
        network::mojom::ParsedHeaders::New(), net::HttpConnectionInfo::kUNKNOWN,
        /*alpn_negotiated_protocol=*/"unknown",
        /*was_fetched_via_spdy=*/false, /*has_range_requested=*/false,
        /*auth_challenge_info=*/std::nullopt,
        /*request_include_credentials=*/true);

    blink::mojom::BatchOperationPtr operation =
        blink::mojom::BatchOperation::New();
    operation->operation_type = blink::mojom::OperationType::kPut;
    operation->request = std::move(request);
    operation->response = std::move(response);

    std::vector<blink::mojom::BatchOperationPtr> operations;
    operations.emplace_back(std::move(operation));
    cache->BatchOperation(
        std::move(operations), /* trace_id = */ 0,
        base::BindOnce(&CacheStorageManagerTest::CachePutCallback,
                       base::Unretained(this), base::Unretained(loop)),
        CacheStorageCache::BadMessageCallback());
  }

  bool CacheDelete(CacheStorageCache* cache, const GURL& url) {
    auto request = blink::mojom::FetchAPIRequest::New();
    request->url = url;

    blink::mojom::BatchOperationPtr operation =
        blink::mojom::BatchOperation::New();
    operation->operation_type = blink::mojom::OperationType::kDelete;
    operation->request = std::move(request);
    operation->response = blink::mojom::FetchAPIResponse::New();

    std::vector<blink::mojom::BatchOperationPtr> operations;
    operations.emplace_back(std::move(operation));
    base::RunLoop loop;
    cache->BatchOperation(
        std::move(operations), /* trace_id = */ 0,
        base::BindOnce(&CacheStorageManagerTest::CacheDeleteCallback,
                       base::Unretained(this), base::Unretained(&loop)),
        CacheStorageCache::BadMessageCallback());
    loop.Run();

    return callback_error_ == CacheStorageError::kSuccess;
  }

  bool CacheMatch(CacheStorageCache* cache, const GURL& url) {
    auto request = blink::mojom::FetchAPIRequest::New();
    request->url = url;
    base::RunLoop loop;
    cache->Match(
        std::move(request), nullptr, CacheStorageSchedulerPriority::kNormal,
        /* trace_id = */ 0,
        base::BindOnce(&CacheStorageManagerTest::CacheMatchCallback,
                       base::Unretained(this), base::Unretained(&loop)));
    loop.Run();

    return callback_error_ == CacheStorageError::kSuccess;
  }

  CacheStorageHandle CacheStorageForBucket(
      const storage::BucketLocator& bucket_locator) {
    return cache_manager_->OpenCacheStorage(
        bucket_locator, storage::mojom::CacheStorageOwner::kCacheAPI);
  }

  int64_t GetBucketUsage(const storage::BucketLocator& bucket_locator,
                         storage::mojom::CacheStorageOwner owner =
                             storage::mojom::CacheStorageOwner::kCacheAPI) {
    base::RunLoop loop;
    cache_manager_->GetBucketUsage(
        bucket_locator, owner,
        base::BindOnce(&CacheStorageManagerTest::UsageCallback,
                       base::Unretained(this), base::Unretained(&loop)));
    loop.Run();
    return callback_usage_;
  }

  void UsageCallback(base::RunLoop* run_loop, int64_t usage) {
    callback_usage_ = usage;
    run_loop->Quit();
  }

  const std::vector<blink::StorageKey> GetStorageKeys(
      storage::mojom::CacheStorageOwner owner =
          storage::mojom::CacheStorageOwner::kCacheAPI) {
    base::test::TestFuture<const std::vector<::blink::StorageKey>&> future;
    cache_manager_->GetStorageKeys(owner, future.GetCallback());
    return future.Get();
  }

  blink::mojom::QuotaStatusCode DeleteOriginData(
      const std::set<url::Origin>& origins,
      storage::mojom::CacheStorageOwner owner =
          storage::mojom::CacheStorageOwner::kCacheAPI) {
    base::test::TestFuture<::blink::mojom::QuotaStatusCode> future;
    cache_manager_->DeleteOriginData(origins, owner, future.GetCallback());
    return future.Get();
  }

  blink::mojom::QuotaStatusCode DeleteBucketData(
      const storage::BucketLocator& bucket_locator,
      storage::mojom::CacheStorageOwner owner =
          storage::mojom::CacheStorageOwner::kCacheAPI) {
    base::test::TestFuture<::blink::mojom::QuotaStatusCode> future;
    cache_manager_->DeleteBucketData(bucket_locator, owner,
                                     future.GetCallback());
    return future.Get();
  }

  int64_t GetSizeThenCloseAllCaches(
      const storage::BucketLocator& bucket_locator) {
    base::RunLoop loop;
    CacheStorageHandle cache_storage = CacheStorageForBucket(bucket_locator);
    CacheStorage::From(cache_storage)
        ->GetSizeThenCloseAllCaches(
            base::BindOnce(&CacheStorageManagerTest::UsageCallback,
                           base::Unretained(this), &loop));
    loop.Run();
    return callback_usage_;
  }

  int64_t Size(const storage::BucketLocator& bucket_locator) {
    base::RunLoop loop;
    CacheStorageHandle cache_storage = CacheStorageForBucket(bucket_locator);
    CacheStorage::From(cache_storage)
        ->Size(base::BindOnce(&CacheStorageManagerTest::UsageCallback,
                              base::Unretained(this), &loop));
    loop.Run();
    return callback_usage_;
  }

  int64_t GetQuotaKeyUsage(const blink::StorageKey& storage_key) {
    int64_t usage(CacheStorage::kSizeUnknown);
    base::RunLoop loop;
    quota_manager_proxy_->GetUsageAndQuota(
        storage_key, StorageType::kTemporary,
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        base::BindOnce(&CacheStorageManagerTest::DidGetQuotaOriginUsage,
                       base::Unretained(this), base::Unretained(&usage),
                       &loop));
    loop.Run();
    return usage;
  }

  void DidGetQuotaOriginUsage(int64_t* out_usage,
                              base::RunLoop* run_loop,
                              blink::mojom::QuotaStatusCode status_code,
                              int64_t usage,
                              int64_t quota) {
    if (status_code == blink::mojom::QuotaStatusCode::kOk) {
      *out_usage = usage;
    }
    run_loop->Quit();
  }

  storage::QuotaErrorOr<storage::BucketLocator> GetOrCreateBucket(
      const blink::StorageKey& storage_key,
      const std::string& name) {
    base::test::TestFuture<storage::QuotaErrorOr<storage::BucketInfo>> future;
    quota_manager_proxy_->UpdateOrCreateBucket(
        name == storage::kDefaultBucketName
            ? storage::BucketInitParams::ForDefaultBucket(storage_key)
            : storage::BucketInitParams(storage_key, name),
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        future.GetCallback());
    return future.Take().transform(&storage::BucketInfo::ToBucketLocator);
  }

 protected:
  // Temporary directory must be allocated first so as to be destroyed last.
  base::ScopedTempDir temp_dir_;

  BrowserTaskEnvironment task_environment_;
  TestBrowserContext browser_context_;
  scoped_refptr<BlobStorageContextWrapper> blob_storage_context_;

  scoped_refptr<storage::MockSpecialStoragePolicy> quota_policy_;
  scoped_refptr<storage::MockQuotaManager> mock_quota_manager_;
  scoped_refptr<MockCacheStorageQuotaManagerProxy> quota_manager_proxy_;
  scoped_refptr<CacheStorageManager> cache_manager_;

  CacheStorageCacheHandle callback_cache_handle_;
  int callback_bool_ = false;
  CacheStorageError callback_error_ = CacheStorageError::kSuccess;
  blink::mojom::FetchAPIResponsePtr callback_cache_handle_response_;
  std::vector<std::string> cache_names_;

  blink::StorageKey storage_key1_;
  blink::StorageKey storage_key2_;
  storage::BucketLocator bucket_locator1_;
  storage::BucketLocator bucket_locator2_;

  int64_t callback_usage_;
};

class CacheStorageManagerMemoryOnlyTest : public CacheStorageManagerTest {
 public:
  bool MemoryOnly() override { return true; }
};

class CacheStorageManagerTestP
    : public CacheStorageManagerTest,
      public testing::WithParamInterface<TestStorage> {
 public:
  bool MemoryOnly() override { return GetParam() == TestStorage::kMemory; }
};

class CacheStorageManagerStorageKeyAndBucketTestP
    : public CacheStorageManagerTest,
      public testing::WithParamInterface<StorageKeyAndBucketTestCase> {
 public:
  void SetUp() override {
    CacheStorageManagerTest::SetUp();

    test_case_ = GetParam();

    if (ThirdPartyStoragePartitioningEnabled()) {
      scoped_feature_list_.InitAndEnableFeature(
          net::features::kThirdPartyStoragePartitioning);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          net::features::kThirdPartyStoragePartitioning);
    }

    ReinitializeStorageKeysAndBucketLocators();
  }

  void CreateStorageManager() {
    // Always reset `storage_key1_` and `storage_key2_` to what they
    // are expected to be in CacheStorageManagerTest so that when we
    // reinitialize below the bucket IDs of the new `bucket_locator1_`
    // and `bucket_locator2_` will be the same regardless of how many
    // times `DestroyStorageManager()` and then `CreateStorageManager()`
    // are called.
    storage_key1_ =
        blink::StorageKey::CreateFromStringForTesting("http://example1.com");
    storage_key2_ =
        blink::StorageKey::CreateFromStringForTesting("http://example2.com");

    CacheStorageManagerTest::CreateStorageManager();

    ReinitializeStorageKeysAndBucketLocators();
  }

  void ReinitializeStorageKeysAndBucketLocators() {
    std::string bucket_name;
    switch (test_case_) {
      case StorageKeyAndBucketTestCase::kFirstPartyDefault:
      case StorageKeyAndBucketTestCase::kFirstPartyDefaultPartitionEnabled:
      case StorageKeyAndBucketTestCase::kThirdPartyDefault:
        bucket_name = storage::kDefaultBucketName;
        break;
      case StorageKeyAndBucketTestCase::kFirstPartyNamed:
      case StorageKeyAndBucketTestCase::kFirstPartyNamedPartitionEnabled:
      case StorageKeyAndBucketTestCase::kThirdPartyNamed:
        bucket_name = "non-default";
        break;
    }

    switch (test_case_) {
      case StorageKeyAndBucketTestCase::kFirstPartyDefault:
      case StorageKeyAndBucketTestCase::kFirstPartyDefaultPartitionEnabled:
        // For this case, the storage keys and bucket locators are already
        // initialized correctly.
        ASSERT_TRUE(storage_key1_.IsFirstPartyContext());
        ASSERT_TRUE(storage_key2_.IsFirstPartyContext());

        ASSERT_EQ(bucket_locator1_.id, storage::BucketId::FromUnsafeValue(1));
        ASSERT_EQ(bucket_locator2_.id, storage::BucketId::FromUnsafeValue(2));
        break;
      case StorageKeyAndBucketTestCase::kFirstPartyNamed:
      case StorageKeyAndBucketTestCase::kFirstPartyNamedPartitionEnabled: {
        // For this case, the storage keys are initialized correctly but we
        // need to create new named buckets.
        ASSERT_TRUE(storage_key1_.IsFirstPartyContext());
        ASSERT_TRUE(storage_key2_.IsFirstPartyContext());

        ASSERT_OK_AND_ASSIGN(bucket_locator1_,
                             GetOrCreateBucket(storage_key1_, bucket_name));
        ASSERT_OK_AND_ASSIGN(bucket_locator2_,
                             GetOrCreateBucket(storage_key2_, bucket_name));
        ASSERT_EQ(bucket_locator1_.id, storage::BucketId::FromUnsafeValue(3));
        ASSERT_EQ(bucket_locator2_.id, storage::BucketId::FromUnsafeValue(4));
        break;
      }

      case StorageKeyAndBucketTestCase::kThirdPartyDefault:
      case StorageKeyAndBucketTestCase::kThirdPartyNamed:
        // Recreate storage keys and buckets.
        storage_key1_ = blink::StorageKey::Create(
            url::Origin::Create(GURL("http://example1.com")),
            net::SchemefulSite(GURL("http://example3.com")),
            blink::mojom::AncestorChainBit::kCrossSite);
        storage_key2_ = blink::StorageKey::Create(
            url::Origin::Create(GURL("http://example2.com")),
            net::SchemefulSite(GURL("http://example3.com")),
            blink::mojom::AncestorChainBit::kCrossSite);

        ASSERT_OK_AND_ASSIGN(bucket_locator1_,
                             GetOrCreateBucket(storage_key1_, bucket_name));
        ASSERT_OK_AND_ASSIGN(bucket_locator2_,
                             GetOrCreateBucket(storage_key2_, bucket_name));
        ASSERT_EQ(bucket_locator1_.id, storage::BucketId::FromUnsafeValue(3));
        ASSERT_EQ(bucket_locator2_.id, storage::BucketId::FromUnsafeValue(4));
        break;
    }
  }

  bool ThirdPartyStoragePartitioningEnabled() {
    return test_case_ == StorageKeyAndBucketTestCase::
                             kFirstPartyDefaultPartitionEnabled ||
           test_case_ ==
               StorageKeyAndBucketTestCase::kFirstPartyNamedPartitionEnabled ||
           test_case_ == StorageKeyAndBucketTestCase::kThirdPartyDefault ||
           test_case_ == StorageKeyAndBucketTestCase::kThirdPartyNamed;
  }

 private:
  StorageKeyAndBucketTestCase test_case_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(CacheStorageManagerTest, TestsRunOnIOThread) {
  EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
}

TEST_P(CacheStorageManagerTestP, OpenCache) {
  EXPECT_TRUE(Open(bucket_locator1_, "foo"));
}

TEST_P(CacheStorageManagerTestP, OpenTwoCaches) {
  EXPECT_TRUE(Open(bucket_locator1_, "foo"));
  EXPECT_TRUE(Open(bucket_locator1_, "bar"));
}

TEST_P(CacheStorageManagerTestP, OpenSameCacheDifferentOwners) {
  EXPECT_TRUE(Open(bucket_locator1_, "foo",
                   storage::mojom::CacheStorageOwner::kCacheAPI));
  CacheStorageCacheHandle cache_handle = std::move(callback_cache_handle_);
  EXPECT_TRUE(Open(bucket_locator1_, "foo",
                   storage::mojom::CacheStorageOwner::kBackgroundFetch));
  EXPECT_NE(callback_cache_handle_.value(), cache_handle.value());
}

TEST_P(CacheStorageManagerTestP, CachePointersDiffer) {
  EXPECT_TRUE(Open(bucket_locator1_, "foo"));
  CacheStorageCacheHandle cache_handle = std::move(callback_cache_handle_);
  EXPECT_TRUE(Open(bucket_locator1_, "bar"));
  EXPECT_NE(callback_cache_handle_.value(), cache_handle.value());
}

TEST_P(CacheStorageManagerTestP, Open2CachesSameNameDiffKeys) {
  EXPECT_TRUE(Open(bucket_locator1_, "foo"));
  CacheStorageCacheHandle cache_handle = std::move(callback_cache_handle_);
  EXPECT_TRUE(Open(bucket_locator2_, "foo"));
  EXPECT_NE(cache_handle.value(), callback_cache_handle_.value());
}

TEST_P(CacheStorageManagerTestP, OpenExistingCache) {
  EXPECT_TRUE(Open(bucket_locator1_, "foo"));
  CacheStorageCacheHandle cache_handle = std::move(callback_cache_handle_);
  EXPECT_TRUE(Open(bucket_locator1_, "foo"));
  EXPECT_EQ(callback_cache_handle_.value(), cache_handle.value());
}

TEST_P(CacheStorageManagerTestP, HasCache) {
  EXPECT_TRUE(Open(bucket_locator1_, "foo"));
  EXPECT_TRUE(Has(bucket_locator1_, "foo"));
  EXPECT_TRUE(callback_bool_);
}

TEST_P(CacheStorageManagerTestP, HasCacheDifferentOwners) {
  EXPECT_TRUE(Open(bucket_locator1_, "public",
                   storage::mojom::CacheStorageOwner::kCacheAPI));
  EXPECT_TRUE(Open(bucket_locator1_, "bgf",
                   storage::mojom::CacheStorageOwner::kBackgroundFetch));

  EXPECT_TRUE(Has(bucket_locator1_, "public",
                  storage::mojom::CacheStorageOwner::kCacheAPI));
  EXPECT_TRUE(callback_bool_);
  EXPECT_FALSE(Has(bucket_locator1_, "bgf",
                   storage::mojom::CacheStorageOwner::kCacheAPI));
  EXPECT_FALSE(callback_bool_);

  EXPECT_TRUE(Has(bucket_locator1_, "bgf",
                  storage::mojom::CacheStorageOwner::kBackgroundFetch));
  EXPECT_TRUE(callback_bool_);
  EXPECT_FALSE(Has(bucket_locator1_, "public",
                   storage::mojom::CacheStorageOwner::kBackgroundFetch));
  EXPECT_FALSE(callback_bool_);
}

TEST_P(CacheStorageManagerTestP, HasNonExistent) {
  EXPECT_FALSE(Has(bucket_locator1_, "foo"));
}

TEST_P(CacheStorageManagerTestP, DeleteCache) {
  EXPECT_TRUE(Open(bucket_locator1_, "foo"));
  EXPECT_TRUE(Delete(bucket_locator1_, "foo"));
  EXPECT_FALSE(Has(bucket_locator1_, "foo"));
}

TEST_P(CacheStorageManagerTestP, DeleteTwice) {
  EXPECT_TRUE(Open(bucket_locator1_, "foo"));
  EXPECT_TRUE(Delete(bucket_locator1_, "foo"));
  EXPECT_FALSE(Delete(bucket_locator1_, "foo"));
  EXPECT_EQ(CacheStorageError::kErrorNotFound, callback_error_);
}

TEST_P(CacheStorageManagerTestP, DeleteCacheReducesKeySize) {
  EXPECT_TRUE(Open(bucket_locator1_, "foo"));
  EXPECT_TRUE(
      CachePut(callback_cache_handle_.value(), GURL("http://example.com/foo")));
  // The quota manager gets updated after the put operation runs its callback so
  // run the event loop.
  base::RunLoop().RunUntilIdle();
  int64_t put_delta = *quota_manager_proxy_->last_notified_bucket_delta();
  EXPECT_LT(0, put_delta);
  EXPECT_TRUE(Delete(bucket_locator1_, "foo"));

  // Drop the cache handle so that the cache can be erased from disk.
  callback_cache_handle_ = CacheStorageCacheHandle();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(*quota_manager_proxy_->last_notified_bucket_delta(), -put_delta);
}

TEST_P(CacheStorageManagerTestP, EmptyKeys) {
  EXPECT_EQ(0u, Keys(bucket_locator1_));
}

TEST_P(CacheStorageManagerTestP, SomeKeys) {
  EXPECT_TRUE(Open(bucket_locator1_, "foo"));
  EXPECT_TRUE(Open(bucket_locator1_, "bar"));
  EXPECT_TRUE(Open(bucket_locator2_, "baz"));
  EXPECT_EQ(2u, Keys(bucket_locator1_));
  std::vector<std::string> expected_keys;
  expected_keys.push_back("foo");
  expected_keys.push_back("bar");
  EXPECT_EQ(expected_keys, GetIndexNames());
  EXPECT_EQ(1u, Keys(bucket_locator2_));
  EXPECT_STREQ("baz", GetFirstIndexName().c_str());
}

TEST_P(CacheStorageManagerTestP, DeletedKeysGone) {
  EXPECT_TRUE(Open(bucket_locator1_, "foo"));
  EXPECT_TRUE(Open(bucket_locator1_, "bar"));
  EXPECT_TRUE(Open(bucket_locator2_, "baz"));
  EXPECT_TRUE(Delete(bucket_locator1_, "bar"));
  EXPECT_EQ(1u, Keys(bucket_locator1_));
  EXPECT_STREQ("foo", GetFirstIndexName().c_str());
}

TEST_P(CacheStorageManagerTestP, StorageMatchEntryExists) {
  EXPECT_TRUE(Open(bucket_locator1_, "foo"));
  EXPECT_TRUE(
      CachePut(callback_cache_handle_.value(), GURL("http://example.com/foo")));
  EXPECT_TRUE(
      StorageMatch(bucket_locator1_, "foo", GURL("http://example.com/foo")));
}

TEST_P(CacheStorageManagerTestP, StorageMatchNoEntry) {
  EXPECT_TRUE(Open(bucket_locator1_, "foo"));
  EXPECT_TRUE(
      CachePut(callback_cache_handle_.value(), GURL("http://example.com/foo")));
  EXPECT_FALSE(
      StorageMatch(bucket_locator1_, "foo", GURL("http://example.com/bar")));
  EXPECT_EQ(CacheStorageError::kErrorNotFound, callback_error_);
}

TEST_P(CacheStorageManagerTestP, StorageMatchNoCache) {
  EXPECT_TRUE(Open(bucket_locator1_, "foo"));
  EXPECT_TRUE(
      CachePut(callback_cache_handle_.value(), GURL("http://example.com/foo")));
  EXPECT_FALSE(
      StorageMatch(bucket_locator1_, "bar", GURL("http://example.com/foo")));
  EXPECT_EQ(CacheStorageError::kErrorCacheNameNotFound, callback_error_);
}

TEST_P(CacheStorageManagerTestP, StorageMatchAllEntryExists) {
  EXPECT_TRUE(Open(bucket_locator1_, "foo"));
  EXPECT_TRUE(
      CachePut(callback_cache_handle_.value(), GURL("http://example.com/foo")));
  EXPECT_TRUE(
      StorageMatchAll(bucket_locator1_, GURL("http://example.com/foo")));
}

TEST_P(CacheStorageManagerTestP, StorageMatchAllNoEntry) {
  EXPECT_TRUE(Open(bucket_locator1_, "foo"));
  EXPECT_TRUE(
      CachePut(callback_cache_handle_.value(), GURL("http://example.com/foo")));
  EXPECT_FALSE(
      StorageMatchAll(bucket_locator1_, GURL("http://example.com/bar")));
  EXPECT_EQ(CacheStorageError::kErrorNotFound, callback_error_);
}

TEST_P(CacheStorageManagerTestP, StorageMatchAllNoCaches) {
  EXPECT_FALSE(
      StorageMatchAll(bucket_locator1_, GURL("http://example.com/foo")));
  EXPECT_EQ(CacheStorageError::kErrorNotFound, callback_error_);
}

TEST_P(CacheStorageManagerTestP, StorageMatchDifferentOwners) {
  EXPECT_TRUE(Open(bucket_locator1_, "foo",
                   storage::mojom::CacheStorageOwner::kCacheAPI));
  EXPECT_TRUE(CachePut(callback_cache_handle_.value(),
                       GURL("http://example.com/public")));
  EXPECT_TRUE(Open(bucket_locator1_, "foo",
                   storage::mojom::CacheStorageOwner::kBackgroundFetch));
  EXPECT_TRUE(
      CachePut(callback_cache_handle_.value(), GURL("http://example.com/bgf")));

  // Check the public cache.
  EXPECT_TRUE(StorageMatch(bucket_locator1_, "foo",
                           GURL("http://example.com/public"), nullptr,
                           storage::mojom::CacheStorageOwner::kCacheAPI));
  EXPECT_FALSE(StorageMatch(bucket_locator1_, "foo",
                            GURL("http://example.com/bgf"), nullptr,
                            storage::mojom::CacheStorageOwner::kCacheAPI));

  // Check the internal cache.
  EXPECT_FALSE(StorageMatch(
      bucket_locator1_, "foo", GURL("http://example.com/public"), nullptr,
      storage::mojom::CacheStorageOwner::kBackgroundFetch));
  EXPECT_TRUE(StorageMatch(
      bucket_locator1_, "foo", GURL("http://example.com/bgf"), nullptr,
      storage::mojom::CacheStorageOwner::kBackgroundFetch));
}

TEST_F(CacheStorageManagerTest, StorageReuseCacheName) {
  // Deleting a cache and creating one with the same name and adding an entry
  // with the same URL should work. (see crbug.com/542668)
  const GURL kTestURL = GURL("http://example.com/foo");
  EXPECT_TRUE(Open(bucket_locator1_, "foo"));
  EXPECT_TRUE(CachePut(callback_cache_handle_.value(), kTestURL));
  EXPECT_TRUE(CacheMatch(callback_cache_handle_.value(), kTestURL));

  EXPECT_TRUE(Delete(bucket_locator1_, "foo"));
  // The cache is deleted but the handle to one of its entries is still
  // open. Creating a new cache in the same directory would fail on Windows.
  EXPECT_TRUE(Open(bucket_locator1_, "foo"));
  EXPECT_TRUE(CachePut(callback_cache_handle_.value(), kTestURL));
}

TEST_P(CacheStorageManagerTestP, DropRefAfterNewCacheWithSameNameCreated) {
  // Make sure that dropping the final cache handle to a doomed cache doesn't
  // affect newer caches with the same name. (see crbug.com/631467)

  // 1. Create cache A and hang onto the handle
  const GURL kTestURL = GURL("http://example.com/foo");
  EXPECT_TRUE(Open(bucket_locator1_, "foo"));
  CacheStorageCacheHandle cache_handle = std::move(callback_cache_handle_);

  // 2. Doom the cache
  EXPECT_TRUE(Delete(bucket_locator1_, "foo"));

  // 3. Create cache B (with the same name)
  EXPECT_TRUE(Open(bucket_locator1_, "foo"));

  // 4. Drop handle to A
  cache_handle = CacheStorageCacheHandle();

  // 5. Verify that B still works
  EXPECT_FALSE(CacheMatch(callback_cache_handle_.value(), kTestURL));
}

TEST_P(CacheStorageManagerTestP, DeleteCorrectDirectory) {
  // This test reproduces crbug.com/630036.
  // 1. Cache A with name "foo" is created
  const GURL kTestURL = GURL("http://example.com/foo");
  EXPECT_TRUE(Open(bucket_locator1_, "foo"));
  CacheStorageCacheHandle cache_handle = std::move(callback_cache_handle_);

  // 2. Cache A is doomed, but js hangs onto the handle.
  EXPECT_TRUE(Delete(bucket_locator1_, "foo"));

  // 3. Cache B with name "foo" is created
  EXPECT_TRUE(Open(bucket_locator1_, "foo"));

  // 4. Cache B is doomed, and both handles are reset.
  EXPECT_TRUE(Delete(bucket_locator1_, "foo"));
  cache_handle = CacheStorageCacheHandle();

  // Do some busy work on a different cache to move the cache pool threads
  // along and trigger the bug.
  EXPECT_TRUE(Open(bucket_locator1_, "bar"));
  EXPECT_TRUE(CachePut(callback_cache_handle_.value(), kTestURL));
  EXPECT_TRUE(CacheMatch(callback_cache_handle_.value(), kTestURL));
}

TEST_P(CacheStorageManagerTestP, StorageMatchAllEntryExistsTwice) {
  EXPECT_TRUE(Open(bucket_locator1_, "foo"));
  // |request_1| and |request_2| has the same url.
  auto request_1 = blink::mojom::FetchAPIRequest::New();
  request_1->url = GURL("http://example.com/foo");
  auto request_2 = BackgroundFetchSettledFetch::CloneRequest(request_1);
  EXPECT_TRUE(CachePutWithStatusCode(callback_cache_handle_.value(),
                                     std::move(request_1), 200));
  EXPECT_TRUE(Open(bucket_locator1_, "bar"));
  EXPECT_TRUE(CachePutWithStatusCode(callback_cache_handle_.value(),
                                     std::move(request_2), 201));

  EXPECT_TRUE(
      StorageMatchAll(bucket_locator1_, GURL("http://example.com/foo")));

  // The caches need to be searched in order of creation, so verify that the
  // response came from the first cache.
  EXPECT_EQ(200, callback_cache_handle_response_->status_code);
}

TEST_P(CacheStorageManagerTestP, StorageMatchInOneOfMany) {
  EXPECT_TRUE(Open(bucket_locator1_, "foo"));
  EXPECT_TRUE(Open(bucket_locator1_, "bar"));
  EXPECT_TRUE(
      CachePut(callback_cache_handle_.value(), GURL("http://example.com/foo")));
  EXPECT_TRUE(Open(bucket_locator1_, "baz"));

  EXPECT_TRUE(
      StorageMatchAll(bucket_locator1_, GURL("http://example.com/foo")));
}

TEST_P(CacheStorageManagerTestP, Chinese) {
  EXPECT_TRUE(Open(bucket_locator1_, "你好"));
  CacheStorageCacheHandle cache_handle = std::move(callback_cache_handle_);
  EXPECT_TRUE(Open(bucket_locator1_, "你好"));
  EXPECT_EQ(callback_cache_handle_.value(), cache_handle.value());
  EXPECT_EQ(1u, Keys(bucket_locator1_));
  EXPECT_STREQ("你好", GetFirstIndexName().c_str());
}

TEST_F(CacheStorageManagerTest, EmptyKey) {
  EXPECT_TRUE(Open(bucket_locator1_, ""));
  CacheStorageCacheHandle cache_handle = std::move(callback_cache_handle_);
  EXPECT_TRUE(Open(bucket_locator1_, ""));
  EXPECT_EQ(cache_handle.value(), callback_cache_handle_.value());
  EXPECT_EQ(1u, Keys(bucket_locator1_));
  EXPECT_STREQ("", GetFirstIndexName().c_str());
  EXPECT_TRUE(Has(bucket_locator1_, ""));
  EXPECT_TRUE(Delete(bucket_locator1_, ""));
  EXPECT_EQ(0u, Keys(bucket_locator1_));
}

TEST_P(CacheStorageManagerStorageKeyAndBucketTestP, DataPersists) {
  EXPECT_TRUE(Open(bucket_locator1_, "foo"));
  EXPECT_TRUE(Open(bucket_locator1_, "bar"));
  EXPECT_TRUE(Open(bucket_locator1_, "baz"));
  EXPECT_TRUE(Open(bucket_locator2_, "raz"));
  EXPECT_TRUE(Delete(bucket_locator1_, "bar"));
  RecreateStorageManager();
  EXPECT_EQ(2u, Keys(bucket_locator1_));
  std::vector<std::string> expected_keys;
  expected_keys.push_back("foo");
  expected_keys.push_back("baz");
  EXPECT_EQ(expected_keys, GetIndexNames());
}

TEST_F(CacheStorageManagerMemoryOnlyTest, DataLostWhenMemoryOnly) {
  EXPECT_TRUE(Open(bucket_locator1_, "foo"));
  EXPECT_TRUE(Open(bucket_locator2_, "baz"));
  RecreateStorageManager();
  EXPECT_EQ(0u, Keys(bucket_locator1_));
}

TEST_F(CacheStorageManagerTest, BadCacheName) {
  // Since the implementation writes cache names to disk, ensure that we don't
  // escape the directory.
  const std::string bad_name = "../../../../../../../../../../../../../../foo";
  EXPECT_TRUE(Open(bucket_locator1_, bad_name));
  EXPECT_EQ(1u, Keys(bucket_locator1_));
  EXPECT_STREQ(bad_name.c_str(), GetFirstIndexName().c_str());
}

TEST_F(CacheStorageManagerTest, BadKeyName) {
  // Since the implementation writes origin names to disk, ensure that we don't
  // escape the directory.
  const blink::StorageKey bad_key =
      blink::StorageKey::CreateFromStringForTesting(
          "http://../../../../../../../../../../../../../../foo");
  ASSERT_OK_AND_ASSIGN(storage::BucketLocator bad_bucket_locator,
                       GetOrCreateBucket(bad_key, storage::kDefaultBucketName));
  EXPECT_TRUE(Open(bad_bucket_locator, "foo"));
  EXPECT_EQ(1u, Keys(bad_bucket_locator));
  EXPECT_EQ("foo", GetFirstIndexName());
}

// Dropping a reference to a cache should not immediately destroy it.  These
// warm cache objects are kept alive to optimize the next open.
TEST_F(CacheStorageManagerTest, DropReference) {
  CacheStorageHandle cache_storage = CacheStorageForBucket(bucket_locator1_);

  EXPECT_TRUE(Open(bucket_locator1_, "foo"));
  base::WeakPtr<CacheStorageCache> cache =
      CacheStorageCache::From(callback_cache_handle_)->AsWeakPtr();
  // Run a cache operation to ensure that the cache has finished initializing so
  // that when the handle is dropped it could possibly close immediately.
  EXPECT_FALSE(CacheMatch(callback_cache_handle_.value(),
                          GURL("http://example.com/foo")));

  callback_cache_handle_ = CacheStorageCacheHandle();
  EXPECT_TRUE(cache) << "unreferenced cache destroyed while owning "
                        "CacheStorage is still referenced";

  cache_storage = CacheStorageHandle();
  EXPECT_FALSE(cache) << "unreferenced cache not destroyed after last "
                         "CacheStorage reference removed";
}

// Deleting a cache should remove any warmed caches that been kept alive
// without a reference.
TEST_F(CacheStorageManagerTest, DropReferenceAndDelete) {
  // Hold a reference to the CacheStorage to permit the warmed
  // CacheStorageCache to be kept alive.
  CacheStorageHandle cache_storage = CacheStorageForBucket(bucket_locator1_);

  EXPECT_TRUE(Open(bucket_locator1_, "foo"));
  base::WeakPtr<CacheStorageCache> cache =
      CacheStorageCache::From(callback_cache_handle_)->AsWeakPtr();
  // Run a cache operation to ensure that the cache has finished initializing so
  // that when the handle is dropped it could possibly close immediately.
  EXPECT_FALSE(CacheMatch(callback_cache_handle_.value(),
                          GURL("http://example.com/foo")));

  callback_cache_handle_ = CacheStorageCacheHandle();
  EXPECT_TRUE(cache) << "unreferenced cache destroyed while owning "
                        "CacheStorage is still referenced";

  // Delete() should trigger its destruction, however.
  EXPECT_TRUE(Delete(bucket_locator1_, "foo"));
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(cache)
      << "deleted cache not destroyed after last reference removed";
}

// Critical memory pressure should remove any warmed caches that been kept
// alive without a reference.
TEST_F(CacheStorageManagerTest, DropReferenceAndMemoryPressure) {
  // Hold a reference to the CacheStorage to permit the warmed
  // CacheStorageCache to be kept alive.
  CacheStorageHandle cache_storage = CacheStorageForBucket(bucket_locator1_);

  EXPECT_TRUE(Open(bucket_locator1_, "foo"));
  base::WeakPtr<CacheStorageCache> cache =
      CacheStorageCache::From(callback_cache_handle_)->AsWeakPtr();
  // Run a cache operation to ensure that the cache has finished initializing so
  // that when the handle is dropped it could possibly close immediately.
  EXPECT_FALSE(CacheMatch(callback_cache_handle_.value(),
                          GURL("http://example.com/foo")));

  callback_cache_handle_ = CacheStorageCacheHandle();
  EXPECT_TRUE(cache) << "unreferenced cache destroyed while owning "
                        "CacheStorage is still referenced";

  // Moderate memory pressure should not destroy unreferenced cache objects
  // since reading data back in from disk can be expensive.
  base::MemoryPressureListener::NotifyMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel::
          MEMORY_PRESSURE_LEVEL_MODERATE);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(cache);

  // Critical memory pressure should destroy unreferenced cache objects.
  base::MemoryPressureListener::NotifyMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel::
          MEMORY_PRESSURE_LEVEL_CRITICAL);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(cache)
      << "unreferenced cache not destroyed on critical memory pressure";
}

TEST_F(CacheStorageManagerTest, DropReferenceDuringQuery) {
  // Setup the cache and execute an operation to make sure all initialization
  // is complete.
  EXPECT_TRUE(Open(bucket_locator1_, "foo"));
  base::WeakPtr<CacheStorageCache> cache =
      CacheStorageCache::From(callback_cache_handle_)->AsWeakPtr();
  EXPECT_FALSE(CacheMatch(callback_cache_handle_.value(),
                          GURL("http://example.com/foo")));

  // Override the cache scheduler so that we can take an action below
  // after the query operation begins.
  base::RunLoop scheduler_loop;
  auto scheduler =
      std::make_unique<CallbackScheduler>(scheduler_loop.QuitClosure());
  cache->SetSchedulerForTesting(std::move(scheduler));

  // Perform a MatchAll() operation to trigger a full query of the cache
  // that does not hit the fast path optimization.
  base::RunLoop match_loop;
  cache->MatchAll(
      nullptr, nullptr, /* trace_id = */ 0,
      base::BindOnce(&CacheStorageManagerTest::CacheMatchAllCallback,
                     base::Unretained(this), base::Unretained(&match_loop)));

  // Wait for the MatchAll operation to begin.
  scheduler_loop.Run();

  // Clear the external cache handle.
  callback_cache_handle_ = CacheStorageCacheHandle();

  // Wait for the MatchAll operation to complete as expected.
  match_loop.Run();
  EXPECT_EQ(CacheStorageError::kSuccess, callback_error_);
}
// A cache continues to work so long as there is a handle to it. Only after the
// last cache handle is deleted can the cache be freed.
TEST_P(CacheStorageManagerTestP, CacheWorksAfterDelete) {
  const GURL kFooURL("http://example.com/foo");
  const GURL kBarURL("http://example.com/bar");
  const GURL kBazURL("http://example.com/baz");
  EXPECT_TRUE(Open(bucket_locator1_, "foo"));
  CacheStorageCacheHandle original_handle = std::move(callback_cache_handle_);
  EXPECT_TRUE(CachePut(original_handle.value(), kFooURL));
  EXPECT_TRUE(Delete(bucket_locator1_, "foo"));

  // Verify that the existing cache handle still works.
  EXPECT_TRUE(CacheMatch(original_handle.value(), kFooURL));
  EXPECT_TRUE(CachePut(original_handle.value(), kBarURL));
  EXPECT_TRUE(CacheMatch(original_handle.value(), kBarURL));

  // The cache shouldn't be visible to subsequent storage operations.
  EXPECT_EQ(0u, Keys(bucket_locator1_));

  // Open a new cache with the same name, it should create a new cache, but not
  // interfere with the original cache.
  EXPECT_TRUE(Open(bucket_locator1_, "foo"));
  CacheStorageCacheHandle new_handle = std::move(callback_cache_handle_);
  EXPECT_TRUE(CachePut(new_handle.value(), kBazURL));

  EXPECT_FALSE(CacheMatch(new_handle.value(), kFooURL));
  EXPECT_FALSE(CacheMatch(new_handle.value(), kBarURL));
  EXPECT_TRUE(CacheMatch(new_handle.value(), kBazURL));

  EXPECT_TRUE(CacheMatch(original_handle.value(), kFooURL));
  EXPECT_TRUE(CacheMatch(original_handle.value(), kBarURL));
  EXPECT_FALSE(CacheMatch(original_handle.value(), kBazURL));
}

// Deleted caches can still be modified, but all changes will eventually be
// thrown away when all references are released.
TEST_F(CacheStorageManagerTest, DeletedCacheIgnoredInIndex) {
  const GURL kFooURL("http://example.com/foo");
  const GURL kBarURL("http://example.com/bar");
  const GURL kBazURL("http://example.com/baz");
  const std::string kCacheName = "foo";

  EXPECT_TRUE(Open(bucket_locator1_, kCacheName));
  auto original_handle = std::move(callback_cache_handle_);
  EXPECT_TRUE(CachePut(original_handle.value(), kFooURL));
  EXPECT_TRUE(Delete(bucket_locator1_, kCacheName));

  // Now a second cache using the same name, but with different data.
  EXPECT_TRUE(Open(bucket_locator1_, kCacheName));
  auto new_handle = std::move(callback_cache_handle_);
  EXPECT_TRUE(CachePut(new_handle.value(), kFooURL));
  EXPECT_TRUE(CachePut(new_handle.value(), kBarURL));
  EXPECT_TRUE(CachePut(new_handle.value(), kBazURL));
  auto new_cache_size = Size(bucket_locator1_);

  // Now modify the first cache.
  EXPECT_TRUE(CachePut(original_handle.value(), kBarURL));

  // Now deref both caches, and recreate the storage manager.
  original_handle = CacheStorageCacheHandle();
  new_handle = CacheStorageCacheHandle();
  EXPECT_TRUE(FlushCacheStorageIndex(bucket_locator1_));
  DestroyStorageManager();
  CreateStorageManager();

  EXPECT_TRUE(Open(bucket_locator1_, kCacheName));
  EXPECT_EQ(new_cache_size, Size(bucket_locator1_));
}

TEST_F(CacheStorageManagerTest, TestErrorInitializingCache) {
  if (MemoryOnly()) {
    return;
  }
  const GURL kFooURL("http://example.com/foo");
  const std::string kCacheName = "foo";

  EXPECT_TRUE(Open(bucket_locator1_, kCacheName));
  auto original_handle = std::move(callback_cache_handle_);
  EXPECT_TRUE(CachePut(original_handle.value(), kFooURL));
  auto size_before_close = Size(bucket_locator1_);
  EXPECT_GT(size_before_close, 0);

  CacheStorageHandle cache_storage = CacheStorageForBucket(bucket_locator1_);
  auto cache_handle =
      CacheStorage::From(cache_storage)->GetLoadedCache(kCacheName);
  base::FilePath cache_path = CacheStorageCache::From(cache_handle)->path();
  base::FilePath storage_path = cache_path.DirName();
  base::FilePath index_path = cache_path.AppendASCII("index");
  cache_handle = CacheStorageCacheHandle();

  // Do our best to flush any pending cache_storage index file writes to disk
  // before proceeding.  This does not guarantee the simple disk_cache index
  // is written, though.
  EXPECT_TRUE(FlushCacheStorageIndex(bucket_locator1_));
  EXPECT_FALSE(FlushCacheStorageIndex(bucket_locator1_));

  DestroyStorageManager();

  // Truncate the SimpleCache index to force an error when next opened.
  ASSERT_FALSE(index_path.empty());
  ASSERT_TRUE(base::WriteFile(index_path, "hello"));

  // The cache_storage index and simple disk_cache index files are written from
  // background threads.  They may be written in unexpected orders due to timing
  // differences.  We need to ensure the cache directory to have a newer time
  // stamp, though, in order for the Size() method to actually try calculating
  // the size from the corrupted simple disk_cache.  Therefore we force the
  // cache_storage index to have a much older time to ensure that it is not used
  // in the following Size() call.
  base::FilePath cache_index_path =
      storage_path.AppendASCII(CacheStorage::kIndexFileName);
  base::Time t = base::Time::Now() + base::Hours(-1);
  EXPECT_TRUE(base::TouchFile(cache_index_path, t, t));
  EXPECT_FALSE(IsIndexFileCurrent(storage_path));

  CreateStorageManager();

  EXPECT_TRUE(Open(bucket_locator1_, kCacheName));
  EXPECT_EQ(0, Size(bucket_locator1_));
}

TEST_P(CacheStorageManagerStorageKeyAndBucketTestP,
       CacheSizeCorrectAfterReopen) {
  const GURL kFooURL("http://example.com/foo");
  const std::string kCacheName = "foo";

  EXPECT_TRUE(Open(bucket_locator1_, kCacheName));
  auto original_handle = std::move(callback_cache_handle_);
  EXPECT_TRUE(CachePut(original_handle.value(), kFooURL));
  auto size_before_close = Size(bucket_locator1_);
  EXPECT_GT(size_before_close, 0);

  // Double-check that the CacheStorage files have been written to the
  // directory we expect.
  base::FilePath calculated_path = CacheStorageManager::ConstructBucketPath(
      cache_manager_->profile_path(), bucket_locator1_,
      storage::mojom::CacheStorageOwner::kCacheAPI);

  base::FilePath expected_path;
  if (!bucket_locator1_.is_default || (ThirdPartyStoragePartitioningEnabled() &&
                                       storage_key1_.IsThirdPartyContext())) {
    // Named buckets and third-party contexts should use the new directory
    // format.
    expected_path =
        temp_dir_.GetPath()
            .AppendASCII("WebStorage")
            .AppendASCII(base::NumberToString(bucket_locator1_.id.value()))
            .Append(storage::kCacheStorageDirectory);
  } else {
    // Default buckets and first-party contexts should use the legacy path
    // format. We'll hardcode the origin hash corresponding to
    // `storage_key1_.origin()`.
    expected_path =
        temp_dir_.GetPath()
            .AppendASCII("Service Worker")
            .Append(storage::kCacheStorageDirectory)
            .AppendASCII("0430f1a484a0ea6d8de562488c5fbeec0111d16f");
  }

  EXPECT_EQ(expected_path, calculated_path);

  DestroyStorageManager();

  EXPECT_TRUE(base::PathExists(calculated_path));

  CreateStorageManager();

  EXPECT_TRUE(Open(bucket_locator1_, kCacheName));
  EXPECT_EQ(size_before_close, Size(bucket_locator1_));
}

TEST_F(CacheStorageManagerTest, CacheSizePaddedAfterReopen) {
  const GURL kFooURL = storage_key1_.origin().GetURL().Resolve("foo");
  const std::string kCacheName = "foo";

  EXPECT_FALSE(quota_manager_proxy_->last_notified_bucket_delta().has_value());
  EXPECT_EQ(0, quota_manager_proxy_->notify_bucket_modified_count());

  EXPECT_TRUE(Open(bucket_locator1_, kCacheName));
  CacheStorageCacheHandle original_handle = std::move(callback_cache_handle_);

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(quota_manager_proxy_->last_notified_bucket_delta().has_value());
  EXPECT_EQ(0, quota_manager_proxy_->notify_bucket_modified_count());

  EXPECT_TRUE(
      CachePut(original_handle.value(), kFooURL, FetchResponseType::kOpaque));
  int64_t cache_size_before_close = Size(bucket_locator1_);
  base::FilePath storage_dir =
      CacheStorageCache::From(original_handle)->path().DirName();
  original_handle = CacheStorageCacheHandle();
  EXPECT_GT(cache_size_before_close, 0);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(cache_size_before_close, GetQuotaKeyUsage(storage_key1_));

  base::RunLoop().RunUntilIdle();
  const int64_t put_delta =
      quota_manager_proxy_->last_notified_bucket_delta().value_or(0);
  EXPECT_GT(put_delta, 0);
  EXPECT_EQ(1, quota_manager_proxy_->notify_bucket_modified_count());

  EXPECT_EQ(GetQuotaKeyUsage(storage_key1_), put_delta);

  // Close the caches and cache manager.
  EXPECT_TRUE(FlushCacheStorageIndex(bucket_locator1_));
  DestroyStorageManager();

  // Create a new CacheStorageManager that hasn't yet loaded the key.
  CreateStorageManager();
  RecreateStorageManager();
  EXPECT_TRUE(Open(bucket_locator1_, kCacheName));

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(quota_manager_proxy_->last_notified_bucket_delta().has_value());
  EXPECT_EQ(0, quota_manager_proxy_->notify_bucket_modified_count());

  EXPECT_EQ(cache_size_before_close, Size(bucket_locator1_));
}

TEST_F(CacheStorageManagerTest, QuotaCorrectAfterReopen) {
  const std::string kCacheName = "foo";

  // Choose a response type that will not be padded so that the expected
  // cache size can be calculated.
  const FetchResponseType response_type = FetchResponseType::kCors;

  // Create a new cache.
  int64_t cache_size;
  {
    EXPECT_TRUE(Open(bucket_locator1_, kCacheName));
    CacheStorageCacheHandle cache_handle = std::move(callback_cache_handle_);
    base::RunLoop().RunUntilIdle();

    const GURL kFooURL = storage_key1_.origin().GetURL().Resolve("foo");
    EXPECT_TRUE(CachePut(cache_handle.value(), kFooURL, response_type));
    cache_size = Size(bucket_locator1_);

    EXPECT_EQ(cache_size, GetQuotaKeyUsage(storage_key1_));
  }

  // Wait for the dereferenced cache to be closed.
  base::RunLoop().RunUntilIdle();

  // Now reopen the cache.
  EXPECT_TRUE(Open(bucket_locator1_, kCacheName));
  CacheStorageCacheHandle cache_handle = std::move(callback_cache_handle_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(cache_size, GetQuotaKeyUsage(storage_key1_));

  // And write a second equally sized value and verify size is doubled.
  const GURL kBarURL = storage_key1_.origin().GetURL().Resolve("bar");
  EXPECT_TRUE(CachePut(cache_handle.value(), kBarURL, response_type));

  EXPECT_EQ(2 * cache_size, GetQuotaKeyUsage(storage_key1_));
}

// With a memory cache the cache can't be freed from memory until the client
// calls delete.
TEST_F(CacheStorageManagerMemoryOnlyTest, MemoryLosesReferenceOnlyAfterDelete) {
  EXPECT_TRUE(Open(bucket_locator1_, "foo"));
  base::WeakPtr<CacheStorageCache> cache =
      CacheStorageCache::From(callback_cache_handle_)->AsWeakPtr();
  callback_cache_handle_ = CacheStorageCacheHandle();
  EXPECT_TRUE(cache);
  EXPECT_TRUE(Delete(bucket_locator1_, "foo"));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(cache);
}

TEST_P(CacheStorageManagerTestP, DeleteBeforeRelease) {
  EXPECT_TRUE(Open(bucket_locator1_, "foo"));
  EXPECT_TRUE(Delete(bucket_locator1_, "foo"));
  EXPECT_TRUE(callback_cache_handle_.value());
}

TEST_P(CacheStorageManagerTestP, OpenRunsSerially) {
  EXPECT_FALSE(Delete(bucket_locator1_, "tmp"));  // Init storage.
  CacheStorageHandle cache_storage = CacheStorageForBucket(bucket_locator1_);
  auto* impl = CacheStorage::From(cache_storage);
  auto id = impl->StartAsyncOperationForTesting();

  base::RunLoop open_loop;
  cache_storage.value()->OpenCache(
      "foo", /* trace_id = */ 0,
      base::BindOnce(&CacheStorageManagerTest::CacheAndErrorCallback,
                     base::Unretained(this), base::Unretained(&open_loop)));

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(callback_cache_handle_.value());

  impl->CompleteAsyncOperationForTesting(id);
  open_loop.Run();
  EXPECT_TRUE(callback_cache_handle_.value());
}

TEST_P(CacheStorageManagerTestP, GetBucketUsage) {
  EXPECT_EQ(0, GetBucketUsage(bucket_locator1_));
  EXPECT_TRUE(Open(bucket_locator1_, "foo"));
  EXPECT_EQ(0, GetBucketUsage(bucket_locator1_));
  EXPECT_TRUE(
      CachePut(callback_cache_handle_.value(), GURL("http://example.com/foo")));
  int64_t foo_size = GetBucketUsage(bucket_locator1_);
  EXPECT_LT(0, GetBucketUsage(bucket_locator1_));
  EXPECT_EQ(0, GetBucketUsage(bucket_locator2_));

  // Add the same entry into a second cache, the size should double.
  EXPECT_TRUE(Open(bucket_locator1_, "bar"));
  EXPECT_TRUE(
      CachePut(callback_cache_handle_.value(), GURL("http://example.com/foo")));
  EXPECT_EQ(2 * foo_size, GetBucketUsage(bucket_locator1_));
}

TEST_P(CacheStorageManagerStorageKeyAndBucketTestP, GetBucketUsage) {
  EXPECT_EQ(0, GetBucketUsage(bucket_locator1_));
  EXPECT_TRUE(Open(bucket_locator1_, "foo"));
  EXPECT_EQ(0, GetBucketUsage(bucket_locator1_));
  EXPECT_TRUE(
      CachePut(callback_cache_handle_.value(), GURL("http://example.com/foo")));
  int64_t foo_size = GetBucketUsage(bucket_locator1_);
  EXPECT_LT(0, GetBucketUsage(bucket_locator1_));
  EXPECT_EQ(0, GetBucketUsage(bucket_locator2_));

  // Add the same entry into a second cache, the size should double.
  EXPECT_TRUE(Open(bucket_locator1_, "bar"));
  EXPECT_TRUE(
      CachePut(callback_cache_handle_.value(), GURL("http://example.com/foo")));
  EXPECT_EQ(2 * foo_size, GetBucketUsage(bucket_locator1_));
}

TEST_F(CacheStorageManagerTest, GetBucketUsageConflictingBucketIds) {
  // For buckets with first-party storage keys and default names, the directory
  // path will be calculated solely based on the origin and owner. This means
  // that two `BucketLocator`s that are the same in all ways except the bucket
  // ID will use the same path under these conditions, so if we call
  // `OpenCacheStorage` with two such `BucketLocator`s, the second call will
  // instantiate an instance that would hang if we tried to use it (while the
  // the first instance remains open and "owns" the directory). In theory this
  // shouldn't happen, but just in case We have some logic to prevent this so
  // test that here.
  const GURL kTestURL = GURL("http://example.com/foo");
  auto modified_bucket_locator1_{bucket_locator1_};
  modified_bucket_locator1_.id = storage::BucketId::FromUnsafeValue(999);

  base::FilePath bucket_locator1_path =
      CacheStorageManager::ConstructBucketPath(
          cache_manager_->profile_path(), bucket_locator1_,
          storage::mojom::CacheStorageOwner::kCacheAPI);

  base::FilePath modified_bucket_locator1_path =
      CacheStorageManager::ConstructBucketPath(
          cache_manager_->profile_path(), modified_bucket_locator1_,
          storage::mojom::CacheStorageOwner::kCacheAPI);

  ASSERT_EQ(bucket_locator1_path, modified_bucket_locator1_path);

  EXPECT_TRUE(Open(bucket_locator1_, "foo"));
  EXPECT_TRUE(CachePut(callback_cache_handle_.value(), kTestURL));

  disk_cache::FlushCacheThreadForTesting();
  content::RunAllTasksUntilIdle();

  // We are testing that this call doesn't hang and returns 0 since the
  // directory is owned by the existing instance.
  EXPECT_EQ(GetBucketUsage(modified_bucket_locator1_), 0);

  // Clear out the CacheStorageManager's instance map.
  DestroyStorageManager();
  CreateStorageManager();

  // Now try the reverse order when there's an existing index file on disk but
  // there's no corresponding instance in the CacheStorage map. There should
  // already be an index file on disk leftover from the test above. In this
  // case, the `GetBucketUsage()` call should be able to read the size info
  // from disk and won't take ownership of the directory, allowing the
  // operations on the other instance to succeed.
  ASSERT_TRUE(base::PathExists(bucket_locator1_path));

  EXPECT_NE(GetBucketUsage(modified_bucket_locator1_), 0);

  disk_cache::FlushCacheThreadForTesting();
  content::RunAllTasksUntilIdle();

  EXPECT_TRUE(Open(bucket_locator1_, "foo"));
  EXPECT_TRUE(CacheMatch(callback_cache_handle_.value(), kTestURL));
  EXPECT_NE(Size(bucket_locator1_), 0);
}

// TODO(crbug.com/40868994): Re-enable test for Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_GetBucketUsageWithPadding DISABLED_GetBucketUsageWithPadding
#else
#define MAYBE_GetBucketUsageWithPadding GetBucketUsageWithPadding
#endif
TEST_P(CacheStorageManagerStorageKeyAndBucketTestP,
       MAYBE_GetBucketUsageWithPadding) {
  EXPECT_EQ(0, GetBucketUsage(bucket_locator1_));

  EXPECT_TRUE(Open(bucket_locator1_, "foo"));
  base::FilePath storage_dir =
      CacheStorageCache::From(callback_cache_handle_)->path().DirName();
  base::FilePath index_path =
      storage_dir.AppendASCII(CacheStorage::kIndexFileName);
  EXPECT_TRUE(
      CachePut(callback_cache_handle_.value(), GURL("http://example.com/foo")));

  // Ensure that the index file has been written to disk before calling
  // `GetBucketUsage()`.
  disk_cache::FlushCacheThreadForTesting();
  content::RunAllTasksUntilIdle();

  auto unpadded_size = GetBucketUsage(bucket_locator1_);
  EXPECT_TRUE(CachePut(callback_cache_handle_.value(),
                       GURL("http://example.com/foo"),
                       FetchResponseType::kOpaque));

  EXPECT_TRUE(FlushCacheStorageIndex(bucket_locator1_));

  // We want to verify that padded values are read from the index
  // file.  If the index is out-of-date, though, the code falls back
  // to the CacheStorage Size() method.  Further, the underlying disk_cache
  // does a delayed write which can cause the index to become out-of-date
  // at any moment.  Therefore, we loop here touching the index file until
  // we confirm we got an up-to-date index file used in our check.
  do {
    base::Time t = base::Time::Now();
    EXPECT_TRUE(base::TouchFile(index_path, t, t));

    int64_t padded_size = GetBucketUsage(bucket_locator1_);
    EXPECT_GT(padded_size, unpadded_size);
  } while (!IsIndexFileCurrent(storage_dir));
}

TEST_F(CacheStorageManagerTest, GetBucketUsageWithOldIndex) {
  // Write a single value (V1) to the cache.
  const GURL kFooURL = storage_key1_.origin().GetURL().Resolve("foo");
  const std::string kCacheName = "foo";
  EXPECT_TRUE(Open(bucket_locator1_, kCacheName));
  CacheStorageCacheHandle original_handle = std::move(callback_cache_handle_);

  EXPECT_TRUE(CachePut(original_handle.value(), kFooURL));
  int64_t cache_size_v1 = Size(bucket_locator1_);
  base::FilePath storage_dir =
      CacheStorageCache::From(original_handle)->path().DirName();
  original_handle = CacheStorageCacheHandle();
  EXPECT_GE(cache_size_v1, 0);

  // Close the caches and cache manager.
  EXPECT_TRUE(FlushCacheStorageIndex(bucket_locator1_));
  DestroyStorageManager();

  // Save a copy of the V1 index.
  base::FilePath index_path =
      storage_dir.AppendASCII(CacheStorage::kIndexFileName);
  EXPECT_TRUE(base::PathExists(index_path));
  base::FilePath backup_index_path = storage_dir.AppendASCII("index.txt.bak");
  EXPECT_TRUE(base::CopyFile(index_path, backup_index_path));

  // Create a new CacheStorageManager that hasn't yet loaded the origin.
  CreateStorageManager();
  RecreateStorageManager();

  // Create a second value (V2) in the cache.
  EXPECT_TRUE(Open(bucket_locator1_, kCacheName));
  original_handle = std::move(callback_cache_handle_);
  const GURL kBarURL = storage_key1_.origin().GetURL().Resolve("bar");
  EXPECT_TRUE(CachePut(original_handle.value(), kBarURL));
  original_handle = CacheStorageCacheHandle();

  // Capture the size before the index has necessarily flushed to disk.
  int64_t usage_before_close = GetBucketUsage(bucket_locator1_);
  EXPECT_GT(usage_before_close, 0);

  // Flush the index to ensure we can read it correctly from the index file.
  EXPECT_TRUE(FlushCacheStorageIndex(bucket_locator1_));

  // Close the caches and cache manager.
  DestroyStorageManager();

  CreateStorageManager();
  RecreateStorageManager();

  // Read the size from the index file.
  CreateStorageManager();
  int64_t usage = GetBucketUsage(bucket_locator1_);
  EXPECT_EQ(usage_before_close, usage);

  DestroyStorageManager();

  // Restore the index to the V1 state. Make the access/mod times of index file
  // older than the other directories in the store to trigger size
  // recalculation.
  EXPECT_TRUE(base::CopyFile(backup_index_path, index_path));
  base::Time t = base::Time::Now() - base::Hours(1);
  EXPECT_TRUE(base::TouchFile(index_path, t, t));
  EXPECT_FALSE(IsIndexFileCurrent(storage_dir));

  // Read the size with the stale index file forcing a recalculation.
  CreateStorageManager();
  usage = GetBucketUsage(bucket_locator1_);
  EXPECT_EQ(usage_before_close, usage);
}

TEST_P(CacheStorageManagerTestP, GetStorageKeysIgnoresKeysFromNamedBuckets) {
  // `GetStorageKeys()` should only return storage keys associated with
  // default buckets, so create separate storage keys that we expect to not find
  // in the list returned by `GetStorageKeys()`.
  for (const bool partitioning_enabled : {false, true}) {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatureState(
        net::features::kThirdPartyStoragePartitioning, partitioning_enabled);

    url::Origin test_origin = url::Origin::Create(GURL("http://example4.com"));

    auto storage_key3 = blink::StorageKey::CreateFirstParty(test_origin);

    auto storage_key4 = blink::StorageKey::Create(
        test_origin, net::SchemefulSite(GURL("http://example5.com")),
        blink::mojom::AncestorChainBit::kCrossSite);

    ASSERT_OK_AND_ASSIGN(const storage::BucketLocator bucket_locator3,
                         GetOrCreateBucket(storage_key3, "non-default"));

    ASSERT_OK_AND_ASSIGN(const storage::BucketLocator bucket_locator4,
                         GetOrCreateBucket(storage_key4, "non-default"));

    GURL test_url = GURL("http://example.com/foo");

    EXPECT_TRUE(Open(bucket_locator1_, "foo"));
    EXPECT_TRUE(CachePut(callback_cache_handle_.value(), test_url));

    EXPECT_TRUE(Open(bucket_locator2_, "bar"));
    EXPECT_TRUE(CachePut(callback_cache_handle_.value(), test_url));

    EXPECT_TRUE(Open(bucket_locator3, "baz"));
    EXPECT_TRUE(CachePut(callback_cache_handle_.value(), test_url));

    EXPECT_TRUE(Open(bucket_locator4, "cool"));
    EXPECT_TRUE(CachePut(callback_cache_handle_.value(), test_url));

    // Ensure that the index files have been written to disk before calling
    // `GetStorageKeys()`.
    EXPECT_TRUE(FlushCacheStorageIndex(bucket_locator1_));
    EXPECT_TRUE(FlushCacheStorageIndex(bucket_locator2_));
    EXPECT_TRUE(FlushCacheStorageIndex(bucket_locator3));
    if (partitioning_enabled) {
      EXPECT_TRUE(FlushCacheStorageIndex(bucket_locator4));
    }
    disk_cache::FlushCacheThreadForTesting();
    content::RunAllTasksUntilIdle();

    std::vector<blink::StorageKey> storage_keys = GetStorageKeys();

    ASSERT_EQ(2ULL, storage_keys.size());
    EXPECT_NE(storage_keys[0].origin(), test_origin);
    EXPECT_NE(storage_keys[1].origin(), test_origin);

    EXPECT_EQ(DeleteBucketData(bucket_locator1_),
              blink::mojom::QuotaStatusCode::kOk);

    EXPECT_EQ(DeleteBucketData(bucket_locator2_),
              blink::mojom::QuotaStatusCode::kOk);

    EXPECT_EQ(DeleteBucketData(bucket_locator3),
              blink::mojom::QuotaStatusCode::kOk);

    EXPECT_EQ(DeleteBucketData(bucket_locator4),
              blink::mojom::QuotaStatusCode::kOk);

    ASSERT_EQ(0ULL, GetStorageKeys().size());
  }
}

TEST_F(CacheStorageManagerTest, GetKeySizeWithOldIndex) {
  // Write a single value (V1) to the cache.
  const GURL kFooURL = storage_key1_.origin().GetURL().Resolve("foo");
  const std::string kCacheName = "foo";
  EXPECT_TRUE(Open(bucket_locator1_, kCacheName));
  CacheStorageCacheHandle original_handle = std::move(callback_cache_handle_);

  EXPECT_TRUE(CachePut(original_handle.value(), kFooURL));
  int64_t cache_size_v1 = Size(bucket_locator1_);
  base::FilePath storage_dir =
      CacheStorageCache::From(original_handle)->path().DirName();
  original_handle = CacheStorageCacheHandle();
  EXPECT_GE(cache_size_v1, 0);

  // Close the caches and cache manager.
  EXPECT_TRUE(FlushCacheStorageIndex(bucket_locator1_));
  DestroyStorageManager();

  // Save a copy of the V1 index.
  base::FilePath index_path =
      storage_dir.AppendASCII(CacheStorage::kIndexFileName);
  EXPECT_TRUE(base::PathExists(index_path));
  base::FilePath backup_index_path = storage_dir.AppendASCII("index.txt.bak");
  EXPECT_TRUE(base::CopyFile(index_path, backup_index_path));

  // Create a new CacheStorageManager that hasn't yet loaded the origin.
  CreateStorageManager();
  RecreateStorageManager();

  // Reopen the cache and write a second value (V2).
  EXPECT_TRUE(Open(bucket_locator1_, kCacheName));
  original_handle = std::move(callback_cache_handle_);
  const GURL kBarURL = storage_key1_.origin().GetURL().Resolve("bar");
  EXPECT_TRUE(CachePut(original_handle.value(), kBarURL));
  original_handle = CacheStorageCacheHandle();
  int64_t cache_size_v2 = Size(bucket_locator1_);
  EXPECT_GE(cache_size_v2, 0);

  // Close the caches and cache manager.
  DestroyStorageManager();

  // Restore the index to the V1 state.
  EXPECT_TRUE(base::CopyFile(backup_index_path, index_path));

  // Make the access/mod times of index file older than the other files in the
  // cache to trigger size recalculation.
  base::Time t = base::Time::Now() - base::Hours(1);
  EXPECT_TRUE(base::TouchFile(index_path, t, t));
  EXPECT_FALSE(IsIndexFileCurrent(storage_dir));

  // Reopen the cache and ensure the size is correct for the V2 value.
  CreateStorageManager();
  EXPECT_TRUE(Open(bucket_locator1_, kCacheName));
  EXPECT_EQ(cache_size_v2, Size(bucket_locator1_));
}

TEST_P(CacheStorageManagerTestP, GetSizeThenCloseAllCaches) {
  EXPECT_TRUE(Open(bucket_locator1_, "foo"));
  EXPECT_TRUE(
      CachePut(callback_cache_handle_.value(), GURL("http://example.com/foo")));
  EXPECT_TRUE(CachePut(callback_cache_handle_.value(),
                       GURL("http://example.com/foo2")));
  EXPECT_TRUE(Open(bucket_locator1_, "bar"));
  EXPECT_TRUE(
      CachePut(callback_cache_handle_.value(), GURL("http://example.com/bar")));

  int64_t origin_size = GetBucketUsage(bucket_locator1_);
  EXPECT_LT(0, origin_size);

  EXPECT_EQ(origin_size, GetSizeThenCloseAllCaches(bucket_locator1_));
  EXPECT_FALSE(
      CachePut(callback_cache_handle_.value(), GURL("http://example.com/baz")));
}

TEST_P(CacheStorageManagerTestP, GetSizeThenCloseAllCachesTwoOwners) {
  EXPECT_TRUE(Open(bucket_locator1_, "foo",
                   storage::mojom::CacheStorageOwner::kCacheAPI));
  CacheStorageCacheHandle public_handle = std::move(callback_cache_handle_);
  EXPECT_TRUE(Open(bucket_locator1_, "foo",
                   storage::mojom::CacheStorageOwner::kBackgroundFetch));
  CacheStorageCacheHandle bgf_handle = std::move(callback_cache_handle_);

  EXPECT_TRUE(
      CachePut(public_handle.value(), GURL("http://example.com/public")));
  EXPECT_TRUE(CachePut(bgf_handle.value(), GURL("http://example.com/bgf")));

  int64_t origin_size = GetBucketUsage(bucket_locator1_);
  EXPECT_LT(0, origin_size);

  EXPECT_EQ(origin_size, GetSizeThenCloseAllCaches(bucket_locator1_));
  EXPECT_FALSE(CachePut(public_handle.value(), GURL("http://example.com/baz")));
}

TEST_P(CacheStorageManagerTestP, GetSizeThenCloseAllCachesAfterDelete) {
  // Tests that doomed caches are also deleted by GetSizeThenCloseAllCaches.
  EXPECT_TRUE(Open(bucket_locator1_, "foo"));
  EXPECT_TRUE(
      CachePut(callback_cache_handle_.value(), GURL("http://example.com/foo")));

  int64_t size_after_put = GetBucketUsage(bucket_locator1_);
  EXPECT_LT(0, size_after_put);

  // Keep a handle to a (soon-to-be deleted cache).
  auto saved_cache_handle = callback_cache_handle_.Clone();

  // Delete will only doom the cache because there is still at least one handle
  // referencing an open cache.
  EXPECT_TRUE(Delete(bucket_locator1_, "foo"));

  // GetSizeThenCloseAllCaches should close the cache (which is then deleted)
  // even though there is still an open handle.
  EXPECT_EQ(size_after_put, GetSizeThenCloseAllCaches(bucket_locator1_));
  EXPECT_EQ(0, GetBucketUsage(bucket_locator1_));
}

TEST_F(CacheStorageManagerTest, DeleteUnreferencedCacheDirectories) {
  // Create a referenced cache.
  EXPECT_TRUE(Open(bucket_locator1_, "foo"));
  EXPECT_TRUE(
      CachePut(callback_cache_handle_.value(), GURL("http://example.com/foo")));

  // Create an unreferenced directory next to the referenced one.
  auto* legacy_manager =
      static_cast<CacheStorageManager*>(cache_manager_.get());
  base::FilePath origin_path = CacheStorageManager::ConstructBucketPath(
      legacy_manager->profile_path(), bucket_locator1_,
      storage::mojom::CacheStorageOwner::kCacheAPI);
  base::FilePath unreferenced_path = origin_path.AppendASCII("bar");
  EXPECT_TRUE(CreateDirectory(unreferenced_path));
  EXPECT_TRUE(base::DirectoryExists(unreferenced_path));

  // Create a new StorageManager so that the next time the cache is opened the
  // unreferenced directory can be deleted.
  RecreateStorageManager();

  // Verify that the referenced cache still works.
  EXPECT_TRUE(Open(bucket_locator1_, "foo"));
  EXPECT_TRUE(CacheMatch(callback_cache_handle_.value(),
                         GURL("http://example.com/foo")));

  // Verify that the unreferenced cache is gone.
  EXPECT_FALSE(base::DirectoryExists(unreferenced_path));
}

TEST_P(CacheStorageManagerTestP, OpenCacheStorageAccessed) {
  EXPECT_EQ(0, quota_manager_proxy_->notify_bucket_accessed_count());
  EXPECT_TRUE(Open(bucket_locator1_, "foo"));
  EXPECT_EQ(1, quota_manager_proxy_->notify_bucket_accessed_count());
}

TEST_P(CacheStorageManagerTestP, HasStorageAccessed) {
  EXPECT_EQ(0, quota_manager_proxy_->notify_bucket_accessed_count());
  EXPECT_FALSE(Has(bucket_locator1_, "foo"));
  EXPECT_EQ(1, quota_manager_proxy_->notify_bucket_accessed_count());
}

TEST_P(CacheStorageManagerTestP, DeleteStorageAccessed) {
  EXPECT_EQ(0, quota_manager_proxy_->notify_bucket_accessed_count());
  EXPECT_FALSE(Delete(bucket_locator1_, "foo"));
  EXPECT_EQ(1, quota_manager_proxy_->notify_bucket_accessed_count());
}

TEST_P(CacheStorageManagerTestP, KeysStorageAccessed) {
  EXPECT_EQ(0, quota_manager_proxy_->notify_bucket_accessed_count());
  EXPECT_EQ(0u, Keys(bucket_locator1_));
  EXPECT_EQ(1, quota_manager_proxy_->notify_bucket_accessed_count());
}

TEST_P(CacheStorageManagerTestP, MatchCacheStorageAccessed) {
  EXPECT_EQ(0, quota_manager_proxy_->notify_bucket_accessed_count());
  EXPECT_FALSE(
      StorageMatch(bucket_locator1_, "foo", GURL("http://example.com/foo")));
  EXPECT_EQ(1, quota_manager_proxy_->notify_bucket_accessed_count());
}

TEST_P(CacheStorageManagerTestP, MatchAllCachesStorageAccessed) {
  EXPECT_EQ(0, quota_manager_proxy_->notify_bucket_accessed_count());
  EXPECT_FALSE(
      StorageMatchAll(bucket_locator1_, GURL("http://example.com/foo")));
  EXPECT_EQ(1, quota_manager_proxy_->notify_bucket_accessed_count());
}

TEST_P(CacheStorageManagerTestP, SizeStorageAccessed) {
  EXPECT_EQ(0, Size(bucket_locator1_));
  // Size is not part of the web API and should not notify the quota manager of
  // an access.
  EXPECT_EQ(0, quota_manager_proxy_->notify_bucket_accessed_count());
}

TEST_P(CacheStorageManagerTestP, SizeThenCloseStorageAccessed) {
  EXPECT_EQ(0, GetSizeThenCloseAllCaches(bucket_locator1_));
  // GetSizeThenCloseAllCaches is not part of the web API and should not notify
  // the quota manager of an access.
  EXPECT_EQ(0, quota_manager_proxy_->notify_bucket_accessed_count());
}

TEST_P(CacheStorageManagerTestP, NotifyCacheListChanged_Created) {
  auto observer = CreateObserver();

  EXPECT_EQ(0, observer->notify_list_changed_count);
  EXPECT_TRUE(Open(bucket_locator1_, "foo"));
  observer->Wait();
  EXPECT_EQ(1, observer->notify_list_changed_count);
  EXPECT_TRUE(
      CachePut(callback_cache_handle_.value(), GURL("http://example.com/foo")));
  observer->Wait();
  EXPECT_EQ(1, observer->notify_list_changed_count);
}

TEST_P(CacheStorageManagerTestP, NotifyCacheListChanged_Deleted) {
  auto observer = CreateObserver();

  EXPECT_EQ(0, observer->notify_list_changed_count);
  EXPECT_FALSE(Delete(bucket_locator1_, "foo"));
  // Give any unexpected observer tasks a chance to run.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, observer->notify_list_changed_count);
  EXPECT_TRUE(Open(bucket_locator1_, "foo"));
  observer->Wait();
  EXPECT_EQ(1, observer->notify_list_changed_count);
  EXPECT_TRUE(Delete(bucket_locator1_, "foo"));
  observer->Wait();
  EXPECT_EQ(2, observer->notify_list_changed_count);
}

TEST_P(CacheStorageManagerTestP, NotifyCacheListChanged_DeletedThenCreated) {
  auto observer = CreateObserver();

  EXPECT_EQ(0, observer->notify_list_changed_count);
  EXPECT_TRUE(Open(bucket_locator1_, "foo"));
  observer->Wait();
  EXPECT_EQ(1, observer->notify_list_changed_count);
  EXPECT_TRUE(Delete(bucket_locator1_, "foo"));
  observer->Wait();
  EXPECT_EQ(2, observer->notify_list_changed_count);
  EXPECT_TRUE(Open(bucket_locator1_, "foo2"));
  observer->Wait();
  EXPECT_EQ(3, observer->notify_list_changed_count);
}

TEST_P(CacheStorageManagerTestP, NotifyCacheContentChanged_PutEntry) {
  auto observer = CreateObserver();

  EXPECT_EQ(0, observer->notify_content_changed_count);
  EXPECT_TRUE(Open(bucket_locator1_, "foo"));
  observer->Wait();
  EXPECT_EQ(0, observer->notify_content_changed_count);
  EXPECT_TRUE(
      CachePut(callback_cache_handle_.value(), GURL("http://example.com/foo")));
  observer->Wait();
  EXPECT_EQ(1, observer->notify_content_changed_count);
  EXPECT_TRUE(CachePut(callback_cache_handle_.value(),
                       GURL("http://example.com/foo1")));
  observer->Wait();
  EXPECT_TRUE(CachePut(callback_cache_handle_.value(),
                       GURL("http://example.com/foo2")));
  observer->Wait();
  EXPECT_EQ(3, observer->notify_content_changed_count);
}

TEST_P(CacheStorageManagerTestP, NotifyCacheContentChanged_DeleteEntry) {
  auto observer = CreateObserver();

  EXPECT_EQ(0, observer->notify_content_changed_count);
  EXPECT_FALSE(Delete(bucket_locator1_, "foo"));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, observer->notify_content_changed_count);
  EXPECT_TRUE(Open(bucket_locator1_, "foo"));
  observer->Wait();
  EXPECT_EQ(0, observer->notify_content_changed_count);
  EXPECT_TRUE(
      CachePut(callback_cache_handle_.value(), GURL("http://example.com/foo")));
  observer->Wait();
  EXPECT_EQ(1, observer->notify_content_changed_count);
  EXPECT_TRUE(CacheDelete(callback_cache_handle_.value(),
                          GURL("http://example.com/foo")));
  observer->Wait();
  EXPECT_EQ(2, observer->notify_content_changed_count);
  EXPECT_FALSE(CacheDelete(callback_cache_handle_.value(),
                           GURL("http://example.com/foo")));
  // Give any unexpected observer tasks a chance to run.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, observer->notify_content_changed_count);
}

TEST_P(CacheStorageManagerTestP, NotifyCacheContentChanged_DeleteThenPutEntry) {
  auto observer = CreateObserver();

  EXPECT_EQ(0, observer->notify_content_changed_count);
  EXPECT_TRUE(Open(bucket_locator1_, "foo"));
  observer->Wait();
  EXPECT_EQ(0, observer->notify_content_changed_count);
  EXPECT_TRUE(
      CachePut(callback_cache_handle_.value(), GURL("http://example.com/foo")));
  observer->Wait();
  EXPECT_EQ(1, observer->notify_content_changed_count);
  EXPECT_TRUE(CacheDelete(callback_cache_handle_.value(),
                          GURL("http://example.com/foo")));
  observer->Wait();
  EXPECT_EQ(2, observer->notify_content_changed_count);
  EXPECT_TRUE(
      CachePut(callback_cache_handle_.value(), GURL("http://example.com/foo")));
  observer->Wait();
  EXPECT_EQ(3, observer->notify_content_changed_count);
  EXPECT_TRUE(CacheDelete(callback_cache_handle_.value(),
                          GURL("http://example.com/foo")));
  observer->Wait();
  EXPECT_EQ(4, observer->notify_content_changed_count);
}

TEST_P(CacheStorageManagerTestP, StorageMatch_IgnoreSearch) {
  EXPECT_TRUE(Open(bucket_locator1_, "foo"));
  EXPECT_TRUE(CachePut(callback_cache_handle_.value(),
                       GURL("http://example.com/foo?bar")));

  EXPECT_FALSE(
      StorageMatch(bucket_locator1_, "foo", GURL("http://example.com/foo")));

  blink::mojom::CacheQueryOptionsPtr match_options =
      blink::mojom::CacheQueryOptions::New();
  match_options->ignore_search = true;
  EXPECT_TRUE(StorageMatch(bucket_locator1_, "foo",
                           GURL("http://example.com/foo"),
                           std::move(match_options)));
}

TEST_P(CacheStorageManagerTestP, StorageMatch_IgnoreMethod) {
  GURL url = GURL("http://example.com/foo");
  EXPECT_TRUE(Open(bucket_locator1_, "foo"));
  EXPECT_TRUE(CachePut(callback_cache_handle_.value(), url));

  auto post_request = blink::mojom::FetchAPIRequest::New();
  post_request->url = url;
  post_request->method = "POST";
  EXPECT_FALSE(StorageMatchWithRequest(
      bucket_locator1_, "foo",
      BackgroundFetchSettledFetch::CloneRequest(post_request)));

  blink::mojom::CacheQueryOptionsPtr match_options =
      blink::mojom::CacheQueryOptions::New();
  match_options->ignore_method = true;
  EXPECT_TRUE(StorageMatchWithRequest(bucket_locator1_, "foo",
                                      std::move(post_request),
                                      std::move(match_options)));
}

TEST_P(CacheStorageManagerTestP, StorageMatch_IgnoreVary) {
  GURL url = GURL("http://example.com/foo");
  EXPECT_TRUE(Open(bucket_locator1_, "foo"));

  auto request = blink::mojom::FetchAPIRequest::New();
  request->url = url;
  request->headers["vary_foo"] = "foo";
  // |request_1| and |request_2| has the same url and headers.
  auto request_1 = BackgroundFetchSettledFetch::CloneRequest(request);
  auto request_2 = BackgroundFetchSettledFetch::CloneRequest(request);

  ResponseHeaderMap response_headers;
  response_headers["vary"] = "vary_foo";

  EXPECT_TRUE(CachePutWithRequestAndHeaders(callback_cache_handle_.value(),
                                            std::move(request_1),
                                            std::move(response_headers)));
  EXPECT_TRUE(
      StorageMatchWithRequest(bucket_locator1_, "foo", std::move(request_2)));

  request->headers["vary_foo"] = "bar";
  // |request_3| and |request_4| has the same url and headers.
  auto request_3 = BackgroundFetchSettledFetch::CloneRequest(request);
  auto request_4 = BackgroundFetchSettledFetch::CloneRequest(request);
  EXPECT_FALSE(
      StorageMatchWithRequest(bucket_locator1_, "foo", std::move(request_3)));

  blink::mojom::CacheQueryOptionsPtr match_options =
      blink::mojom::CacheQueryOptions::New();
  match_options->ignore_vary = true;
  EXPECT_TRUE(StorageMatchWithRequest(
      bucket_locator1_, "foo", std::move(request_4), std::move(match_options)));
}

TEST_P(CacheStorageManagerTestP, StorageMatchAll_IgnoreSearch) {
  EXPECT_TRUE(Open(bucket_locator1_, "foo"));
  EXPECT_TRUE(CachePut(callback_cache_handle_.value(),
                       GURL("http://example.com/foo?bar")));

  EXPECT_FALSE(
      StorageMatchAll(bucket_locator1_, GURL("http://example.com/foo")));

  blink::mojom::CacheQueryOptionsPtr match_options =
      blink::mojom::CacheQueryOptions::New();
  match_options->ignore_search = true;
  EXPECT_TRUE(StorageMatchAll(bucket_locator1_, GURL("http://example.com/foo"),
                              std::move(match_options)));
}

TEST_P(CacheStorageManagerTestP, StorageMatchAll_IgnoreMethod) {
  GURL url = GURL("http://example.com/foo");
  EXPECT_TRUE(Open(bucket_locator1_, "foo"));
  EXPECT_TRUE(CachePut(callback_cache_handle_.value(), url));

  auto post_request = blink::mojom::FetchAPIRequest::New();
  post_request->url = url;
  post_request->method = "POST";
  EXPECT_FALSE(StorageMatchAllWithRequest(
      bucket_locator1_,
      BackgroundFetchSettledFetch::CloneRequest(post_request)));

  blink::mojom::CacheQueryOptionsPtr match_options =
      blink::mojom::CacheQueryOptions::New();
  match_options->ignore_method = true;
  EXPECT_TRUE(StorageMatchAllWithRequest(
      bucket_locator1_, std::move(post_request), std::move(match_options)));
}

TEST_P(CacheStorageManagerTestP, StorageMatchAll_IgnoreVary) {
  GURL url = GURL("http://example.com/foo");
  EXPECT_TRUE(Open(bucket_locator1_, "foo"));

  auto request = blink::mojom::FetchAPIRequest::New();
  request->url = url;
  request->headers["vary_foo"] = "foo";
  // |request_1| and |request_2| has the same url and headers.
  auto request_1 = BackgroundFetchSettledFetch::CloneRequest(request);
  auto request_2 = BackgroundFetchSettledFetch::CloneRequest(request);

  ResponseHeaderMap response_headers;
  response_headers["vary"] = "vary_foo";

  EXPECT_TRUE(CachePutWithRequestAndHeaders(
      callback_cache_handle_.value(), std::move(request_1), response_headers));
  EXPECT_TRUE(
      StorageMatchAllWithRequest(bucket_locator1_, std::move(request_2)));

  request->headers["vary_foo"] = "bar";
  // |request_3| and |request_4| has the same url and headers.
  auto request_3 = BackgroundFetchSettledFetch::CloneRequest(request);
  auto request_4 = BackgroundFetchSettledFetch::CloneRequest(request);
  EXPECT_FALSE(
      StorageMatchAllWithRequest(bucket_locator1_, std::move(request_3)));

  blink::mojom::CacheQueryOptionsPtr match_options =
      blink::mojom::CacheQueryOptions::New();
  match_options->ignore_vary = true;
  EXPECT_TRUE(StorageMatchAllWithRequest(bucket_locator1_, std::move(request_4),
                                         std::move(match_options)));
}

TEST_P(CacheStorageManagerTestP, StorageWriteToCache) {
  EXPECT_TRUE(Open(bucket_locator1_, "foo",
                   storage::mojom::CacheStorageOwner::kBackgroundFetch));

  EXPECT_TRUE(Write(bucket_locator1_,
                    storage::mojom::CacheStorageOwner::kBackgroundFetch, "foo",
                    "http://example.com/foo"));

  // Match request we just wrote.
  EXPECT_TRUE(StorageMatch(
      bucket_locator1_, "foo", GURL("http://example.com/foo"), nullptr,
      storage::mojom::CacheStorageOwner::kBackgroundFetch));

  // Don't match with different origin.
  EXPECT_FALSE(StorageMatch(
      bucket_locator2_, "foo", GURL("http://example.com/foo"), nullptr,
      storage::mojom::CacheStorageOwner::kBackgroundFetch));
  // Don't match with different cache name.
  EXPECT_FALSE(StorageMatch(
      bucket_locator1_, "bar", GURL("http://example.com/foo"), nullptr,
      storage::mojom::CacheStorageOwner::kBackgroundFetch));
  // Don't match with different request.
  EXPECT_FALSE(StorageMatch(
      bucket_locator1_, "foo", GURL("http://example.com/bar"), nullptr,
      storage::mojom::CacheStorageOwner::kBackgroundFetch));
  // Don't match with different owner.
  EXPECT_FALSE(StorageMatch(bucket_locator1_, "foo",
                            GURL("http://example.com/foo"), nullptr,
                            storage::mojom::CacheStorageOwner::kCacheAPI));
}

TEST_F(CacheStorageManagerTest, WriteIndexOnlyScheduledWhenValueChanges) {
  const char* kCacheName = "WriteIndexOnlyScheduledWhenValueChanges";

  // In order to make sure operations are flushed through the various scheduler
  // queues we match against a non-existant URL.  This should always return
  // false from Match() and when it completes we know any previous operations
  // have completed as well.
  const GURL kNonExistant("http://example.com/bar");
  const GURL kResource("https://example.com/foo");

  // Opening a new cache should require writing the index.
  EXPECT_TRUE(Open(bucket_locator1_, kCacheName));
  EXPECT_FALSE(CacheMatch(callback_cache_handle_.value(), kNonExistant));
  EXPECT_TRUE(FlushCacheStorageIndex(bucket_locator1_));

  // Allow the cache to close.
  callback_cache_handle_ = CacheStorageCacheHandle();

  // Opening an existing cache should *not* require writing the index.
  EXPECT_TRUE(Open(bucket_locator1_, kCacheName));
  EXPECT_FALSE(CacheMatch(callback_cache_handle_.value(), kNonExistant));
  EXPECT_FALSE(FlushCacheStorageIndex(bucket_locator1_));

  // Putting a value in the cache should require writing the index.
  EXPECT_TRUE(CachePut(callback_cache_handle_.value(), kResource));
  EXPECT_TRUE(FlushCacheStorageIndex(bucket_locator1_));

  // Allow the cache to close.
  callback_cache_handle_ = CacheStorageCacheHandle();

  // Opening an existing cache after writing a value should *not* require
  // writing the index.
  EXPECT_TRUE(Open(bucket_locator1_, kCacheName));
  EXPECT_FALSE(CacheMatch(callback_cache_handle_.value(), kNonExistant));
  EXPECT_TRUE(CacheMatch(callback_cache_handle_.value(), kResource));
  EXPECT_FALSE(FlushCacheStorageIndex(bucket_locator1_));
}

TEST_P(CacheStorageManagerTestP, SlowPutCompletesWithoutExternalRef) {
  EXPECT_TRUE(Open(bucket_locator1_, "foo"));

  auto request = blink::mojom::FetchAPIRequest::New();
  request->url = GURL("http://example.com/foo");

  // Start defining a blob for the response body.
  std::string body_data("hello world");
  auto blob = blink::mojom::SerializedBlob::New();
  blob->uuid = "mock blob";
  blob->size = body_data.length();

  // Provide a fake blob implementation that delays completion.  This will
  // allow us to pause the writing operation so we can drop the external
  // reference.
  base::RunLoop blob_loop;
  DelayedBlob delayed_blob(blob->blob.InitWithNewPipeAndPassReceiver(),
                           body_data, blob_loop.QuitClosure());

  // Begin the operation to write the blob into the cache.
  base::RunLoop cache_loop;
  CachePutWithStatusCodeAndBlobInternal(callback_cache_handle_.value(),
                                        std::move(request), 200,
                                        std::move(blob), &cache_loop);

  // Wait for blob's ReadAll() method to be called.
  blob_loop.Run();

  // Drop the external reference to the cache.  The operation should hold
  // itself alive, but previous versions of the code would destroy the cache
  // immediately at this step.
  callback_cache_handle_ = CacheStorageCacheHandle();

  // Signal the blob to complete reading.
  delayed_blob.Resume();

  // Wait for the cache write operation to be complete and verify that it
  // succeeded.
  cache_loop.Run();
  EXPECT_EQ(CacheStorageError::kSuccess, callback_error_);
}

TEST_P(CacheStorageManagerTestP, StoragePutPartialContentForBackgroundFetch) {
  EXPECT_TRUE(Open(bucket_locator1_, "foo",
                   storage::mojom::CacheStorageOwner::kBackgroundFetch));
  auto request = blink::mojom::FetchAPIRequest::New();
  request->url = GURL("http://example.com/foo");
  auto request_clone = BackgroundFetchSettledFetch::CloneRequest(request);

  EXPECT_TRUE(CachePutWithStatusCode(callback_cache_handle_.value(),
                                     std::move(request), 206));
  EXPECT_TRUE(StorageMatchAllWithRequest(
      bucket_locator1_, std::move(request_clone),
      /* match_options= */ nullptr,
      storage::mojom::CacheStorageOwner::kBackgroundFetch));
  EXPECT_EQ(206, callback_cache_handle_response_->status_code);
}

TEST_P(CacheStorageManagerTestP, DeleteOriginDataEmptyList) {
  std::set<url::Origin> empty_list;
  for (const storage::mojom::CacheStorageOwner owner :
       {storage::mojom::CacheStorageOwner::kCacheAPI,
        storage::mojom::CacheStorageOwner::kBackgroundFetch}) {
    EXPECT_EQ(DeleteOriginData(empty_list, owner),
              blink::mojom::QuotaStatusCode::kOk);
  }
}

TEST_P(CacheStorageManagerTestP, BatchDeleteOriginData) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      net::features::kThirdPartyStoragePartitioning);

  const auto partitioned_storage_key1 = blink::StorageKey::Create(
      url::Origin::Create(GURL("http://example1.com")),
      net::SchemefulSite(GURL("http://example3.com")),
      blink::mojom::AncestorChainBit::kCrossSite);

  ASSERT_OK_AND_ASSIGN(
      const auto partitioned_default_bucket_locator1,
      GetOrCreateBucket(partitioned_storage_key1, storage::kDefaultBucketName));

  GURL test_url = GURL("http://example.com/foo");

  for (const storage::mojom::CacheStorageOwner owner :
       {storage::mojom::CacheStorageOwner::kCacheAPI,
        storage::mojom::CacheStorageOwner::kBackgroundFetch}) {
    EXPECT_TRUE(Open(bucket_locator1_, "foo", owner));
    EXPECT_TRUE(CachePut(callback_cache_handle_.value(), test_url));

    EXPECT_TRUE(Open(bucket_locator2_, "foo", owner));
    EXPECT_TRUE(CachePut(callback_cache_handle_.value(), test_url));

    EXPECT_TRUE(Open(partitioned_default_bucket_locator1, "baz", owner));
    EXPECT_TRUE(CachePut(callback_cache_handle_.value(), test_url));

    EXPECT_EQ(3ULL, GetStorageKeys(owner).size());

    std::set<url::Origin> to_delete = {storage_key1_.origin(),
                                       storage_key2_.origin()};

    EXPECT_EQ(DeleteOriginData(to_delete, owner),
              blink::mojom::QuotaStatusCode::kOk);

    auto storage_keys = GetStorageKeys(owner);
    EXPECT_EQ(0ULL, storage_keys.size());
  }
}

TEST_F(CacheStorageManagerMemoryOnlyTest, DeleteBucketData) {
  for (const storage::mojom::CacheStorageOwner owner :
       {storage::mojom::CacheStorageOwner::kCacheAPI,
        storage::mojom::CacheStorageOwner::kBackgroundFetch}) {
    EXPECT_TRUE(Open(bucket_locator1_, "foo", owner));
    EXPECT_TRUE(CachePut(callback_cache_handle_.value(),
                         GURL("http://example.com/foo")));

    EXPECT_LT(0, GetBucketUsage(bucket_locator1_, owner));
    EXPECT_EQ(DeleteBucketData(bucket_locator1_, owner),
              blink::mojom::QuotaStatusCode::kOk);
    EXPECT_EQ(0, GetBucketUsage(bucket_locator1_, owner));
  }
}

TEST_P(CacheStorageManagerStorageKeyAndBucketTestP, DeleteBucketData) {
  for (const storage::mojom::CacheStorageOwner owner :
       {storage::mojom::CacheStorageOwner::kCacheAPI,
        storage::mojom::CacheStorageOwner::kBackgroundFetch}) {
    EXPECT_TRUE(Open(bucket_locator1_, "foo", owner));
    EXPECT_TRUE(CachePut(callback_cache_handle_.value(),
                         GURL("http://example.com/foo")));

    EXPECT_LT(0, GetBucketUsage(bucket_locator1_, owner));

    base::FilePath calculated_path = CacheStorageManager::ConstructBucketPath(
        cache_manager_->profile_path(), bucket_locator1_, owner);

    EXPECT_TRUE(base::PathExists(calculated_path));

    EXPECT_EQ(DeleteBucketData(bucket_locator1_, owner),
              blink::mojom::QuotaStatusCode::kOk);
    EXPECT_EQ(0, GetBucketUsage(bucket_locator1_, owner));

    EXPECT_FALSE(base::PathExists(calculated_path));
  }
}

TEST_F(CacheStorageManagerTest, DeleteBucketDataConflictingBucketIds) {
  // For an overview of this test, see `GetBucketUsageConflictingBucketIds()`.
  const GURL kTestURL = GURL("http://example.com/foo");
  auto modified_bucket_locator1_{bucket_locator1_};
  modified_bucket_locator1_.id = storage::BucketId::FromUnsafeValue(999);

  base::FilePath bucket_locator1_path =
      CacheStorageManager::ConstructBucketPath(
          cache_manager_->profile_path(), bucket_locator1_,
          storage::mojom::CacheStorageOwner::kCacheAPI);

  base::FilePath modified_bucket_locator1_path =
      CacheStorageManager::ConstructBucketPath(
          cache_manager_->profile_path(), modified_bucket_locator1_,
          storage::mojom::CacheStorageOwner::kCacheAPI);

  ASSERT_EQ(bucket_locator1_path, modified_bucket_locator1_path);

  EXPECT_TRUE(Open(bucket_locator1_, "foo"));
  EXPECT_TRUE(CachePut(callback_cache_handle_.value(), kTestURL));

  disk_cache::FlushCacheThreadForTesting();
  content::RunAllTasksUntilIdle();

  // We are testing that this call doesn't hang and that it returns kOk without
  // actually deleting the directory (which is currently in use).
  EXPECT_EQ(DeleteBucketData(modified_bucket_locator1_),
            blink::mojom::QuotaStatusCode::kOk);
  ASSERT_TRUE(base::PathExists(bucket_locator1_path));

  // Clear out the CacheStorageManager's instance map.
  DestroyStorageManager();
  CreateStorageManager();

  // Now try the reverse order when there's an existing index file on disk but
  // there's no corresponding instance in the CacheStorage map. There should
  // already be an index file on disk leftover from the test above. In this
  // case, the `DeleteBucketData()` call should succeed and a subsequent usage
  // with the correct bucket locator should work as well.
  ASSERT_TRUE(base::PathExists(bucket_locator1_path));
  EXPECT_EQ(DeleteBucketData(modified_bucket_locator1_),
            blink::mojom::QuotaStatusCode::kOk);

  disk_cache::FlushCacheThreadForTesting();
  content::RunAllTasksUntilIdle();

  ASSERT_FALSE(base::PathExists(bucket_locator1_path));

  EXPECT_TRUE(Open(bucket_locator1_, "foo"));
  EXPECT_FALSE(CacheMatch(callback_cache_handle_.value(), kTestURL));
  EXPECT_EQ(Size(bucket_locator1_), 0);
}

class CacheStorageQuotaClientTest : public CacheStorageManagerTest {
 public:
  CacheStorageQuotaClientTest(const CacheStorageQuotaClientTest&) = delete;
  CacheStorageQuotaClientTest& operator=(const CacheStorageQuotaClientTest&) =
      delete;

 protected:
  CacheStorageQuotaClientTest() = default;

  void SetUp() override {
    CacheStorageManagerTest::SetUp();
    quota_client_ = std::make_unique<CacheStorageQuotaClient>(
        cache_manager_, storage::mojom::CacheStorageOwner::kCacheAPI);
  }

  void QuotaUsageCallback(base::RunLoop* run_loop, int64_t usage) {
    callback_quota_usage_ = usage;
    run_loop->Quit();
  }

  void StorageKeysCallback(base::RunLoop* run_loop,
                           const std::vector<blink::StorageKey>& storage_keys) {
    callback_storage_keys_ = storage_keys;
    run_loop->Quit();
  }

  void DeleteStorageKeyCallback(base::RunLoop* run_loop,
                                blink::mojom::QuotaStatusCode status) {
    callback_status_ = status;
    run_loop->Quit();
  }

  int64_t QuotaGetBucketUsage(const storage::BucketLocator& bucket) {
    base::test::TestFuture<int64_t> future;
    quota_client_->GetBucketUsage(bucket, future.GetCallback());
    return future.Get();
  }

  size_t QuotaGetStorageKeysForType() {
    base::RunLoop loop;
    quota_client_->GetStorageKeysForType(
        StorageType::kTemporary,
        base::BindOnce(&CacheStorageQuotaClientTest::StorageKeysCallback,
                       base::Unretained(this), base::Unretained(&loop)));
    loop.Run();
    return callback_storage_keys_.size();
  }

  bool QuotaDeleteBucketData(const storage::BucketLocator& bucket) {
    base::test::TestFuture<blink::mojom::QuotaStatusCode> future;
    quota_client_->DeleteBucketData(bucket, future.GetCallback());
    return future.Get() == blink::mojom::QuotaStatusCode::kOk;
  }

  std::unique_ptr<CacheStorageQuotaClient> quota_client_;

  blink::mojom::QuotaStatusCode callback_status_;
  int64_t callback_quota_usage_ = 0;
  std::vector<blink::StorageKey> callback_storage_keys_;
};

class CacheStorageQuotaClientDiskOnlyTest : public CacheStorageQuotaClientTest {
 public:
  bool MemoryOnly() override { return false; }
};

class CacheStorageQuotaClientTestP
    : public CacheStorageQuotaClientTest,
      public testing::WithParamInterface<TestStorage> {
  bool MemoryOnly() override { return GetParam() == TestStorage::kMemory; }
};

TEST_P(CacheStorageQuotaClientTestP, QuotaGetBucketUsage) {
  EXPECT_EQ(0, QuotaGetBucketUsage(bucket_locator1_));
  EXPECT_TRUE(Open(bucket_locator1_, "foo"));
  EXPECT_TRUE(
      CachePut(callback_cache_handle_.value(), GURL("http://example.com/foo")));
  EXPECT_LT(0, QuotaGetBucketUsage(bucket_locator1_));
}

TEST_P(CacheStorageQuotaClientTestP, QuotaGetStorageKeysForType) {
  EXPECT_EQ(0u, QuotaGetStorageKeysForType());
  EXPECT_TRUE(Open(bucket_locator1_, "foo"));
  EXPECT_TRUE(Open(bucket_locator1_, "bar"));
  EXPECT_TRUE(Open(bucket_locator2_, "foo"));
  EXPECT_EQ(2u, QuotaGetStorageKeysForType());
}

TEST_P(CacheStorageQuotaClientTestP,
       QuotaGetStorageKeysForTypeDifferentOwners) {
  EXPECT_EQ(0u, QuotaGetStorageKeysForType());
  EXPECT_TRUE(Open(bucket_locator1_, "foo"));
  // The |quota_client_| is registered for
  // storage::mojom::CacheStorageOwner::kCacheAPI, so this Open is ignored.
  EXPECT_TRUE(Open(bucket_locator2_, "bar",
                   storage::mojom::CacheStorageOwner::kBackgroundFetch));
  EXPECT_EQ(1u, QuotaGetStorageKeysForType());
}

TEST_P(CacheStorageQuotaClientTestP, QuotaDeleteBucketData) {
  EXPECT_EQ(0, QuotaGetBucketUsage(bucket_locator1_));
  EXPECT_TRUE(Open(bucket_locator1_, "foo"));
  // Call put to test that initialized caches are properly deleted too.
  EXPECT_TRUE(
      CachePut(callback_cache_handle_.value(), GURL("http://example.com/foo")));
  EXPECT_TRUE(Open(bucket_locator1_, "bar"));
  EXPECT_TRUE(Open(bucket_locator2_, "baz"));

  int64_t storage_key1_size = QuotaGetBucketUsage(bucket_locator1_);
  EXPECT_LT(0, storage_key1_size);

  EXPECT_TRUE(QuotaDeleteBucketData(bucket_locator1_));

  EXPECT_EQ(-1 * storage_key1_size,
            quota_manager_proxy_->last_notified_bucket_delta());
  EXPECT_EQ(0, QuotaGetBucketUsage(bucket_locator1_));
  EXPECT_FALSE(Has(bucket_locator1_, "foo"));
  EXPECT_FALSE(Has(bucket_locator1_, "bar"));
  EXPECT_TRUE(Has(bucket_locator2_, "baz"));
  EXPECT_TRUE(Open(bucket_locator1_, "foo"));
}

TEST_P(CacheStorageQuotaClientTestP, QuotaNonDefaultBucket) {
  ASSERT_OK_AND_ASSIGN(auto bucket,
                       GetOrCreateBucket(storage_key1_, "logs_bucket"));
  EXPECT_EQ(0, QuotaGetBucketUsage(bucket));
  EXPECT_TRUE(QuotaDeleteBucketData(bucket));
}

TEST_P(CacheStorageQuotaClientTestP, QuotaDeleteEmptyBucket) {
  EXPECT_TRUE(QuotaDeleteBucketData(bucket_locator1_));
}

TEST_F(CacheStorageQuotaClientDiskOnlyTest, QuotaDeleteUnloadedKeyData) {
  EXPECT_TRUE(Open(bucket_locator1_, "foo"));
  // Call put to test that initialized caches are properly deleted too.
  EXPECT_TRUE(
      CachePut(callback_cache_handle_.value(), GURL("http://example.com/foo")));

  // Close the cache backend so that it writes out its index to disk.
  base::RunLoop run_loop;
  CacheStorageCache::From(callback_cache_handle_)
      ->Close(run_loop.QuitClosure());
  run_loop.Run();

  // Create a new CacheStorageManager that hasn't yet loaded the origin.
  RecreateStorageManager();
  quota_client_ = std::make_unique<CacheStorageQuotaClient>(
      cache_manager_, storage::mojom::CacheStorageOwner::kCacheAPI);

  EXPECT_TRUE(QuotaDeleteBucketData(bucket_locator1_));
  EXPECT_EQ(0, QuotaGetBucketUsage(bucket_locator1_));
}

class CacheStorageIndexMigrationTest : public CacheStorageManagerTest {
 public:
  void DoTest(std::string test_index_path,
              base::RepeatingCallback<void(const proto::CacheStorageIndex&,
                                           const proto::CacheStorageIndex&,
                                           int64_t)> test_logic) {
    std::vector<blink::StorageKey> storage_keys = GetStorageKeys();
    ASSERT_TRUE(storage_keys.empty());

    // Create an empty directory for the cache_storage files.
    auto* legacy_manager =
        static_cast<CacheStorageManager*>(cache_manager_.get());
    base::FilePath profile_path = legacy_manager->profile_path();
    base::FilePath storage_dir = CacheStorageManager::ConstructBucketPath(
        profile_path, bucket_locator1_,
        storage::mojom::CacheStorageOwner::kCacheAPI);
    EXPECT_TRUE(base::CreateDirectory(
        CacheStorageManager::ConstructFirstPartyDefaultRootPath(profile_path)));
    EXPECT_TRUE(base::CreateDirectory(
        CacheStorageManager::ConstructThirdPartyAndNonDefaultRootPath(
            profile_path)));

    // Destroy the manager while we operate on the underlying files.
    DestroyStorageManager();

    // Determine the location of the old, frozen copy of the cache_storage
    // files in the test data.
    base::FilePath root_path;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &root_path);
    base::FilePath test_data_path =
        root_path.AppendASCII(test_index_path).Append(storage_dir.BaseName());

    // Copy the old files into the test storage directory.
    EXPECT_TRUE(base::CopyDirectory(test_data_path, storage_dir.DirName(),
                                    /*recursive=*/true));

    // Read the index file from disk.
    base::FilePath index_path =
        storage_dir.AppendASCII(CacheStorage::kIndexFileName);
    std::string protobuf;
    EXPECT_TRUE(base::ReadFileToString(index_path, &protobuf));
    proto::CacheStorageIndex original_index;
    EXPECT_TRUE(original_index.ParseFromString(protobuf));

    // Re-create the manager and ask it for all storage keys with CacheStorage
    // instances. This tests two things:
    //  - For index files without bucket information, CacheStorageManager will
    //    lookup or create corresponding buckets and then,
    //  - Open each Cache Storage instance to recompute its size, triggering a
    //    migration of its index file on disk.
    CreateStorageManager();
    GetStorageKeys();

    // Get bucket usage after bucket size has been recomputed.
    int64_t bucket_usage = GetBucketUsage(bucket_locator1_);

    // Destroy the manager and then ensure that all tasks have completed before
    // continuing. We can't rely on `FlushCacheStorageIndex()` here because it
    // only ensures that we've kicked off the task to initiate a write via the
    // `SimpleCacheLoader`, but it doesn't ensure that that task (and subsequent
    // ones) have completed.
    DestroyStorageManager();
    disk_cache::FlushCacheThreadForTesting();
    content::RunAllTasksUntilIdle();

    // Read the newly modified index off of disk.
    std::string protobuf2;
    base::ReadFileToString(index_path, &protobuf2);
    proto::CacheStorageIndex upgraded_index;
    EXPECT_TRUE(upgraded_index.ParseFromString(protobuf2));

    // Run the test logic callback.
    test_logic.Run(original_index, upgraded_index, bucket_usage);
  }
};

TEST_F(CacheStorageIndexMigrationTest, PaddingMigration) {
  DoTest("content/test/data/cache_storage/padding_v2/",
         base::BindLambdaForTesting(
             [](const proto::CacheStorageIndex& original_index,
                const proto::CacheStorageIndex& upgraded_index,
                int64_t bucket_usage) {
               // Verify the old index matches our expectations.  It should
               // contain a single cache with the old padding version.
               EXPECT_EQ(original_index.cache_size(), 1);
               EXPECT_EQ(original_index.cache(0).padding_version(), 2);
               int64_t original_padding = original_index.cache(0).padding();

               // Verify the single cache has had its padding version upgraded.
               EXPECT_EQ(upgraded_index.cache_size(), 1);
               EXPECT_EQ(upgraded_index.cache(0).padding_version(), 3);
               int64_t upgraded_size = upgraded_index.cache(0).size();
               int64_t upgraded_padding = upgraded_index.cache(0).padding();

               // Verify the padding has changed with the migration.  Note, the
               // non-padded size may or may not have changed depending on if
               // additional fields are stored in each entry or the index in the
               // new disk schema.
               EXPECT_NE(original_padding, upgraded_padding);
               EXPECT_EQ(bucket_usage, (upgraded_size + upgraded_padding));
             }));
}

TEST_F(CacheStorageIndexMigrationTest, BucketMigration) {
  DoTest("content/test/data/cache_storage/storage_key/",
         base::BindLambdaForTesting(
             [this](const proto::CacheStorageIndex& original_index,
                    const proto::CacheStorageIndex& upgraded_index,
                    int64_t bucket_usage) {
               EXPECT_FALSE(original_index.has_storage_key());
               EXPECT_FALSE(original_index.has_bucket_id());
               EXPECT_FALSE(original_index.has_bucket_is_default());

               EXPECT_TRUE(upgraded_index.has_storage_key());
               EXPECT_TRUE(upgraded_index.has_bucket_id());
               EXPECT_TRUE(upgraded_index.has_bucket_is_default());

               std::optional<blink::StorageKey> result =
                   blink::StorageKey::Deserialize(upgraded_index.storage_key());
               ASSERT_TRUE(result.has_value());
               EXPECT_EQ(this->storage_key1_, result.value());

               storage::BucketLocator bucket_locator = storage::BucketLocator(
                   storage::BucketId(upgraded_index.bucket_id()),
                   result.value(), StorageType::kTemporary,
                   upgraded_index.bucket_is_default());
               EXPECT_EQ(this->bucket_locator1_, bucket_locator);
             }));
}

TEST_F(CacheStorageIndexMigrationTest, InvalidBucketId) {
  DoTest(
      "content/test/data/cache_storage/invalid_bucket_id/",
      base::BindLambdaForTesting(
          [this](const proto::CacheStorageIndex& original_index,
                 const proto::CacheStorageIndex& upgraded_index,
                 int64_t bucket_usage) {
            EXPECT_EQ(original_index.storage_key(),
                      upgraded_index.storage_key());
            EXPECT_EQ(original_index.bucket_is_default(),
                      upgraded_index.bucket_is_default());

            EXPECT_EQ(original_index.bucket_id(), 999);
            EXPECT_GT(original_index.bucket_id(), upgraded_index.bucket_id());

            std::optional<blink::StorageKey> result =
                blink::StorageKey::Deserialize(upgraded_index.storage_key());
            ASSERT_TRUE(result.has_value());
            EXPECT_EQ(this->storage_key1_, result.value());

            storage::BucketLocator bucket_locator = storage::BucketLocator(
                storage::BucketId(upgraded_index.bucket_id()), result.value(),
                StorageType::kTemporary, upgraded_index.bucket_is_default());
            EXPECT_EQ(this->bucket_locator1_, bucket_locator);
          }));
}

INSTANTIATE_TEST_SUITE_P(CacheStorageManagerTests,
                         CacheStorageManagerTestP,
                         ::testing::Values(TestStorage::kMemory,
                                           TestStorage::kDisk));

INSTANTIATE_TEST_SUITE_P(
    CacheStorageManagerStorageKeyAndBucketTests,
    CacheStorageManagerStorageKeyAndBucketTestP,
    ::testing::Values(
        StorageKeyAndBucketTestCase::kFirstPartyDefault,
        StorageKeyAndBucketTestCase::kFirstPartyDefaultPartitionEnabled,
        StorageKeyAndBucketTestCase::kFirstPartyNamed,
        StorageKeyAndBucketTestCase::kFirstPartyNamedPartitionEnabled,
        StorageKeyAndBucketTestCase::kThirdPartyDefault,
        StorageKeyAndBucketTestCase::kThirdPartyNamed));

INSTANTIATE_TEST_SUITE_P(CacheStorageQuotaClientTests,
                         CacheStorageQuotaClientTestP,
                         ::testing::Values(TestStorage::kMemory,
                                           TestStorage::kDisk));

}  // namespace cache_storage_manager_unittest
}  // namespace content
