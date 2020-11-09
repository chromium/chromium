// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/containers/flat_map.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/guid.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/cache_storage/cache_storage.pb.h"
#include "content/browser/cache_storage/cache_storage_cache_handle.h"
#include "content/browser/cache_storage/cache_storage_context_impl.h"
#include "content/browser/cache_storage/cache_storage_quota_client.h"
#include "content/browser/cache_storage/cache_storage_scheduler.h"
#include "content/browser/cache_storage/cross_sequence/cross_sequence_cache_storage_manager.h"
#include "content/browser/cache_storage/legacy/legacy_cache_storage.h"
#include "content/browser/cache_storage/legacy/legacy_cache_storage_manager.h"
#include "content/common/background_fetch/background_fetch_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_usage_info.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/disk_cache/disk_cache.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/quota/padding_key.h"
#include "storage/browser/quota/quota_client_type.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/fake_blob.h"
#include "storage/browser/test/mock_quota_manager_proxy.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/cache_storage/cache_storage.mojom.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

using blink::mojom::CacheStorageError;
using blink::mojom::CacheStorageVerboseErrorPtr;
using network::mojom::FetchResponseType;

namespace content {
namespace cache_storage_manager_unittest {

enum class TestManager {
  kLegacy,
  kCrossSequence,
};

enum class TestStorage {
  kDisk,
  kMemory,
};

struct Param {
  Param(TestManager manager, TestStorage storage)
      : manager_(manager), storage_(storage) {}

  TestManager manager_;
  TestStorage storage_;
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
    uint32_t num_bytes = data_.length();
    producer_handle_->WriteData(data_.data(), &num_bytes,
                                MOJO_WRITE_DATA_FLAG_NONE);
    ASSERT_EQ(data_.length(), num_bytes);

    // Signal that ReadAll() was called.
    std::move(read_closure_).Run();

    MaybeComplete();
  }

 private:
  void MaybeComplete() {
    if (paused_ || !client_)
      return;
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
      : CacheStorageScheduler(CacheStorageSchedulerClient::kCache,
                              base::ThreadTaskRunnerHandle::Get()),
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
    if (callback_)
      std::move(callback_).Run();
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
      scoped_refptr<storage::QuotaClient> client,
      storage::QuotaClientType client_type,
      const std::vector<blink::mojom::StorageType>& storage_types) override {
    registered_clients_.push_back(std::move(client));
  }

  void SimulateQuotaManagerDestroyed() override {
    for (const auto& client : registered_clients_) {
      client->OnQuotaManagerDestroyed();
    }
    registered_clients_.clear();
  }

 private:
  ~MockCacheStorageQuotaManagerProxy() override {
    DCHECK(registered_clients_.empty());
  }

  std::vector<scoped_refptr<storage::QuotaClient>> registered_clients_;
};

bool IsIndexFileCurrent(const base::FilePath& cache_dir) {
  base::File::Info info;
  const base::FilePath index_path =
      cache_dir.AppendASCII(LegacyCacheStorage::kIndexFileName);
  if (!GetFileInfo(index_path, &info))
    return false;
  base::Time index_last_modified = info.last_modified;

  base::FileEnumerator enumerator(cache_dir, false,
                                  base::FileEnumerator::DIRECTORIES);
  for (base::FilePath file_path = enumerator.Next(); !file_path.empty();
       file_path = enumerator.Next()) {
    if (!GetFileInfo(file_path, &info))
      return false;
    if (index_last_modified < info.last_modified)
      return false;
  }

  return true;
}

class TestCacheStorageObserver : public CacheStorageContextImpl::Observer {
 public:
  TestCacheStorageObserver() : loop_(std::make_unique<base::RunLoop>()) {}

  void OnCacheListChanged(const url::Origin& origin) override {
    ++notify_list_changed_count;
    loop_->Quit();
  }

  void OnCacheContentChanged(const url::Origin& origin,
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

  std::unique_ptr<base::RunLoop> loop_;
};

class TestCacheStorageContext : public CacheStorageContextWithManager {
 public:
  explicit TestCacheStorageContext(scoped_refptr<CacheStorageManager> manager)
      : manager_(std::move(manager)) {}

  scoped_refptr<CacheStorageManager> CacheManager() override {
    return manager_;
  }

  void GetAllOriginsInfo(GetUsageInfoCallback callback) override {
    NOTREACHED();
  }

  void DeleteForOrigin(const url::Origin& origin) override { NOTREACHED(); }

 private:
  ~TestCacheStorageContext() override = default;
  scoped_refptr<CacheStorageManager> manager_;
};

class CacheStorageManagerTest : public testing::Test {
 public:
  CacheStorageManagerTest()
      : task_environment_(BrowserTaskEnvironment::IO_MAINLOOP),
        blob_storage_context_(nullptr),
        observers_(
            base::MakeRefCounted<CacheStorageContextImpl::ObserverList>()),
        origin1_(url::Origin::Create(GURL("http://example1.com"))),
        origin2_(url::Origin::Create(GURL("http://example2.com"))) {}

  void SetUp() override {
    base::FilePath temp_dir_path;
    if (!MemoryOnly())
      ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    CreateStorageManager();
  }

  void TearDown() override {
    DestroyStorageManager();
    disk_cache::FlushCacheThreadForTesting();
    content::RunAllTasksUntilIdle();
  }

  virtual bool MemoryOnly() { return false; }
  virtual TestManager ManagerType() { return TestManager::kLegacy; }

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
    if (!MemoryOnly())
      temp_dir_path = temp_dir_.GetPath();

    quota_policy_ = base::MakeRefCounted<storage::MockSpecialStoragePolicy>();
    mock_quota_manager_ = base::MakeRefCounted<storage::MockQuotaManager>(
        MemoryOnly(), temp_dir_path, base::ThreadTaskRunnerHandle::Get().get(),
        quota_policy_.get());
    mock_quota_manager_->SetQuota(origin1_, StorageType::kTemporary,
                                  1024 * 1024 * 100);
    mock_quota_manager_->SetQuota(origin2_, StorageType::kTemporary,
                                  1024 * 1024 * 100);

    quota_manager_proxy_ =
        base::MakeRefCounted<MockCacheStorageQuotaManagerProxy>(
            mock_quota_manager_.get(),
            base::ThreadTaskRunnerHandle::Get().get());

    auto legacy_manager = LegacyCacheStorageManager::Create(
        temp_dir_path, base::ThreadTaskRunnerHandle::Get(),
        base::ThreadTaskRunnerHandle::Get(), quota_manager_proxy_, observers_);
    legacy_manager->SetBlobParametersForCache(blob_storage_context_);

    switch (ManagerType()) {
      case TestManager::kLegacy:
        cache_manager_ = std::move(legacy_manager);
        break;
      case TestManager::kCrossSequence:
        auto context = base::MakeRefCounted<TestCacheStorageContext>(
            std::move(legacy_manager));
        cache_manager_ = base::MakeRefCounted<CrossSequenceCacheStorageManager>(
            base::ThreadTaskRunnerHandle::Get(), std::move(context));
        break;
    }
  }

  void RecreateStorageManager() {
    DCHECK(cache_manager_);
    auto* legacy_manager =
        static_cast<LegacyCacheStorageManager*>(cache_manager_.get());
    cache_manager_ =
        LegacyCacheStorageManager::CreateForTesting(legacy_manager);
  }

  bool FlushCacheStorageIndex(const url::Origin& origin) {
    DCHECK(ManagerType() == TestManager::kLegacy);
    callback_bool_ = false;
    base::RunLoop loop;
    auto* impl = LegacyCacheStorage::From(CacheStorageForOrigin(origin));
    bool write_was_scheduled = impl->InitiateScheduledIndexWriteForTest(
        base::BindOnce(&CacheStorageManagerTest::BoolCallback,
                       base::Unretained(this), &loop));
    loop.Run();
    DCHECK(callback_bool_);
    return write_was_scheduled;
  }

  void DestroyStorageManager() {
    if (quota_manager_proxy_)
      quota_manager_proxy_->SimulateQuotaManagerDestroyed();

    callback_cache_handle_ = CacheStorageCacheHandle();
    callback_bool_ = false;
    callback_cache_handle_response_ = nullptr;
    cache_names_.clear();
    callback_all_origins_usage_.clear();

    base::RunLoop().RunUntilIdle();
    quota_manager_proxy_ = nullptr;

    blob_storage_context_ = nullptr;

    quota_policy_ = nullptr;
    mock_quota_manager_ = nullptr;

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

  bool Open(const url::Origin& origin,
            const std::string& cache_name,
            CacheStorageOwner owner = CacheStorageOwner::kCacheAPI) {
    base::HistogramTester histogram_tester;
    base::RunLoop loop;
    CacheStorageHandle cache_storage =
        cache_manager_->OpenCacheStorage(origin, owner);
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

  bool Has(const url::Origin& origin,
           const std::string& cache_name,
           CacheStorageOwner owner = CacheStorageOwner::kCacheAPI) {
    base::HistogramTester histogram_tester;
    base::RunLoop loop;
    CacheStorageHandle cache_storage =
        cache_manager_->OpenCacheStorage(origin, owner);
    cache_storage.value()->HasCache(
        cache_name, /* trace_id = */ 0,
        base::BindOnce(&CacheStorageManagerTest::BoolAndErrorCallback,
                       base::Unretained(this), base::Unretained(&loop)));
    loop.Run();
    CheckOpHistograms(histogram_tester, "Has");
    return callback_bool_;
  }

  bool Delete(const url::Origin& origin,
              const std::string& cache_name,
              CacheStorageOwner owner = CacheStorageOwner::kCacheAPI) {
    base::HistogramTester histogram_tester;
    base::RunLoop loop;
    CacheStorageHandle cache_storage =
        cache_manager_->OpenCacheStorage(origin, owner);
    cache_storage.value()->DoomCache(
        cache_name, /* trace_id = */ 0,
        base::BindOnce(&CacheStorageManagerTest::ErrorCallback,
                       base::Unretained(this), base::Unretained(&loop)));
    loop.Run();
    CheckOpHistograms(histogram_tester, "Delete");
    return callback_bool_;
  }

  size_t Keys(const url::Origin& origin,
              CacheStorageOwner owner = CacheStorageOwner::kCacheAPI) {
    base::HistogramTester histogram_tester;
    base::RunLoop loop;
    CacheStorageHandle cache_storage =
        cache_manager_->OpenCacheStorage(origin, owner);
    cache_storage.value()->EnumerateCaches(
        /* trace_id = */ 0,
        base::BindOnce(&CacheStorageManagerTest::CacheMetadataCallback,
                       base::Unretained(this), base::Unretained(&loop)));
    loop.Run();
    CheckOpHistograms(histogram_tester, "Keys");
    return cache_names_.size();
  }

  bool StorageMatch(const url::Origin& origin,
                    const std::string& cache_name,
                    const GURL& url,
                    blink::mojom::CacheQueryOptionsPtr match_options = nullptr,
                    CacheStorageOwner owner = CacheStorageOwner::kCacheAPI) {
    auto request = blink::mojom::FetchAPIRequest::New();
    request->url = url;
    return StorageMatchWithRequest(origin, cache_name, std::move(request),
                                   std::move(match_options), owner);
  }

  bool StorageMatchWithRequest(
      const url::Origin& origin,
      const std::string& cache_name,
      blink::mojom::FetchAPIRequestPtr request,
      blink::mojom::CacheQueryOptionsPtr match_options = nullptr,
      CacheStorageOwner owner = CacheStorageOwner::kCacheAPI) {
    base::HistogramTester histogram_tester;
    base::RunLoop loop;
    CacheStorageHandle cache_storage =
        cache_manager_->OpenCacheStorage(origin, owner);
    cache_storage.value()->MatchCache(
        cache_name, std::move(request), std::move(match_options),
        CacheStorageSchedulerPriority::kNormal, /* trace_id = */ 0,
        base::BindOnce(&CacheStorageManagerTest::CacheMatchCallback,
                       base::Unretained(this), base::Unretained(&loop)));
    loop.Run();
    if (callback_error_ == CacheStorageError::kSuccess)
      CheckOpHistograms(histogram_tester, "Match");
    return callback_error_ == CacheStorageError::kSuccess;
  }

  bool StorageMatchAll(
      const url::Origin& origin,
      const GURL& url,
      blink::mojom::CacheQueryOptionsPtr match_options = nullptr) {
    auto request = blink::mojom::FetchAPIRequest::New();
    request->url = url;
    return StorageMatchAllWithRequest(origin, std::move(request),
                                      std::move(match_options));
  }

  bool StorageMatchAllWithRequest(
      const url::Origin& origin,
      blink::mojom::FetchAPIRequestPtr request,
      blink::mojom::CacheQueryOptionsPtr match_options = nullptr,
      CacheStorageOwner owner = CacheStorageOwner::kCacheAPI) {
    base::HistogramTester histogram_tester;
    base::RunLoop loop;
    CacheStorageHandle cache_storage =
        cache_manager_->OpenCacheStorage(origin, owner);
    cache_storage.value()->MatchAllCaches(
        std::move(request), std::move(match_options),
        CacheStorageSchedulerPriority::kNormal, /* trace_id = */ 0,
        base::BindOnce(&CacheStorageManagerTest::CacheMatchCallback,
                       base::Unretained(this), base::Unretained(&loop)));
    loop.Run();
    if (callback_error_ == CacheStorageError::kSuccess)
      CheckOpHistograms(histogram_tester, "MatchAll");
    return callback_error_ == CacheStorageError::kSuccess;
  }

  bool Write(const url::Origin& origin,
             CacheStorageOwner owner,
             const std::string& cache_name,
             const std::string& request_url) {
    auto request = blink::mojom::FetchAPIRequest::New();
    request->url = GURL(request_url);

    base::RunLoop loop;
    CacheStorageHandle cache_storage =
        cache_manager_->OpenCacheStorage(origin, owner);
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
    std::string blob_uuid = base::GenerateGUID();

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
    auto response = blink::mojom::FetchAPIResponse::New(
        std::vector<GURL>({request->url}), status_code, "OK", response_type,
        network::mojom::FetchResponseSource::kUnspecified, response_headers,
        base::nullopt /* mime_type */, net::HttpRequestHeaders::kGetMethod,
        std::move(blob), blink::mojom::ServiceWorkerResponseError::kUnknown,
        base::Time(), std::string() /* cache_storage_cache_name */,
        std::vector<std::string>() /* cors_exposed_header_names */,
        nullptr /* side_data_blob */,
        nullptr /* side_data_blob_for_cache_put */,
        network::mojom::ParsedHeaders::New(),
        net::HttpResponseInfo::CONNECTION_INFO_UNKNOWN,
        "unknown" /* alpn_negotiated_protocol */,
        false /* loaded_with_credentials */, false /* was_fetched_via_spdy */,
        false /* has_range_requested */);

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

  CacheStorageHandle CacheStorageForOrigin(const url::Origin& origin) {
    return cache_manager_->OpenCacheStorage(origin,
                                            CacheStorageOwner::kCacheAPI);
  }

  int64_t GetOriginUsage(
      const url::Origin& origin,
      CacheStorageOwner owner = CacheStorageOwner::kCacheAPI) {
    base::RunLoop loop;
    cache_manager_->GetOriginUsage(
        origin, owner,
        base::BindOnce(&CacheStorageManagerTest::UsageCallback,
                       base::Unretained(this), base::Unretained(&loop)));
    loop.Run();
    return callback_usage_;
  }

  void UsageCallback(base::RunLoop* run_loop, int64_t usage) {
    callback_usage_ = usage;
    run_loop->Quit();
  }

  std::vector<StorageUsageInfo> GetAllOriginsUsage(
      CacheStorageOwner owner = CacheStorageOwner::kCacheAPI) {
    base::RunLoop loop;
    cache_manager_->GetAllOriginsUsage(
        owner, base::BindLambdaForTesting(
                   [&](const std::vector<StorageUsageInfo>& usage) {
                     callback_all_origins_usage_ = usage;
                     loop.Quit();
                   }));
    loop.Run();
    return callback_all_origins_usage_;
  }

  int64_t GetSizeThenCloseAllCaches(const url::Origin& origin) {
    DCHECK(ManagerType() == TestManager::kLegacy);
    base::RunLoop loop;
    CacheStorageHandle cache_storage = CacheStorageForOrigin(origin);
    LegacyCacheStorage::From(cache_storage)
        ->GetSizeThenCloseAllCaches(
            base::BindOnce(&CacheStorageManagerTest::UsageCallback,
                           base::Unretained(this), &loop));
    loop.Run();
    return callback_usage_;
  }

  int64_t Size(const url::Origin& origin) {
    DCHECK(ManagerType() == TestManager::kLegacy);
    base::RunLoop loop;
    CacheStorageHandle cache_storage = CacheStorageForOrigin(origin);
    LegacyCacheStorage::From(cache_storage)
        ->Size(base::BindOnce(&CacheStorageManagerTest::UsageCallback,
                              base::Unretained(this), &loop));
    loop.Run();
    return callback_usage_;
  }

  int64_t GetQuotaOriginUsage(const url::Origin& origin) {
    int64_t usage(CacheStorage::kSizeUnknown);
    base::RunLoop loop;
    quota_manager_proxy_->GetUsageAndQuota(
        base::ThreadTaskRunnerHandle::Get().get(), origin,
        StorageType::kTemporary,
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
    if (status_code == blink::mojom::QuotaStatusCode::kOk)
      *out_usage = usage;
    run_loop->Quit();
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
  scoped_refptr<CacheStorageContextImpl::ObserverList> observers_;
  scoped_refptr<CacheStorageManager> cache_manager_;

  CacheStorageCacheHandle callback_cache_handle_;
  int callback_bool_ = false;
  CacheStorageError callback_error_ = CacheStorageError::kSuccess;
  blink::mojom::FetchAPIResponsePtr callback_cache_handle_response_;
  std::vector<std::string> cache_names_;

  const url::Origin origin1_;
  const url::Origin origin2_;

  int64_t callback_usage_;
  std::vector<StorageUsageInfo> callback_all_origins_usage_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CacheStorageManagerTest);
};

class CacheStorageManagerMemoryOnlyTest : public CacheStorageManagerTest {
 public:
  bool MemoryOnly() override { return true; }
};

class CacheStorageManagerTestP : public CacheStorageManagerTest,
                                 public testing::WithParamInterface<Param> {
 public:
  bool MemoryOnly() override {
    return GetParam().storage_ == TestStorage::kMemory;
  }
  TestManager ManagerType() override { return GetParam().manager_; }
};

// Some tests must be run on the LegacyCacheStorageManager.  This could
// be for a number of reasons:
//  * The test needs to use internal APIs on the legacy manager.
//  * The test is checking behavior that is only true for "real" manager's
//    like that Open() will return the exact same c++ pointer for the
//    underlying cache.  This assumption is not truee for the cross-sequence
//    wrapper.
class CacheStorageManagerLegacyOnlyTestP
    : public CacheStorageManagerTest,
      public testing::WithParamInterface<TestStorage> {
  bool MemoryOnly() override { return GetParam() == TestStorage::kMemory; }
};

TEST_F(CacheStorageManagerTest, TestsRunOnIOThread) {
  EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
}

TEST_P(CacheStorageManagerTestP, OpenCache) {
  EXPECT_TRUE(Open(origin1_, "foo"));
}

TEST_P(CacheStorageManagerTestP, OpenTwoCaches) {
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(Open(origin1_, "bar"));
}

TEST_P(CacheStorageManagerTestP, OpenSameCacheDifferentOwners) {
  EXPECT_TRUE(Open(origin1_, "foo", CacheStorageOwner::kCacheAPI));
  CacheStorageCacheHandle cache_handle = std::move(callback_cache_handle_);
  EXPECT_TRUE(Open(origin1_, "foo", CacheStorageOwner::kBackgroundFetch));
  EXPECT_NE(callback_cache_handle_.value(), cache_handle.value());
}

TEST_P(CacheStorageManagerTestP, CachePointersDiffer) {
  EXPECT_TRUE(Open(origin1_, "foo"));
  CacheStorageCacheHandle cache_handle = std::move(callback_cache_handle_);
  EXPECT_TRUE(Open(origin1_, "bar"));
  EXPECT_NE(callback_cache_handle_.value(), cache_handle.value());
}

TEST_P(CacheStorageManagerTestP, Open2CachesSameNameDiffOrigins) {
  EXPECT_TRUE(Open(origin1_, "foo"));
  CacheStorageCacheHandle cache_handle = std::move(callback_cache_handle_);
  EXPECT_TRUE(Open(origin2_, "foo"));
  EXPECT_NE(cache_handle.value(), callback_cache_handle_.value());
}

TEST_P(CacheStorageManagerLegacyOnlyTestP, OpenExistingCache) {
  EXPECT_TRUE(Open(origin1_, "foo"));
  CacheStorageCacheHandle cache_handle = std::move(callback_cache_handle_);
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_EQ(callback_cache_handle_.value(), cache_handle.value());
}

TEST_P(CacheStorageManagerTestP, HasCache) {
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(Has(origin1_, "foo"));
  EXPECT_TRUE(callback_bool_);
}

TEST_P(CacheStorageManagerTestP, HasCacheDifferentOwners) {
  EXPECT_TRUE(Open(origin1_, "public", CacheStorageOwner::kCacheAPI));
  EXPECT_TRUE(Open(origin1_, "bgf", CacheStorageOwner::kBackgroundFetch));

  EXPECT_TRUE(Has(origin1_, "public", CacheStorageOwner::kCacheAPI));
  EXPECT_TRUE(callback_bool_);
  EXPECT_FALSE(Has(origin1_, "bgf", CacheStorageOwner::kCacheAPI));
  EXPECT_FALSE(callback_bool_);

  EXPECT_TRUE(Has(origin1_, "bgf", CacheStorageOwner::kBackgroundFetch));
  EXPECT_TRUE(callback_bool_);
  EXPECT_FALSE(Has(origin1_, "public", CacheStorageOwner::kBackgroundFetch));
  EXPECT_FALSE(callback_bool_);
}

TEST_P(CacheStorageManagerTestP, HasNonExistent) {
  EXPECT_FALSE(Has(origin1_, "foo"));
}

TEST_P(CacheStorageManagerTestP, DeleteCache) {
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(Delete(origin1_, "foo"));
  EXPECT_FALSE(Has(origin1_, "foo"));
}

TEST_P(CacheStorageManagerTestP, DeleteTwice) {
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(Delete(origin1_, "foo"));
  EXPECT_FALSE(Delete(origin1_, "foo"));
  EXPECT_EQ(CacheStorageError::kErrorNotFound, callback_error_);
}

TEST_P(CacheStorageManagerTestP, DeleteCacheReducesOriginSize) {
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(
      CachePut(callback_cache_handle_.value(), GURL("http://example.com/foo")));
  // The quota manager gets updated after the put operation runs its callback so
  // run the event loop.
  base::RunLoop().RunUntilIdle();
  int64_t put_delta = quota_manager_proxy_->last_notified_delta();
  EXPECT_LT(0, put_delta);
  EXPECT_TRUE(Delete(origin1_, "foo"));

  // Drop the cache handle so that the cache can be erased from disk.
  callback_cache_handle_ = CacheStorageCacheHandle();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(-1 * quota_manager_proxy_->last_notified_delta(), put_delta);
}

TEST_P(CacheStorageManagerTestP, EmptyKeys) {
  EXPECT_EQ(0u, Keys(origin1_));
}

TEST_P(CacheStorageManagerTestP, SomeKeys) {
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(Open(origin1_, "bar"));
  EXPECT_TRUE(Open(origin2_, "baz"));
  EXPECT_EQ(2u, Keys(origin1_));
  std::vector<std::string> expected_keys;
  expected_keys.push_back("foo");
  expected_keys.push_back("bar");
  EXPECT_EQ(expected_keys, GetIndexNames());
  EXPECT_EQ(1u, Keys(origin2_));
  EXPECT_STREQ("baz", GetFirstIndexName().c_str());
}

TEST_P(CacheStorageManagerTestP, DeletedKeysGone) {
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(Open(origin1_, "bar"));
  EXPECT_TRUE(Open(origin2_, "baz"));
  EXPECT_TRUE(Delete(origin1_, "bar"));
  EXPECT_EQ(1u, Keys(origin1_));
  EXPECT_STREQ("foo", GetFirstIndexName().c_str());
}

TEST_P(CacheStorageManagerTestP, StorageMatchEntryExists) {
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(
      CachePut(callback_cache_handle_.value(), GURL("http://example.com/foo")));
  EXPECT_TRUE(StorageMatch(origin1_, "foo", GURL("http://example.com/foo")));
}

TEST_P(CacheStorageManagerTestP, StorageMatchNoEntry) {
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(
      CachePut(callback_cache_handle_.value(), GURL("http://example.com/foo")));
  EXPECT_FALSE(StorageMatch(origin1_, "foo", GURL("http://example.com/bar")));
  EXPECT_EQ(CacheStorageError::kErrorNotFound, callback_error_);
}

TEST_P(CacheStorageManagerTestP, StorageMatchNoCache) {
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(
      CachePut(callback_cache_handle_.value(), GURL("http://example.com/foo")));
  EXPECT_FALSE(StorageMatch(origin1_, "bar", GURL("http://example.com/foo")));
  EXPECT_EQ(CacheStorageError::kErrorCacheNameNotFound, callback_error_);
}

TEST_P(CacheStorageManagerTestP, StorageMatchAllEntryExists) {
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(
      CachePut(callback_cache_handle_.value(), GURL("http://example.com/foo")));
  EXPECT_TRUE(StorageMatchAll(origin1_, GURL("http://example.com/foo")));
}

TEST_P(CacheStorageManagerTestP, StorageMatchAllNoEntry) {
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(
      CachePut(callback_cache_handle_.value(), GURL("http://example.com/foo")));
  EXPECT_FALSE(StorageMatchAll(origin1_, GURL("http://example.com/bar")));
  EXPECT_EQ(CacheStorageError::kErrorNotFound, callback_error_);
}

TEST_P(CacheStorageManagerTestP, StorageMatchAllNoCaches) {
  EXPECT_FALSE(StorageMatchAll(origin1_, GURL("http://example.com/foo")));
  EXPECT_EQ(CacheStorageError::kErrorNotFound, callback_error_);
}

TEST_P(CacheStorageManagerTestP, StorageMatchDifferentOwners) {
  EXPECT_TRUE(Open(origin1_, "foo", CacheStorageOwner::kCacheAPI));
  EXPECT_TRUE(CachePut(callback_cache_handle_.value(),
                       GURL("http://example.com/public")));
  EXPECT_TRUE(Open(origin1_, "foo", CacheStorageOwner::kBackgroundFetch));
  EXPECT_TRUE(
      CachePut(callback_cache_handle_.value(), GURL("http://example.com/bgf")));

  // Check the public cache.
  EXPECT_TRUE(StorageMatch(origin1_, "foo", GURL("http://example.com/public"),
                           nullptr, CacheStorageOwner::kCacheAPI));
  EXPECT_FALSE(StorageMatch(origin1_, "foo", GURL("http://example.com/bgf"),
                            nullptr, CacheStorageOwner::kCacheAPI));

  // Check the internal cache.
  EXPECT_FALSE(StorageMatch(origin1_, "foo", GURL("http://example.com/public"),
                            nullptr, CacheStorageOwner::kBackgroundFetch));
  EXPECT_TRUE(StorageMatch(origin1_, "foo", GURL("http://example.com/bgf"),
                           nullptr, CacheStorageOwner::kBackgroundFetch));
}

TEST_F(CacheStorageManagerTest, StorageReuseCacheName) {
  // Deleting a cache and creating one with the same name and adding an entry
  // with the same URL should work. (see crbug.com/542668)
  const GURL kTestURL = GURL("http://example.com/foo");
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(CachePut(callback_cache_handle_.value(), kTestURL));
  EXPECT_TRUE(CacheMatch(callback_cache_handle_.value(), kTestURL));

  EXPECT_TRUE(Delete(origin1_, "foo"));
  // The cache is deleted but the handle to one of its entries is still
  // open. Creating a new cache in the same directory would fail on Windows.
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(CachePut(callback_cache_handle_.value(), kTestURL));
}

TEST_P(CacheStorageManagerTestP, DropRefAfterNewCacheWithSameNameCreated) {
  // Make sure that dropping the final cache handle to a doomed cache doesn't
  // affect newer caches with the same name. (see crbug.com/631467)

  // 1. Create cache A and hang onto the handle
  const GURL kTestURL = GURL("http://example.com/foo");
  EXPECT_TRUE(Open(origin1_, "foo"));
  CacheStorageCacheHandle cache_handle = std::move(callback_cache_handle_);

  // 2. Doom the cache
  EXPECT_TRUE(Delete(origin1_, "foo"));

  // 3. Create cache B (with the same name)
  EXPECT_TRUE(Open(origin1_, "foo"));

  // 4. Drop handle to A
  cache_handle = CacheStorageCacheHandle();

  // 5. Verify that B still works
  EXPECT_FALSE(CacheMatch(callback_cache_handle_.value(), kTestURL));
}

TEST_P(CacheStorageManagerTestP, DeleteCorrectDirectory) {
  // This test reproduces crbug.com/630036.
  // 1. Cache A with name "foo" is created
  const GURL kTestURL = GURL("http://example.com/foo");
  EXPECT_TRUE(Open(origin1_, "foo"));
  CacheStorageCacheHandle cache_handle = std::move(callback_cache_handle_);

  // 2. Cache A is doomed, but js hangs onto the handle.
  EXPECT_TRUE(Delete(origin1_, "foo"));

  // 3. Cache B with name "foo" is created
  EXPECT_TRUE(Open(origin1_, "foo"));

  // 4. Cache B is doomed, and both handles are reset.
  EXPECT_TRUE(Delete(origin1_, "foo"));
  cache_handle = CacheStorageCacheHandle();

  // Do some busy work on a different cache to move the cache pool threads
  // along and trigger the bug.
  EXPECT_TRUE(Open(origin1_, "bar"));
  EXPECT_TRUE(CachePut(callback_cache_handle_.value(), kTestURL));
  EXPECT_TRUE(CacheMatch(callback_cache_handle_.value(), kTestURL));
}

TEST_P(CacheStorageManagerTestP, StorageMatchAllEntryExistsTwice) {
  EXPECT_TRUE(Open(origin1_, "foo"));
  // |request_1| and |request_2| has the same url.
  auto request_1 = blink::mojom::FetchAPIRequest::New();
  request_1->url = GURL("http://example.com/foo");
  auto request_2 = BackgroundFetchSettledFetch::CloneRequest(request_1);
  EXPECT_TRUE(CachePutWithStatusCode(callback_cache_handle_.value(),
                                     std::move(request_1), 200));
  EXPECT_TRUE(Open(origin1_, "bar"));
  EXPECT_TRUE(CachePutWithStatusCode(callback_cache_handle_.value(),
                                     std::move(request_2), 201));

  EXPECT_TRUE(StorageMatchAll(origin1_, GURL("http://example.com/foo")));

  // The caches need to be searched in order of creation, so verify that the
  // response came from the first cache.
  EXPECT_EQ(200, callback_cache_handle_response_->status_code);
}

TEST_P(CacheStorageManagerTestP, StorageMatchInOneOfMany) {
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(Open(origin1_, "bar"));
  EXPECT_TRUE(
      CachePut(callback_cache_handle_.value(), GURL("http://example.com/foo")));
  EXPECT_TRUE(Open(origin1_, "baz"));

  EXPECT_TRUE(StorageMatchAll(origin1_, GURL("http://example.com/foo")));
}

TEST_P(CacheStorageManagerLegacyOnlyTestP, Chinese) {
  EXPECT_TRUE(Open(origin1_, "你好"));
  CacheStorageCacheHandle cache_handle = std::move(callback_cache_handle_);
  EXPECT_TRUE(Open(origin1_, "你好"));
  EXPECT_EQ(callback_cache_handle_.value(), cache_handle.value());
  EXPECT_EQ(1u, Keys(origin1_));
  EXPECT_STREQ("你好", GetFirstIndexName().c_str());
}

TEST_F(CacheStorageManagerTest, EmptyKey) {
  EXPECT_TRUE(Open(origin1_, ""));
  CacheStorageCacheHandle cache_handle = std::move(callback_cache_handle_);
  EXPECT_TRUE(Open(origin1_, ""));
  EXPECT_EQ(cache_handle.value(), callback_cache_handle_.value());
  EXPECT_EQ(1u, Keys(origin1_));
  EXPECT_STREQ("", GetFirstIndexName().c_str());
  EXPECT_TRUE(Has(origin1_, ""));
  EXPECT_TRUE(Delete(origin1_, ""));
  EXPECT_EQ(0u, Keys(origin1_));
}

TEST_F(CacheStorageManagerTest, DataPersists) {
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(Open(origin1_, "bar"));
  EXPECT_TRUE(Open(origin1_, "baz"));
  EXPECT_TRUE(Open(origin2_, "raz"));
  EXPECT_TRUE(Delete(origin1_, "bar"));
  quota_manager_proxy_->SimulateQuotaManagerDestroyed();
  RecreateStorageManager();
  EXPECT_EQ(2u, Keys(origin1_));
  std::vector<std::string> expected_keys;
  expected_keys.push_back("foo");
  expected_keys.push_back("baz");
  EXPECT_EQ(expected_keys, GetIndexNames());
}

TEST_F(CacheStorageManagerMemoryOnlyTest, DataLostWhenMemoryOnly) {
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(Open(origin2_, "baz"));
  quota_manager_proxy_->SimulateQuotaManagerDestroyed();
  RecreateStorageManager();
  EXPECT_EQ(0u, Keys(origin1_));
}

TEST_F(CacheStorageManagerTest, BadCacheName) {
  // Since the implementation writes cache names to disk, ensure that we don't
  // escape the directory.
  const std::string bad_name = "../../../../../../../../../../../../../../foo";
  EXPECT_TRUE(Open(origin1_, bad_name));
  EXPECT_EQ(1u, Keys(origin1_));
  EXPECT_STREQ(bad_name.c_str(), GetFirstIndexName().c_str());
}

TEST_F(CacheStorageManagerTest, BadOriginName) {
  // Since the implementation writes origin names to disk, ensure that we don't
  // escape the directory.
  url::Origin bad_origin(url::Origin::Create(
      GURL("http://../../../../../../../../../../../../../../foo")));
  EXPECT_TRUE(Open(bad_origin, "foo"));
  EXPECT_EQ(1u, Keys(bad_origin));
  EXPECT_STREQ("foo", GetFirstIndexName().c_str());
}

// Dropping a reference to a cache should not immediately destroy it.  These
// warm cache objects are kept alive to optimize the next open.
TEST_F(CacheStorageManagerTest, DropReference) {
  CacheStorageHandle cache_storage = CacheStorageForOrigin(origin1_);

  EXPECT_TRUE(Open(origin1_, "foo"));
  base::WeakPtr<LegacyCacheStorageCache> cache =
      LegacyCacheStorageCache::From(callback_cache_handle_)->AsWeakPtr();
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
  CacheStorageHandle cache_storage = CacheStorageForOrigin(origin1_);

  EXPECT_TRUE(Open(origin1_, "foo"));
  base::WeakPtr<LegacyCacheStorageCache> cache =
      LegacyCacheStorageCache::From(callback_cache_handle_)->AsWeakPtr();
  // Run a cache operation to ensure that the cache has finished initializing so
  // that when the handle is dropped it could possibly close immediately.
  EXPECT_FALSE(CacheMatch(callback_cache_handle_.value(),
                          GURL("http://example.com/foo")));

  callback_cache_handle_ = CacheStorageCacheHandle();
  EXPECT_TRUE(cache) << "unreferenced cache destroyed while owning "
                        "CacheStorage is still referenced";

  // Delete() should trigger its destruction, however.
  EXPECT_TRUE(Delete(origin1_, "foo"));
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(cache)
      << "deleted cache not destroyed after last reference removed";
}

// Critical memory pressure should remove any warmed caches that been kept
// alive without a reference.
TEST_F(CacheStorageManagerTest, DropReferenceAndMemoryPressure) {
  // Hold a reference to the CacheStorage to permit the warmed
  // CacheStorageCache to be kept alive.
  CacheStorageHandle cache_storage = CacheStorageForOrigin(origin1_);

  EXPECT_TRUE(Open(origin1_, "foo"));
  base::WeakPtr<LegacyCacheStorageCache> cache =
      LegacyCacheStorageCache::From(callback_cache_handle_)->AsWeakPtr();
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
  EXPECT_TRUE(Open(origin1_, "foo"));
  base::WeakPtr<LegacyCacheStorageCache> cache =
      LegacyCacheStorageCache::From(callback_cache_handle_)->AsWeakPtr();
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
  EXPECT_TRUE(Open(origin1_, "foo"));
  CacheStorageCacheHandle original_handle = std::move(callback_cache_handle_);
  EXPECT_TRUE(CachePut(original_handle.value(), kFooURL));
  EXPECT_TRUE(Delete(origin1_, "foo"));

  // Verify that the existing cache handle still works.
  EXPECT_TRUE(CacheMatch(original_handle.value(), kFooURL));
  EXPECT_TRUE(CachePut(original_handle.value(), kBarURL));
  EXPECT_TRUE(CacheMatch(original_handle.value(), kBarURL));

  // The cache shouldn't be visible to subsequent storage operations.
  EXPECT_EQ(0u, Keys(origin1_));

  // Open a new cache with the same name, it should create a new cache, but not
  // interfere with the original cache.
  EXPECT_TRUE(Open(origin1_, "foo"));
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

  EXPECT_TRUE(Open(origin1_, kCacheName));
  auto original_handle = std::move(callback_cache_handle_);
  EXPECT_TRUE(CachePut(original_handle.value(), kFooURL));
  EXPECT_TRUE(Delete(origin1_, kCacheName));

  // Now a second cache using the same name, but with different data.
  EXPECT_TRUE(Open(origin1_, kCacheName));
  auto new_handle = std::move(callback_cache_handle_);
  EXPECT_TRUE(CachePut(new_handle.value(), kFooURL));
  EXPECT_TRUE(CachePut(new_handle.value(), kBarURL));
  EXPECT_TRUE(CachePut(new_handle.value(), kBazURL));
  auto new_cache_size = Size(origin1_);

  // Now modify the first cache.
  EXPECT_TRUE(CachePut(original_handle.value(), kBarURL));

  // Now deref both caches, and recreate the storage manager.
  original_handle = CacheStorageCacheHandle();
  new_handle = CacheStorageCacheHandle();
  EXPECT_TRUE(FlushCacheStorageIndex(origin1_));
  DestroyStorageManager();
  CreateStorageManager();

  EXPECT_TRUE(Open(origin1_, kCacheName));
  EXPECT_EQ(new_cache_size, Size(origin1_));
}

TEST_F(CacheStorageManagerTest, TestErrorInitializingCache) {
  if (MemoryOnly())
    return;
  const GURL kFooURL("http://example.com/foo");
  const std::string kCacheName = "foo";

  EXPECT_TRUE(Open(origin1_, kCacheName));
  auto original_handle = std::move(callback_cache_handle_);
  EXPECT_TRUE(CachePut(original_handle.value(), kFooURL));
  auto size_before_close = Size(origin1_);
  EXPECT_GT(size_before_close, 0);

  CacheStorageHandle cache_storage = CacheStorageForOrigin(origin1_);
  auto cache_handle =
      LegacyCacheStorage::From(cache_storage)->GetLoadedCache(kCacheName);
  base::FilePath cache_path =
      LegacyCacheStorageCache::From(cache_handle)->path();
  base::FilePath storage_path = cache_path.DirName();
  base::FilePath index_path = cache_path.AppendASCII("index");
  cache_handle = CacheStorageCacheHandle();

  // Do our best to flush any pending cache_storage index file writes to disk
  // before proceeding.  This does not guarantee the simple disk_cache index
  // is written, though.
  EXPECT_TRUE(FlushCacheStorageIndex(origin1_));
  EXPECT_FALSE(FlushCacheStorageIndex(origin1_));

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
      storage_path.AppendASCII(LegacyCacheStorage::kIndexFileName);
  base::Time t = base::Time::Now() + base::TimeDelta::FromHours(-1);
  EXPECT_TRUE(base::TouchFile(cache_index_path, t, t));
  EXPECT_FALSE(IsIndexFileCurrent(storage_path));

  CreateStorageManager();

  EXPECT_TRUE(Open(origin1_, kCacheName));
  EXPECT_EQ(0, Size(origin1_));
}

TEST_F(CacheStorageManagerTest, CacheSizeCorrectAfterReopen) {
  const GURL kFooURL("http://example.com/foo");
  const std::string kCacheName = "foo";

  EXPECT_TRUE(Open(origin1_, kCacheName));
  auto original_handle = std::move(callback_cache_handle_);
  EXPECT_TRUE(CachePut(original_handle.value(), kFooURL));
  auto size_before_close = Size(origin1_);
  EXPECT_GT(size_before_close, 0);

  DestroyStorageManager();
  CreateStorageManager();

  EXPECT_TRUE(Open(origin1_, kCacheName));
  EXPECT_EQ(size_before_close, Size(origin1_));
}

TEST_F(CacheStorageManagerTest, CacheSizePaddedAfterReopen) {
  const GURL kFooURL = origin1_.GetURL().Resolve("foo");
  const std::string kCacheName = "foo";

  int64_t put_delta = quota_manager_proxy_->last_notified_delta();
  EXPECT_EQ(0, put_delta);
  EXPECT_EQ(0, quota_manager_proxy_->notify_storage_modified_count());

  EXPECT_TRUE(Open(origin1_, kCacheName));
  CacheStorageCacheHandle original_handle = std::move(callback_cache_handle_);

  base::RunLoop().RunUntilIdle();
  put_delta += quota_manager_proxy_->last_notified_delta();
  EXPECT_EQ(0, put_delta);
  EXPECT_EQ(0, quota_manager_proxy_->notify_storage_modified_count());

  EXPECT_TRUE(
      CachePut(original_handle.value(), kFooURL, FetchResponseType::kOpaque));
  int64_t cache_size_before_close = Size(origin1_);
  base::FilePath storage_dir =
      LegacyCacheStorageCache::From(original_handle)->path().DirName();
  original_handle = CacheStorageCacheHandle();
  EXPECT_GT(cache_size_before_close, 0);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(cache_size_before_close, GetQuotaOriginUsage(origin1_));

  base::RunLoop().RunUntilIdle();
  put_delta = quota_manager_proxy_->last_notified_delta();
  EXPECT_GT(put_delta, 0);
  EXPECT_EQ(1, quota_manager_proxy_->notify_storage_modified_count());

  EXPECT_EQ(GetQuotaOriginUsage(origin1_), put_delta);

  // Close the caches and cache manager.
  EXPECT_TRUE(FlushCacheStorageIndex(origin1_));
  DestroyStorageManager();

  // Create a new CacheStorageManager that hasn't yet loaded the origin.
  CreateStorageManager();
  quota_manager_proxy_->SimulateQuotaManagerDestroyed();
  RecreateStorageManager();
  EXPECT_TRUE(Open(origin1_, kCacheName));

  base::RunLoop().RunUntilIdle();
  put_delta = quota_manager_proxy_->last_notified_delta();
  EXPECT_EQ(0, put_delta);
  EXPECT_EQ(0, quota_manager_proxy_->notify_storage_modified_count());

  EXPECT_EQ(cache_size_before_close, Size(origin1_));
}

TEST_F(CacheStorageManagerTest, QuotaCorrectAfterReopen) {
  const std::string kCacheName = "foo";

  // Choose a response type that will not be padded so that the expected
  // cache size can be calculated.
  const FetchResponseType response_type = FetchResponseType::kCors;

  // Create a new cache.
  int64_t cache_size;
  {
    EXPECT_TRUE(Open(origin1_, kCacheName));
    CacheStorageCacheHandle cache_handle = std::move(callback_cache_handle_);
    base::RunLoop().RunUntilIdle();

    const GURL kFooURL = origin1_.GetURL().Resolve("foo");
    EXPECT_TRUE(CachePut(cache_handle.value(), kFooURL, response_type));
    cache_size = Size(origin1_);

    EXPECT_EQ(cache_size, GetQuotaOriginUsage(origin1_));
  }

  // Wait for the dereferenced cache to be closed.
  base::RunLoop().RunUntilIdle();

  // Now reopen the cache.
  EXPECT_TRUE(Open(origin1_, kCacheName));
  CacheStorageCacheHandle cache_handle = std::move(callback_cache_handle_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(cache_size, GetQuotaOriginUsage(origin1_));

  // And write a second equally sized value and verify size is doubled.
  const GURL kBarURL = origin1_.GetURL().Resolve("bar");
  EXPECT_TRUE(CachePut(cache_handle.value(), kBarURL, response_type));

  EXPECT_EQ(2 * cache_size, GetQuotaOriginUsage(origin1_));
}

TEST_F(CacheStorageManagerTest, PersistedCacheKeyUsed) {
  const GURL kFooURL = origin1_.GetURL().Resolve("foo");
  const std::string kCacheName = "foo";

  EXPECT_TRUE(Open(origin1_, kCacheName));
  CacheStorageCacheHandle original_handle = std::move(callback_cache_handle_);

  EXPECT_TRUE(
      CachePut(original_handle.value(), kFooURL, FetchResponseType::kOpaque));

  int64_t cache_size_after_put = Size(origin1_);
  EXPECT_LT(0, cache_size_after_put);

  // Close the caches and cache manager.
  EXPECT_TRUE(FlushCacheStorageIndex(origin1_));
  DestroyStorageManager();

  // ResetPaddingKeyForTesting isn't thread safe so
  base::RunLoop().RunUntilIdle();
  storage::ResetPaddingKeyForTesting();

  // Create a new CacheStorageManager that hasn't yet loaded the origin.
  CreateStorageManager();
  quota_manager_proxy_->SimulateQuotaManagerDestroyed();
  RecreateStorageManager();

  // Reopening the origin/cache creates a new CacheStorage instance with a new
  // random key.
  EXPECT_TRUE(Open(origin1_, kCacheName));

  // Size (before any change) should be the same as before it was closed.
  EXPECT_EQ(cache_size_after_put, Size(origin1_));

  // Delete the value. If the new padding key was used to deduct the padded size
  // then after deletion we would expect to see a non-zero cache size.
  EXPECT_TRUE(Delete(origin1_, "foo"));
  EXPECT_EQ(0, Size(origin1_));

  // Now put the exact same resource back into the cache. This time we expect to
  // see a different size as the padding is calculated with a different key.
  CacheStorageCacheHandle new_handle = std::move(callback_cache_handle_);
  EXPECT_TRUE(
      CachePut(new_handle.value(), kFooURL, FetchResponseType::kOpaque));

  EXPECT_NE(cache_size_after_put, Size(origin1_));
}

// With a memory cache the cache can't be freed from memory until the client
// calls delete.
TEST_F(CacheStorageManagerMemoryOnlyTest, MemoryLosesReferenceOnlyAfterDelete) {
  EXPECT_TRUE(Open(origin1_, "foo"));
  base::WeakPtr<LegacyCacheStorageCache> cache =
      LegacyCacheStorageCache::From(callback_cache_handle_)->AsWeakPtr();
  callback_cache_handle_ = CacheStorageCacheHandle();
  EXPECT_TRUE(cache);
  EXPECT_TRUE(Delete(origin1_, "foo"));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(cache);
}

TEST_P(CacheStorageManagerTestP, DeleteBeforeRelease) {
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(Delete(origin1_, "foo"));
  EXPECT_TRUE(callback_cache_handle_.value());
}

TEST_P(CacheStorageManagerLegacyOnlyTestP, OpenRunsSerially) {
  EXPECT_FALSE(Delete(origin1_, "tmp"));  // Init storage.
  CacheStorageHandle cache_storage = CacheStorageForOrigin(origin1_);
  auto* impl = LegacyCacheStorage::From(cache_storage);
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

TEST_P(CacheStorageManagerTestP, GetOriginUsage) {
  EXPECT_EQ(0, GetOriginUsage(origin1_));
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_EQ(0, GetOriginUsage(origin1_));
  EXPECT_TRUE(
      CachePut(callback_cache_handle_.value(), GURL("http://example.com/foo")));
  int64_t foo_size = GetOriginUsage(origin1_);
  EXPECT_LT(0, GetOriginUsage(origin1_));
  EXPECT_EQ(0, GetOriginUsage(origin2_));

  // Add the same entry into a second cache, the size should double.
  EXPECT_TRUE(Open(origin1_, "bar"));
  EXPECT_TRUE(
      CachePut(callback_cache_handle_.value(), GURL("http://example.com/foo")));
  EXPECT_EQ(2 * foo_size, GetOriginUsage(origin1_));
}

TEST_P(CacheStorageManagerTestP, GetAllOriginsUsage) {
  EXPECT_EQ(0ULL, GetAllOriginsUsage().size());
  // Put one entry in a cache on origin 1.
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(
      CachePut(callback_cache_handle_.value(), GURL("http://example.com/foo")));

  // Put two entries (of identical size) in a cache on origin 2.
  EXPECT_TRUE(Open(origin2_, "foo"));
  EXPECT_TRUE(
      CachePut(callback_cache_handle_.value(), GURL("http://example.com/foo")));
  EXPECT_TRUE(
      CachePut(callback_cache_handle_.value(), GURL("http://example.com/bar")));

  std::vector<StorageUsageInfo> usage = GetAllOriginsUsage();
  EXPECT_EQ(2ULL, usage.size());

  int origin1_index = usage[0].origin == origin1_ ? 0 : 1;
  int origin2_index = usage[1].origin == origin2_ ? 1 : 0;
  EXPECT_NE(origin1_index, origin2_index);

  int64_t origin1_size = usage[origin1_index].total_size_bytes;
  int64_t origin2_size = usage[origin2_index].total_size_bytes;
  EXPECT_EQ(2 * origin1_size, origin2_size);

  if (MemoryOnly()) {
    EXPECT_TRUE(usage[origin1_index].last_modified.is_null());
    EXPECT_TRUE(usage[origin2_index].last_modified.is_null());
  } else {
    EXPECT_FALSE(usage[origin1_index].last_modified.is_null());
    EXPECT_FALSE(usage[origin2_index].last_modified.is_null());
  }
}

TEST_P(CacheStorageManagerTestP, GetAllOriginsUsageDifferentOwners) {
  EXPECT_EQ(0ULL, GetAllOriginsUsage(CacheStorageOwner::kCacheAPI).size());
  EXPECT_EQ(0ULL,
            GetAllOriginsUsage(CacheStorageOwner::kBackgroundFetch).size());

  // Put one entry in a cache of owner 1.
  EXPECT_TRUE(Open(origin1_, "foo", CacheStorageOwner::kCacheAPI));
  EXPECT_TRUE(
      CachePut(callback_cache_handle_.value(), GURL("http://example.com/foo")));

  // Put two entries (of identical size) in two origins in a cache of owner 2.
  EXPECT_TRUE(Open(origin1_, "foo", CacheStorageOwner::kBackgroundFetch));
  EXPECT_TRUE(
      CachePut(callback_cache_handle_.value(), GURL("http://example.com/foo")));
  EXPECT_TRUE(Open(origin2_, "foo", CacheStorageOwner::kBackgroundFetch));
  EXPECT_TRUE(
      CachePut(callback_cache_handle_.value(), GURL("http://example.com/bar")));

  std::vector<StorageUsageInfo> usage_cache =
      GetAllOriginsUsage(CacheStorageOwner::kCacheAPI);
  EXPECT_EQ(1ULL, usage_cache.size());
  std::vector<StorageUsageInfo> usage_bgf =
      GetAllOriginsUsage(CacheStorageOwner::kBackgroundFetch);
  EXPECT_EQ(2ULL, usage_bgf.size());

  int origin1_index = usage_bgf[0].origin == origin1_ ? 0 : 1;
  int origin2_index = usage_bgf[1].origin == origin2_ ? 1 : 0;
  EXPECT_NE(origin1_index, origin2_index);

  EXPECT_EQ(usage_cache[0].origin, origin1_);
  EXPECT_EQ(usage_bgf[origin1_index].origin, origin1_);
  EXPECT_EQ(usage_bgf[origin2_index].origin, origin2_);

  EXPECT_EQ(usage_cache[0].total_size_bytes,
            usage_bgf[origin1_index].total_size_bytes);

  if (MemoryOnly()) {
    EXPECT_TRUE(usage_cache[0].last_modified.is_null());
    EXPECT_TRUE(usage_bgf[origin1_index].last_modified.is_null());
    EXPECT_TRUE(usage_bgf[origin2_index].last_modified.is_null());
  } else {
    EXPECT_FALSE(usage_cache[0].last_modified.is_null());
    EXPECT_FALSE(usage_bgf[origin1_index].last_modified.is_null());
    EXPECT_FALSE(usage_bgf[origin2_index].last_modified.is_null());
  }
}

// TODO(crbug.com/760687): Flaky on Fuchsia.
#if defined(OS_FUCHSIA)
#define MAYBE_GetAllOriginsUsageWithOldIndex DISABLED_GetAllOriginsUsageWithOldIndex
#else
#define MAYBE_GetAllOriginsUsageWithOldIndex GetAllOriginsUsageWithOldIndex
#endif
TEST_F(CacheStorageManagerTest, MAYBE_GetAllOriginsUsageWithOldIndex) {
  // Write a single value (V1) to the cache.
  const GURL kFooURL = origin1_.GetURL().Resolve("foo");
  const std::string kCacheName = "foo";
  EXPECT_TRUE(Open(origin1_, kCacheName));
  CacheStorageCacheHandle original_handle = std::move(callback_cache_handle_);

  EXPECT_TRUE(CachePut(original_handle.value(), kFooURL));
  int64_t cache_size_v1 = Size(origin1_);
  base::FilePath storage_dir =
      LegacyCacheStorageCache::From(original_handle)->path().DirName();
  original_handle = CacheStorageCacheHandle();
  EXPECT_GE(cache_size_v1, 0);

  // Close the caches and cache manager.
  EXPECT_TRUE(FlushCacheStorageIndex(origin1_));
  DestroyStorageManager();

  // Save a copy of the V1 index.
  EXPECT_TRUE(IsIndexFileCurrent(storage_dir));
  base::FilePath index_path = storage_dir.AppendASCII("index.txt");
  EXPECT_TRUE(base::PathExists(index_path));
  base::FilePath backup_index_path = storage_dir.AppendASCII("index.txt.bak");
  EXPECT_TRUE(base::CopyFile(index_path, backup_index_path));

  // Create a new CacheStorageManager that hasn't yet loaded the origin.
  CreateStorageManager();
  quota_manager_proxy_->SimulateQuotaManagerDestroyed();
  RecreateStorageManager();

  // Create a second value (V2) in the cache.
  EXPECT_TRUE(Open(origin1_, kCacheName));
  original_handle = std::move(callback_cache_handle_);
  const GURL kBarURL = origin1_.GetURL().Resolve("bar");
  EXPECT_TRUE(CachePut(original_handle.value(), kBarURL));
  original_handle = CacheStorageCacheHandle();

  // Capture the size before the index has necessarily flushed to disk.
  std::vector<StorageUsageInfo> usage = GetAllOriginsUsage();
  ASSERT_EQ(1ULL, usage.size());
  int64_t usage_before_close = usage[0].total_size_bytes;
  EXPECT_GT(usage_before_close, 0);

  // Flush the index to ensure we can read it correctly from the index file.
  EXPECT_TRUE(FlushCacheStorageIndex(origin1_));

  // Close the caches and cache manager.
  DestroyStorageManager();

  CreateStorageManager();
  quota_manager_proxy_->SimulateQuotaManagerDestroyed();
  RecreateStorageManager();

  // Read the size from the index file.
  CreateStorageManager();
  usage = GetAllOriginsUsage();
  ASSERT_EQ(1ULL, usage.size());
  EXPECT_EQ(usage_before_close, usage[0].total_size_bytes);

  DestroyStorageManager();

  // Restore the index to the V1 state. Make the access/mod times of index file
  // older than the other directories in the store to trigger size
  // recalculation.
  EXPECT_TRUE(base::CopyFile(backup_index_path, index_path));
  base::Time t = base::Time::Now() - base::TimeDelta::FromHours(1);
  EXPECT_TRUE(base::TouchFile(index_path, t, t));
  EXPECT_FALSE(IsIndexFileCurrent(storage_dir));

  // Read the size with the stale index file forcing a recalculation.
  CreateStorageManager();
  usage = GetAllOriginsUsage();
  ASSERT_EQ(1ULL, usage.size());

  EXPECT_EQ(usage_before_close, usage[0].total_size_bytes);

  EXPECT_FALSE(usage[0].last_modified.is_null());
}

// TODO(crbug.com/760687): Flaky on Fuchsia.
#if defined(OS_FUCHSIA)
#define MAYBE_GetOriginSizeWithOldIndex DISABLED_GetOriginSizeWithOldIndex
#else
#define MAYBE_GetOriginSizeWithOldIndex GetOriginSizeWithOldIndex
#endif
TEST_F(CacheStorageManagerTest, MAYBE_GetOriginSizeWithOldIndex) {
  // Write a single value (V1) to the cache.
  const GURL kFooURL = origin1_.GetURL().Resolve("foo");
  const std::string kCacheName = "foo";
  EXPECT_TRUE(Open(origin1_, kCacheName));
  CacheStorageCacheHandle original_handle = std::move(callback_cache_handle_);

  EXPECT_TRUE(CachePut(original_handle.value(), kFooURL));
  int64_t cache_size_v1 = Size(origin1_);
  base::FilePath storage_dir =
      LegacyCacheStorageCache::From(original_handle)->path().DirName();
  original_handle = CacheStorageCacheHandle();
  EXPECT_GE(cache_size_v1, 0);

  // Close the caches and cache manager.
  EXPECT_TRUE(FlushCacheStorageIndex(origin1_));
  DestroyStorageManager();

  // Save a copy of the V1 index.
  EXPECT_TRUE(IsIndexFileCurrent(storage_dir));
  base::FilePath index_path = storage_dir.AppendASCII("index.txt");
  EXPECT_TRUE(base::PathExists(index_path));
  base::FilePath backup_index_path = storage_dir.AppendASCII("index.txt.bak");
  EXPECT_TRUE(base::CopyFile(index_path, backup_index_path));

  // Create a new CacheStorageManager that hasn't yet loaded the origin.
  CreateStorageManager();
  quota_manager_proxy_->SimulateQuotaManagerDestroyed();
  RecreateStorageManager();

  // Reopen the cache and write a second value (V2).
  EXPECT_TRUE(Open(origin1_, kCacheName));
  original_handle = std::move(callback_cache_handle_);
  const GURL kBarURL = origin1_.GetURL().Resolve("bar");
  EXPECT_TRUE(CachePut(original_handle.value(), kBarURL));
  original_handle = CacheStorageCacheHandle();
  int64_t cache_size_v2 = Size(origin1_);
  EXPECT_GE(cache_size_v2, 0);

  // Close the caches and cache manager.
  DestroyStorageManager();

  // Restore the index to the V1 state.
  EXPECT_TRUE(base::CopyFile(backup_index_path, index_path));

  // Make the access/mod times of index file older than the other files in the
  // cache to trigger size recalculation.
  base::Time t = base::Time::Now() - base::TimeDelta::FromHours(1);
  EXPECT_TRUE(base::TouchFile(index_path, t, t));
  EXPECT_FALSE(IsIndexFileCurrent(storage_dir));

  // Reopen the cache and ensure the size is correct for the V2 value.
  CreateStorageManager();
  EXPECT_TRUE(Open(origin1_, kCacheName));
  EXPECT_EQ(cache_size_v2, Size(origin1_));
}

TEST_P(CacheStorageManagerLegacyOnlyTestP, GetSizeThenCloseAllCaches) {
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(
      CachePut(callback_cache_handle_.value(), GURL("http://example.com/foo")));
  EXPECT_TRUE(CachePut(callback_cache_handle_.value(),
                       GURL("http://example.com/foo2")));
  EXPECT_TRUE(Open(origin1_, "bar"));
  EXPECT_TRUE(
      CachePut(callback_cache_handle_.value(), GURL("http://example.com/bar")));

  int64_t origin_size = GetOriginUsage(origin1_);
  EXPECT_LT(0, origin_size);

  EXPECT_EQ(origin_size, GetSizeThenCloseAllCaches(origin1_));
  EXPECT_FALSE(
      CachePut(callback_cache_handle_.value(), GURL("http://example.com/baz")));
}

TEST_P(CacheStorageManagerLegacyOnlyTestP, GetSizeThenCloseAllCachesTwoOwners) {
  EXPECT_TRUE(Open(origin1_, "foo", CacheStorageOwner::kCacheAPI));
  CacheStorageCacheHandle public_handle = std::move(callback_cache_handle_);
  EXPECT_TRUE(Open(origin1_, "foo", CacheStorageOwner::kBackgroundFetch));
  CacheStorageCacheHandle bgf_handle = std::move(callback_cache_handle_);

  EXPECT_TRUE(
      CachePut(public_handle.value(), GURL("http://example.com/public")));
  EXPECT_TRUE(CachePut(bgf_handle.value(), GURL("http://example.com/bgf")));

  int64_t origin_size = GetOriginUsage(origin1_);
  EXPECT_LT(0, origin_size);

  EXPECT_EQ(origin_size, GetSizeThenCloseAllCaches(origin1_));
  EXPECT_FALSE(CachePut(public_handle.value(), GURL("http://example.com/baz")));
}

TEST_P(CacheStorageManagerLegacyOnlyTestP,
       GetSizeThenCloseAllCachesAfterDelete) {
  // Tests that doomed caches are also deleted by GetSizeThenCloseAllCaches.
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(
      CachePut(callback_cache_handle_.value(), GURL("http://example.com/foo")));

  int64_t size_after_put = GetOriginUsage(origin1_);
  EXPECT_LT(0, size_after_put);

  // Keep a handle to a (soon-to-be deleted cache).
  auto saved_cache_handle = callback_cache_handle_.Clone();

  // Delete will only doom the cache because there is still at least one handle
  // referencing an open cache.
  EXPECT_TRUE(Delete(origin1_, "foo"));

  // GetSizeThenCloseAllCaches should close the cache (which is then deleted)
  // even though there is still an open handle.
  EXPECT_EQ(size_after_put, GetSizeThenCloseAllCaches(origin1_));
  EXPECT_EQ(0, GetOriginUsage(origin1_));
}

TEST_F(CacheStorageManagerTest, DeleteUnreferencedCacheDirectories) {
  // Create a referenced cache.
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(
      CachePut(callback_cache_handle_.value(), GURL("http://example.com/foo")));

  // Create an unreferenced directory next to the referenced one.
  auto* legacy_manager =
      static_cast<LegacyCacheStorageManager*>(cache_manager_.get());
  base::FilePath origin_path = LegacyCacheStorageManager::ConstructOriginPath(
      legacy_manager->root_path(), origin1_, CacheStorageOwner::kCacheAPI);
  base::FilePath unreferenced_path = origin_path.AppendASCII("bar");
  EXPECT_TRUE(CreateDirectory(unreferenced_path));
  EXPECT_TRUE(base::DirectoryExists(unreferenced_path));

  // Create a new StorageManager so that the next time the cache is opened
  // the unreferenced directory can be deleted.
  quota_manager_proxy_->SimulateQuotaManagerDestroyed();
  RecreateStorageManager();

  // Verify that the referenced cache still works.
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(CacheMatch(callback_cache_handle_.value(),
                         GURL("http://example.com/foo")));

  // Verify that the unreferenced cache is gone.
  EXPECT_FALSE(base::DirectoryExists(unreferenced_path));
}

TEST_P(CacheStorageManagerTestP, OpenCacheStorageAccessed) {
  EXPECT_EQ(0, quota_manager_proxy_->notify_storage_accessed_count());
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_EQ(1, quota_manager_proxy_->notify_storage_accessed_count());
}

TEST_P(CacheStorageManagerTestP, HasStorageAccessed) {
  EXPECT_EQ(0, quota_manager_proxy_->notify_storage_accessed_count());
  EXPECT_FALSE(Has(origin1_, "foo"));
  EXPECT_EQ(1, quota_manager_proxy_->notify_storage_accessed_count());
}

TEST_P(CacheStorageManagerTestP, DeleteStorageAccessed) {
  EXPECT_EQ(0, quota_manager_proxy_->notify_storage_accessed_count());
  EXPECT_FALSE(Delete(origin1_, "foo"));
  EXPECT_EQ(1, quota_manager_proxy_->notify_storage_accessed_count());
}

TEST_P(CacheStorageManagerTestP, KeysStorageAccessed) {
  EXPECT_EQ(0, quota_manager_proxy_->notify_storage_accessed_count());
  EXPECT_EQ(0u, Keys(origin1_));
  EXPECT_EQ(1, quota_manager_proxy_->notify_storage_accessed_count());
}

TEST_P(CacheStorageManagerTestP, MatchCacheStorageAccessed) {
  EXPECT_EQ(0, quota_manager_proxy_->notify_storage_accessed_count());
  EXPECT_FALSE(StorageMatch(origin1_, "foo", GURL("http://example.com/foo")));
  EXPECT_EQ(1, quota_manager_proxy_->notify_storage_accessed_count());
}

TEST_P(CacheStorageManagerTestP, MatchAllCachesStorageAccessed) {
  EXPECT_EQ(0, quota_manager_proxy_->notify_storage_accessed_count());
  EXPECT_FALSE(StorageMatchAll(origin1_, GURL("http://example.com/foo")));
  EXPECT_EQ(1, quota_manager_proxy_->notify_storage_accessed_count());
}

TEST_P(CacheStorageManagerLegacyOnlyTestP, SizeStorageAccessed) {
  EXPECT_EQ(0, Size(origin1_));
  // Size is not part of the web API and should not notify the quota manager of
  // an access.
  EXPECT_EQ(0, quota_manager_proxy_->notify_storage_accessed_count());
}

TEST_P(CacheStorageManagerLegacyOnlyTestP, SizeThenCloseStorageAccessed) {
  EXPECT_EQ(0, GetSizeThenCloseAllCaches(origin1_));
  // GetSizeThenCloseAllCaches is not part of the web API and should not notify
  // the quota manager of an access.
  EXPECT_EQ(0, quota_manager_proxy_->notify_storage_accessed_count());
}

TEST_P(CacheStorageManagerTestP, NotifyCacheListChanged_Created) {
  TestCacheStorageObserver observer;
  observers_->AddObserver(&observer);

  EXPECT_EQ(0, observer.notify_list_changed_count);
  EXPECT_TRUE(Open(origin1_, "foo"));
  observer.Wait();
  EXPECT_EQ(1, observer.notify_list_changed_count);
  EXPECT_TRUE(
      CachePut(callback_cache_handle_.value(), GURL("http://example.com/foo")));
  observer.Wait();
  EXPECT_EQ(1, observer.notify_list_changed_count);
}

TEST_P(CacheStorageManagerTestP, NotifyCacheListChanged_Deleted) {
  TestCacheStorageObserver observer;
  observers_->AddObserver(&observer);

  EXPECT_EQ(0, observer.notify_list_changed_count);
  EXPECT_FALSE(Delete(origin1_, "foo"));
  // Give any unexpected observer tasks a chance to run.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, observer.notify_list_changed_count);
  EXPECT_TRUE(Open(origin1_, "foo"));
  observer.Wait();
  EXPECT_EQ(1, observer.notify_list_changed_count);
  EXPECT_TRUE(Delete(origin1_, "foo"));
  observer.Wait();
  EXPECT_EQ(2, observer.notify_list_changed_count);
}

TEST_P(CacheStorageManagerTestP, NotifyCacheListChanged_DeletedThenCreated) {
  TestCacheStorageObserver observer;
  observers_->AddObserver(&observer);

  EXPECT_EQ(0, observer.notify_list_changed_count);
  EXPECT_TRUE(Open(origin1_, "foo"));
  observer.Wait();
  EXPECT_EQ(1, observer.notify_list_changed_count);
  EXPECT_TRUE(Delete(origin1_, "foo"));
  observer.Wait();
  EXPECT_EQ(2, observer.notify_list_changed_count);
  EXPECT_TRUE(Open(origin2_, "foo2"));
  observer.Wait();
  EXPECT_EQ(3, observer.notify_list_changed_count);
}

TEST_P(CacheStorageManagerTestP, NotifyCacheContentChanged_PutEntry) {
  TestCacheStorageObserver observer;
  observers_->AddObserver(&observer);

  EXPECT_EQ(0, observer.notify_content_changed_count);
  EXPECT_TRUE(Open(origin1_, "foo"));
  observer.Wait();
  EXPECT_EQ(0, observer.notify_content_changed_count);
  EXPECT_TRUE(
      CachePut(callback_cache_handle_.value(), GURL("http://example.com/foo")));
  observer.Wait();
  EXPECT_EQ(1, observer.notify_content_changed_count);
  EXPECT_TRUE(CachePut(callback_cache_handle_.value(),
                       GURL("http://example.com/foo1")));
  observer.Wait();
  EXPECT_TRUE(CachePut(callback_cache_handle_.value(),
                       GURL("http://example.com/foo2")));
  observer.Wait();
  EXPECT_EQ(3, observer.notify_content_changed_count);
}

TEST_P(CacheStorageManagerTestP, NotifyCacheContentChanged_DeleteEntry) {
  TestCacheStorageObserver observer;
  observers_->AddObserver(&observer);

  EXPECT_EQ(0, observer.notify_content_changed_count);
  EXPECT_FALSE(Delete(origin1_, "foo"));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, observer.notify_content_changed_count);
  EXPECT_TRUE(Open(origin1_, "foo"));
  observer.Wait();
  EXPECT_EQ(0, observer.notify_content_changed_count);
  EXPECT_TRUE(
      CachePut(callback_cache_handle_.value(), GURL("http://example.com/foo")));
  observer.Wait();
  EXPECT_EQ(1, observer.notify_content_changed_count);
  EXPECT_TRUE(CacheDelete(callback_cache_handle_.value(),
                          GURL("http://example.com/foo")));
  observer.Wait();
  EXPECT_EQ(2, observer.notify_content_changed_count);
  EXPECT_FALSE(CacheDelete(callback_cache_handle_.value(),
                           GURL("http://example.com/foo")));
  // Give any unexpected observer tasks a chance to run.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, observer.notify_content_changed_count);
}

TEST_P(CacheStorageManagerTestP, NotifyCacheContentChanged_DeleteThenPutEntry) {
  TestCacheStorageObserver observer;
  observers_->AddObserver(&observer);

  EXPECT_EQ(0, observer.notify_content_changed_count);
  EXPECT_TRUE(Open(origin1_, "foo"));
  observer.Wait();
  EXPECT_EQ(0, observer.notify_content_changed_count);
  EXPECT_TRUE(
      CachePut(callback_cache_handle_.value(), GURL("http://example.com/foo")));
  observer.Wait();
  EXPECT_EQ(1, observer.notify_content_changed_count);
  EXPECT_TRUE(CacheDelete(callback_cache_handle_.value(),
                          GURL("http://example.com/foo")));
  observer.Wait();
  EXPECT_EQ(2, observer.notify_content_changed_count);
  EXPECT_TRUE(
      CachePut(callback_cache_handle_.value(), GURL("http://example.com/foo")));
  observer.Wait();
  EXPECT_EQ(3, observer.notify_content_changed_count);
  EXPECT_TRUE(CacheDelete(callback_cache_handle_.value(),
                          GURL("http://example.com/foo")));
  observer.Wait();
  EXPECT_EQ(4, observer.notify_content_changed_count);
}

TEST_P(CacheStorageManagerTestP, StorageMatch_IgnoreSearch) {
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(CachePut(callback_cache_handle_.value(),
                       GURL("http://example.com/foo?bar")));

  EXPECT_FALSE(StorageMatch(origin1_, "foo", GURL("http://example.com/foo")));

  blink::mojom::CacheQueryOptionsPtr match_options =
      blink::mojom::CacheQueryOptions::New();
  match_options->ignore_search = true;
  EXPECT_TRUE(StorageMatch(origin1_, "foo", GURL("http://example.com/foo"),
                           std::move(match_options)));
}

TEST_P(CacheStorageManagerTestP, StorageMatch_IgnoreMethod) {
  GURL url = GURL("http://example.com/foo");
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(CachePut(callback_cache_handle_.value(), url));

  auto post_request = blink::mojom::FetchAPIRequest::New();
  post_request->url = url;
  post_request->method = "POST";
  EXPECT_FALSE(StorageMatchWithRequest(
      origin1_, "foo",
      BackgroundFetchSettledFetch::CloneRequest(post_request)));

  blink::mojom::CacheQueryOptionsPtr match_options =
      blink::mojom::CacheQueryOptions::New();
  match_options->ignore_method = true;
  EXPECT_TRUE(StorageMatchWithRequest(origin1_, "foo", std::move(post_request),
                                      std::move(match_options)));
}

TEST_P(CacheStorageManagerTestP, StorageMatch_IgnoreVary) {
  GURL url = GURL("http://example.com/foo");
  EXPECT_TRUE(Open(origin1_, "foo"));

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
  EXPECT_TRUE(StorageMatchWithRequest(origin1_, "foo", std::move(request_2)));

  request->headers["vary_foo"] = "bar";
  // |request_3| and |request_4| has the same url and headers.
  auto request_3 = BackgroundFetchSettledFetch::CloneRequest(request);
  auto request_4 = BackgroundFetchSettledFetch::CloneRequest(request);
  EXPECT_FALSE(StorageMatchWithRequest(origin1_, "foo", std::move(request_3)));

  blink::mojom::CacheQueryOptionsPtr match_options =
      blink::mojom::CacheQueryOptions::New();
  match_options->ignore_vary = true;
  EXPECT_TRUE(StorageMatchWithRequest(origin1_, "foo", std::move(request_4),
                                      std::move(match_options)));
}

TEST_P(CacheStorageManagerTestP, StorageMatchAll_IgnoreSearch) {
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(CachePut(callback_cache_handle_.value(),
                       GURL("http://example.com/foo?bar")));

  EXPECT_FALSE(StorageMatchAll(origin1_, GURL("http://example.com/foo")));

  blink::mojom::CacheQueryOptionsPtr match_options =
      blink::mojom::CacheQueryOptions::New();
  match_options->ignore_search = true;
  EXPECT_TRUE(StorageMatchAll(origin1_, GURL("http://example.com/foo"),
                              std::move(match_options)));
}

TEST_P(CacheStorageManagerTestP, StorageMatchAll_IgnoreMethod) {
  GURL url = GURL("http://example.com/foo");
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(CachePut(callback_cache_handle_.value(), url));

  auto post_request = blink::mojom::FetchAPIRequest::New();
  post_request->url = url;
  post_request->method = "POST";
  EXPECT_FALSE(StorageMatchAllWithRequest(
      origin1_, BackgroundFetchSettledFetch::CloneRequest(post_request)));

  blink::mojom::CacheQueryOptionsPtr match_options =
      blink::mojom::CacheQueryOptions::New();
  match_options->ignore_method = true;
  EXPECT_TRUE(StorageMatchAllWithRequest(origin1_, std::move(post_request),
                                         std::move(match_options)));
}

TEST_P(CacheStorageManagerTestP, StorageMatchAll_IgnoreVary) {
  GURL url = GURL("http://example.com/foo");
  EXPECT_TRUE(Open(origin1_, "foo"));

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
  EXPECT_TRUE(StorageMatchAllWithRequest(origin1_, std::move(request_2)));

  request->headers["vary_foo"] = "bar";
  // |request_3| and |request_4| has the same url and headers.
  auto request_3 = BackgroundFetchSettledFetch::CloneRequest(request);
  auto request_4 = BackgroundFetchSettledFetch::CloneRequest(request);
  EXPECT_FALSE(StorageMatchAllWithRequest(origin1_, std::move(request_3)));

  blink::mojom::CacheQueryOptionsPtr match_options =
      blink::mojom::CacheQueryOptions::New();
  match_options->ignore_vary = true;
  EXPECT_TRUE(StorageMatchAllWithRequest(origin1_, std::move(request_4),
                                         std::move(match_options)));
}

TEST_P(CacheStorageManagerTestP, StorageWriteToCache) {
  EXPECT_TRUE(Open(origin1_, "foo", CacheStorageOwner::kBackgroundFetch));

  EXPECT_TRUE(Write(origin1_, CacheStorageOwner::kBackgroundFetch, "foo",
                    "http://example.com/foo"));

  // Match request we just wrote.
  EXPECT_TRUE(StorageMatch(origin1_, "foo", GURL("http://example.com/foo"),
                           nullptr, CacheStorageOwner::kBackgroundFetch));

  // Don't match with different origin.
  EXPECT_FALSE(StorageMatch(origin2_, "foo", GURL("http://example.com/foo"),
                            nullptr, CacheStorageOwner::kBackgroundFetch));
  // Don't match with different cache name.
  EXPECT_FALSE(StorageMatch(origin1_, "bar", GURL("http://example.com/foo"),
                            nullptr, CacheStorageOwner::kBackgroundFetch));
  // Don't match with different request.
  EXPECT_FALSE(StorageMatch(origin1_, "foo", GURL("http://example.com/bar"),
                            nullptr, CacheStorageOwner::kBackgroundFetch));
  // Don't match with different owner.
  EXPECT_FALSE(StorageMatch(origin1_, "foo", GURL("http://example.com/foo"),
                            nullptr, CacheStorageOwner::kCacheAPI));
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
  EXPECT_TRUE(Open(origin1_, kCacheName));
  EXPECT_FALSE(CacheMatch(callback_cache_handle_.value(), kNonExistant));
  EXPECT_TRUE(FlushCacheStorageIndex(origin1_));

  // Allow the cache to close.
  callback_cache_handle_ = CacheStorageCacheHandle();

  // Opening an existing cache should *not* require writing the index.
  EXPECT_TRUE(Open(origin1_, kCacheName));
  EXPECT_FALSE(CacheMatch(callback_cache_handle_.value(), kNonExistant));
  EXPECT_FALSE(FlushCacheStorageIndex(origin1_));

  // Putting a value in the cache should require writing the index.
  EXPECT_TRUE(CachePut(callback_cache_handle_.value(), kResource));
  EXPECT_TRUE(FlushCacheStorageIndex(origin1_));

  // Allow the cache to close.
  callback_cache_handle_ = CacheStorageCacheHandle();

  // Opening an existing cache after writing a value should *not* require
  // writing the index.
  EXPECT_TRUE(Open(origin1_, kCacheName));
  EXPECT_FALSE(CacheMatch(callback_cache_handle_.value(), kNonExistant));
  EXPECT_TRUE(CacheMatch(callback_cache_handle_.value(), kResource));
  EXPECT_FALSE(FlushCacheStorageIndex(origin1_));
}

TEST_P(CacheStorageManagerTestP, SlowPutCompletesWithoutExternalRef) {
  EXPECT_TRUE(Open(origin1_, "foo"));

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

class CacheStorageQuotaClientTest : public CacheStorageManagerTest {
 protected:
  CacheStorageQuotaClientTest() = default;

  void SetUp() override {
    CacheStorageManagerTest::SetUp();
    quota_client_ = base::MakeRefCounted<CacheStorageQuotaClient>(
        cache_manager_, CacheStorageOwner::kCacheAPI);
  }

  void QuotaUsageCallback(base::RunLoop* run_loop, int64_t usage) {
    callback_quota_usage_ = usage;
    run_loop->Quit();
  }

  void OriginsCallback(base::RunLoop* run_loop,
                       const std::vector<url::Origin>& origins) {
    callback_origins_ = origins;
    run_loop->Quit();
  }

  void DeleteOriginCallback(base::RunLoop* run_loop,
                            blink::mojom::QuotaStatusCode status) {
    callback_status_ = status;
    run_loop->Quit();
  }

  int64_t QuotaGetOriginUsage(const url::Origin& origin) {
    base::RunLoop loop;
    quota_client_->GetOriginUsage(
        origin, StorageType::kTemporary,
        base::BindOnce(&CacheStorageQuotaClientTest::QuotaUsageCallback,
                       base::Unretained(this), base::Unretained(&loop)));
    loop.Run();
    return callback_quota_usage_;
  }

  size_t QuotaGetOriginsForType() {
    base::RunLoop loop;
    quota_client_->GetOriginsForType(
        StorageType::kTemporary,
        base::BindOnce(&CacheStorageQuotaClientTest::OriginsCallback,
                       base::Unretained(this), base::Unretained(&loop)));
    loop.Run();
    return callback_origins_.size();
  }

  size_t QuotaGetOriginsForHost(const std::string& host) {
    base::RunLoop loop;
    quota_client_->GetOriginsForHost(
        StorageType::kTemporary, host,
        base::BindOnce(&CacheStorageQuotaClientTest::OriginsCallback,
                       base::Unretained(this), base::Unretained(&loop)));
    loop.Run();
    return callback_origins_.size();
  }

  bool QuotaDeleteOriginData(const url::Origin& origin) {
    base::RunLoop loop;
    quota_client_->DeleteOriginData(
        origin, StorageType::kTemporary,
        base::BindOnce(&CacheStorageQuotaClientTest::DeleteOriginCallback,
                       base::Unretained(this), base::Unretained(&loop)));
    loop.Run();
    return callback_status_ == blink::mojom::QuotaStatusCode::kOk;
  }

  scoped_refptr<CacheStorageQuotaClient> quota_client_;

  blink::mojom::QuotaStatusCode callback_status_;
  int64_t callback_quota_usage_ = 0;
  std::vector<url::Origin> callback_origins_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CacheStorageQuotaClientTest);
};

class CacheStorageQuotaClientDiskOnlyTest : public CacheStorageQuotaClientTest {
 public:
  bool MemoryOnly() override { return false; }
};

class CacheStorageQuotaClientTestP : public CacheStorageQuotaClientTest,
                                     public testing::WithParamInterface<Param> {
  bool MemoryOnly() override {
    return GetParam().storage_ == TestStorage::kMemory;
  }
  TestManager ManagerType() override { return GetParam().manager_; }
};

TEST_P(CacheStorageQuotaClientTestP, QuotaGetOriginUsage) {
  EXPECT_EQ(0, QuotaGetOriginUsage(origin1_));
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(
      CachePut(callback_cache_handle_.value(), GURL("http://example.com/foo")));
  EXPECT_LT(0, QuotaGetOriginUsage(origin1_));
}

TEST_P(CacheStorageQuotaClientTestP, QuotaGetOriginsForType) {
  EXPECT_EQ(0u, QuotaGetOriginsForType());
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(Open(origin1_, "bar"));
  EXPECT_TRUE(Open(origin2_, "foo"));
  EXPECT_EQ(2u, QuotaGetOriginsForType());
}

TEST_P(CacheStorageQuotaClientTestP, QuotaGetOriginsForTypeDifferentOwners) {
  EXPECT_EQ(0u, QuotaGetOriginsForType());
  EXPECT_TRUE(Open(origin1_, "foo"));
  // The |quota_client_| is registered for CacheStorageOwner::kCacheAPI, so this
  // Open is ignored.
  EXPECT_TRUE(Open(origin2_, "bar", CacheStorageOwner::kBackgroundFetch));
  EXPECT_EQ(1u, QuotaGetOriginsForType());
}

TEST_P(CacheStorageQuotaClientTestP, QuotaGetOriginsForHost) {
  EXPECT_EQ(0u, QuotaGetOriginsForHost("example.com"));
  EXPECT_TRUE(
      Open(url::Origin::Create(GURL("http://example.com:8080")), "foo"));
  EXPECT_TRUE(
      Open(url::Origin::Create(GURL("http://example.com:9000")), "foo"));
  EXPECT_TRUE(Open(url::Origin::Create(GURL("ftp://example.com")), "foo"));
  EXPECT_TRUE(Open(url::Origin::Create(GURL("http://example2.com")), "foo"));
  EXPECT_EQ(3u, QuotaGetOriginsForHost("example.com"));
  EXPECT_EQ(1u, QuotaGetOriginsForHost("example2.com"));
  EXPECT_THAT(
      callback_origins_,
      testing::Contains(url::Origin::Create(GURL("http://example2.com"))));
  EXPECT_EQ(0u, QuotaGetOriginsForHost("unknown.com"));
}

TEST_P(CacheStorageQuotaClientTestP, QuotaDeleteOriginData) {
  EXPECT_EQ(0, QuotaGetOriginUsage(origin1_));
  EXPECT_TRUE(Open(origin1_, "foo"));
  // Call put to test that initialized caches are properly deleted too.
  EXPECT_TRUE(
      CachePut(callback_cache_handle_.value(), GURL("http://example.com/foo")));
  EXPECT_TRUE(Open(origin1_, "bar"));
  EXPECT_TRUE(Open(origin2_, "baz"));

  int64_t origin1_size = QuotaGetOriginUsage(origin1_);
  EXPECT_LT(0, origin1_size);

  EXPECT_TRUE(QuotaDeleteOriginData(origin1_));

  EXPECT_EQ(-1 * origin1_size, quota_manager_proxy_->last_notified_delta());
  EXPECT_EQ(0, QuotaGetOriginUsage(origin1_));
  EXPECT_FALSE(Has(origin1_, "foo"));
  EXPECT_FALSE(Has(origin1_, "bar"));
  EXPECT_TRUE(Has(origin2_, "baz"));
  EXPECT_TRUE(Open(origin1_, "foo"));
}

TEST_P(CacheStorageQuotaClientTestP, QuotaDeleteEmptyOrigin) {
  EXPECT_TRUE(QuotaDeleteOriginData(origin1_));
}

TEST_F(CacheStorageQuotaClientDiskOnlyTest, QuotaDeleteUnloadedOriginData) {
  EXPECT_TRUE(Open(origin1_, "foo"));
  // Call put to test that initialized caches are properly deleted too.
  EXPECT_TRUE(
      CachePut(callback_cache_handle_.value(), GURL("http://example.com/foo")));

  // Close the cache backend so that it writes out its index to disk.
  base::RunLoop run_loop;
  LegacyCacheStorageCache::From(callback_cache_handle_)
      ->Close(run_loop.QuitClosure());
  run_loop.Run();

  // Create a new CacheStorageManager that hasn't yet loaded the origin.
  quota_manager_proxy_->SimulateQuotaManagerDestroyed();
  RecreateStorageManager();
  quota_client_ = base::MakeRefCounted<CacheStorageQuotaClient>(
      cache_manager_, CacheStorageOwner::kCacheAPI);

  EXPECT_TRUE(QuotaDeleteOriginData(origin1_));
  EXPECT_EQ(0, QuotaGetOriginUsage(origin1_));
}

INSTANTIATE_TEST_SUITE_P(
    CacheStorageManagerTests,
    CacheStorageManagerTestP,
    ::testing::Values(Param(TestManager::kLegacy, TestStorage::kMemory),
                      Param(TestManager::kLegacy, TestStorage::kDisk),
                      Param(TestManager::kCrossSequence, TestStorage::kMemory),
                      Param(TestManager::kCrossSequence, TestStorage::kDisk)));

INSTANTIATE_TEST_SUITE_P(CacheStorageManagerTests,
                         CacheStorageManagerLegacyOnlyTestP,
                         ::testing::Values(TestStorage::kMemory,
                                           TestStorage::kDisk));

INSTANTIATE_TEST_SUITE_P(
    CacheStorageQuotaClientTests,
    CacheStorageQuotaClientTestP,
    ::testing::Values(Param(TestManager::kLegacy, TestStorage::kMemory),
                      Param(TestManager::kLegacy, TestStorage::kDisk),
                      Param(TestManager::kCrossSequence, TestStorage::kMemory),
                      Param(TestManager::kCrossSequence, TestStorage::kDisk)));

}  // namespace cache_storage_manager_unittest
}  // namespace content
