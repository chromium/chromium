// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cache_storage/cache_storage_manager.h"

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <set>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/guid.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/sha1.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/cache_storage/cache_storage.h"
#include "content/browser/cache_storage/cache_storage.pb.h"
#include "content/browser/cache_storage/cache_storage_cache_handle.h"
#include "content/browser/cache_storage/cache_storage_context_impl.h"
#include "content/browser/cache_storage/cache_storage_index.h"
#include "content/browser/cache_storage/cache_storage_quota_client.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/cache_storage_usage_info.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "net/disk_cache/disk_cache.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_impl.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/blob/blob_url_request_job_factory.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/mock_quota_manager_proxy.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "storage/common/blob_storage/blob_handle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/modules/cache_storage/cache_storage.mojom.h"
#include "url/origin.h"

using blink::mojom::CacheStorageError;
using blink::mojom::CacheStorageVerboseErrorPtr;
using network::mojom::FetchResponseType;

namespace content {
namespace cache_storage_manager_unittest {

using blink::mojom::StorageType;
using ResponseHeaderMap = base::flat_map<std::string, std::string>;

class MockCacheStorageQuotaManagerProxy : public MockQuotaManagerProxy {
 public:
  MockCacheStorageQuotaManagerProxy(MockQuotaManager* quota_manager,
                                    base::SingleThreadTaskRunner* task_runner)
      : MockQuotaManagerProxy(quota_manager, task_runner) {}

  void RegisterClient(QuotaClient* client) override {
    registered_clients_.push_back(client);
  }

  void SimulateQuotaManagerDestroyed() override {
    for (auto* client : registered_clients_) {
      client->OnQuotaManagerDestroyed();
    }
    registered_clients_.clear();
  }

 private:
  ~MockCacheStorageQuotaManagerProxy() override {
    DCHECK(registered_clients_.empty());
  }

  std::vector<QuotaClient*> registered_clients_;
};

bool IsIndexFileCurrent(const base::FilePath& cache_dir) {
  base::File::Info info;
  const base::FilePath index_path =
      cache_dir.AppendASCII(CacheStorage::kIndexFileName);
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

void CopyCacheStorageIndex(CacheStorageIndex* dest,
                           const CacheStorageIndex& src) {
  DCHECK_EQ(0U, dest->num_entries());
  for (const auto& cache_metadata : src.ordered_cache_metadata())
    dest->Insert(cache_metadata);
}

class TestCacheStorageObserver : public CacheStorageContextImpl::Observer {
 public:
  void OnCacheListChanged(const url::Origin& origin) override {
    ++notify_list_changed_count;
  }

  void OnCacheContentChanged(const url::Origin& origin,
                             const std::string& cache_name) override {
    ++notify_content_changed_count;
  }

  int notify_list_changed_count = 0;
  int notify_content_changed_count = 0;
};

// Returns a BlobProtocolHandler that uses |blob_storage_context|. Caller owns
// the memory.
std::unique_ptr<storage::BlobProtocolHandler> CreateMockBlobProtocolHandler(
    storage::BlobStorageContext* blob_storage_context) {
  return base::WrapUnique(
      new storage::BlobProtocolHandler(blob_storage_context));
}

class CacheStorageManagerTest : public testing::Test {
 public:
  CacheStorageManagerTest()
      : browser_thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP),
        blob_storage_context_(nullptr),
        callback_bool_(false),
        callback_error_(CacheStorageError::kSuccess),
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
                             const CacheStorageIndex& cache_index) {
    callback_cache_index_ = CacheStorageIndex();
    CopyCacheStorageIndex(&callback_cache_index_, cache_index);
    run_loop->Quit();
  }

  const std::string& GetFirstIndexName() const {
    return callback_cache_index_.ordered_cache_metadata().front().name;
  }

  std::vector<std::string> GetIndexNames() const {
    std::vector<std::string> cache_names;
    for (const auto& metadata : callback_cache_index_.ordered_cache_metadata())
      cache_names.push_back(metadata.name);
    return cache_names;
  }

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

  void CreateStorageManager() {
    ChromeBlobStorageContext* blob_storage_context(
        ChromeBlobStorageContext::GetFor(&browser_context_));
    // Wait for ChromeBlobStorageContext to finish initializing.
    base::RunLoop().RunUntilIdle();

    blob_storage_context_ = blob_storage_context->context();

    url_request_job_factory_.reset(new net::URLRequestJobFactoryImpl);
    url_request_job_factory_->SetProtocolHandler(
        "blob", CreateMockBlobProtocolHandler(blob_storage_context->context()));

    net::URLRequestContext* url_request_context =
        BrowserContext::GetDefaultStoragePartition(&browser_context_)->
              GetURLRequestContext()->GetURLRequestContext();

    url_request_context->set_job_factory(url_request_job_factory_.get());

    base::FilePath temp_dir_path;
    if (!MemoryOnly())
      temp_dir_path = temp_dir_.GetPath();

    quota_policy_ = new MockSpecialStoragePolicy;
    mock_quota_manager_ = new MockQuotaManager(
        MemoryOnly(), temp_dir_path, base::ThreadTaskRunnerHandle::Get().get(),
        quota_policy_.get());
    mock_quota_manager_->SetQuota(origin1_, StorageType::kTemporary,
                                  1024 * 1024 * 100);
    mock_quota_manager_->SetQuota(origin2_, StorageType::kTemporary,
                                  1024 * 1024 * 100);

    quota_manager_proxy_ = new MockCacheStorageQuotaManagerProxy(
        mock_quota_manager_.get(), base::ThreadTaskRunnerHandle::Get().get());

    cache_manager_ = CacheStorageManager::Create(
        temp_dir_path, base::ThreadTaskRunnerHandle::Get(),
        quota_manager_proxy_);

    cache_manager_->SetBlobParametersForCache(
        BrowserContext::GetDefaultStoragePartition(&browser_context_)
            ->GetURLRequestContext(),
        blob_storage_context->context()->AsWeakPtr());
  }

  bool FlushCacheStorageIndex(const url::Origin& origin) {
    callback_bool_ = false;
    base::RunLoop loop;
    bool write_was_scheduled =
        CacheStorageForOrigin(origin)->InitiateScheduledIndexWriteForTest(
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
    callback_cache_index_ = CacheStorageIndex();
    callback_all_origins_usage_.clear();

    base::RunLoop().RunUntilIdle();
    quota_manager_proxy_ = nullptr;

    url_request_job_factory_.reset();
    blob_storage_context_ = nullptr;

    quota_policy_ = nullptr;
    mock_quota_manager_ = nullptr;

    cache_manager_ = nullptr;
  }

  bool Open(const url::Origin& origin,
            const std::string& cache_name,
            CacheStorageOwner owner = CacheStorageOwner::kCacheAPI) {
    base::RunLoop loop;
    cache_manager_->OpenCache(
        origin, owner, cache_name,
        base::BindOnce(&CacheStorageManagerTest::CacheAndErrorCallback,
                       base::Unretained(this), base::Unretained(&loop)));
    loop.Run();

    bool error = callback_error_ != CacheStorageError::kSuccess;
    if (error)
      EXPECT_FALSE(callback_cache_handle_.value());
    else
      EXPECT_TRUE(callback_cache_handle_.value());
    return !error;
  }

  bool Has(const url::Origin& origin,
           const std::string& cache_name,
           CacheStorageOwner owner = CacheStorageOwner::kCacheAPI) {
    base::RunLoop loop;
    cache_manager_->HasCache(
        origin, owner, cache_name,
        base::BindOnce(&CacheStorageManagerTest::BoolAndErrorCallback,
                       base::Unretained(this), base::Unretained(&loop)));
    loop.Run();

    return callback_bool_;
  }

  bool Delete(const url::Origin& origin,
              const std::string& cache_name,
              CacheStorageOwner owner = CacheStorageOwner::kCacheAPI) {
    base::RunLoop loop;
    cache_manager_->DeleteCache(
        origin, owner, cache_name,
        base::BindOnce(&CacheStorageManagerTest::ErrorCallback,
                       base::Unretained(this), base::Unretained(&loop)));
    loop.Run();

    return callback_bool_;
  }

  size_t Keys(const url::Origin& origin,
              CacheStorageOwner owner = CacheStorageOwner::kCacheAPI) {
    base::RunLoop loop;
    cache_manager_->EnumerateCaches(
        origin, owner,
        base::BindOnce(&CacheStorageManagerTest::CacheMetadataCallback,
                       base::Unretained(this), base::Unretained(&loop)));
    loop.Run();
    return callback_cache_index_.num_entries();
  }

  bool StorageMatch(const url::Origin& origin,
                    const std::string& cache_name,
                    const GURL& url,
                    blink::mojom::QueryParamsPtr match_params = nullptr,
                    CacheStorageOwner owner = CacheStorageOwner::kCacheAPI) {
    ServiceWorkerFetchRequest request;
    request.url = url;
    return StorageMatchWithRequest(origin, cache_name, request,
                                   std::move(match_params), owner);
  }

  bool StorageMatchWithRequest(
      const url::Origin& origin,
      const std::string& cache_name,
      const ServiceWorkerFetchRequest& request,
      blink::mojom::QueryParamsPtr match_params = nullptr,
      CacheStorageOwner owner = CacheStorageOwner::kCacheAPI) {
    std::unique_ptr<ServiceWorkerFetchRequest> unique_request =
        std::make_unique<ServiceWorkerFetchRequest>(request);

    base::RunLoop loop;
    cache_manager_->MatchCache(
        origin, owner, cache_name, std::move(unique_request),
        std::move(match_params),
        base::BindOnce(&CacheStorageManagerTest::CacheMatchCallback,
                       base::Unretained(this), base::Unretained(&loop)));
    loop.Run();

    return callback_error_ == CacheStorageError::kSuccess;
  }

  bool StorageMatchAll(const url::Origin& origin,
                       const GURL& url,
                       blink::mojom::QueryParamsPtr match_params = nullptr) {
    ServiceWorkerFetchRequest request;
    request.url = url;
    return StorageMatchAllWithRequest(origin, request, std::move(match_params));
  }

  bool StorageMatchAllWithRequest(
      const url::Origin& origin,
      const ServiceWorkerFetchRequest& request,
      blink::mojom::QueryParamsPtr match_params = nullptr,
      CacheStorageOwner owner = CacheStorageOwner::kCacheAPI) {
    std::unique_ptr<ServiceWorkerFetchRequest> unique_request =
        std::make_unique<ServiceWorkerFetchRequest>(request);
    base::RunLoop loop;
    cache_manager_->MatchAllCaches(
        origin, owner, std::move(unique_request), std::move(match_params),
        base::BindOnce(&CacheStorageManagerTest::CacheMatchCallback,
                       base::Unretained(this), base::Unretained(&loop)));
    loop.Run();

    return callback_error_ == CacheStorageError::kSuccess;
  }

  bool Write(const url::Origin& origin,
             CacheStorageOwner owner,
             const std::string& cache_name,
             const std::string& request_url) {
    auto request = std::make_unique<ServiceWorkerFetchRequest>();
    request->url = GURL(request_url);

    base::RunLoop loop;
    cache_manager_->WriteToCache(
        origin, owner, cache_name, std::move(request),
        blink::mojom::FetchAPIResponse::New(),
        base::BindOnce(&CacheStorageManagerTest::ErrorCallback,
                       base::Unretained(this), base::Unretained(&loop)));
    loop.Run();
    return callback_bool_;
  }

  bool CachePut(CacheStorageCache* cache,
                const GURL& url,
                FetchResponseType response_type = FetchResponseType::kDefault) {
    ServiceWorkerFetchRequest request;
    request.url = url;

    return CachePutWithStatusCode(cache, request, 200, response_type);
  }

  bool CachePutWithRequestAndHeaders(
      CacheStorageCache* cache,
      const ServiceWorkerFetchRequest& request,
      ResponseHeaderMap response_headers,
      FetchResponseType response_type = FetchResponseType::kDefault) {
    return CachePutWithStatusCode(cache, request, 200, response_type,
                                  std::move(response_headers));
  }

  bool CachePutWithStatusCode(
      CacheStorageCache* cache,
      const ServiceWorkerFetchRequest& request,
      int status_code,
      FetchResponseType response_type = FetchResponseType::kDefault,
      ResponseHeaderMap response_headers = ResponseHeaderMap()) {
    std::string blob_uuid = base::GenerateGUID();
    std::unique_ptr<storage::BlobDataBuilder> blob_data(
        new storage::BlobDataBuilder(blob_uuid));
    blob_data->AppendData(request.url.spec());

    std::unique_ptr<storage::BlobDataHandle> blob_data_handle =
        blob_storage_context_->AddFinishedBlob(std::move(blob_data));

    blink::mojom::BlobPtrInfo blob_ptr_info;
    storage::BlobImpl::Create(std::move(blob_data_handle),
                              MakeRequest(&blob_ptr_info));

    auto blob = blink::mojom::SerializedBlob::New();
    blob->uuid = blob_uuid;
    blob->size = request.url.spec().size();
    blob->blob = std::move(blob_ptr_info);

    auto response = blink::mojom::FetchAPIResponse::New(
        std::vector<GURL>({request.url}), status_code, "OK", response_type,
        response_headers, std::move(blob),
        blink::mojom::ServiceWorkerResponseError::kUnknown, base::Time(),
        std::string() /* cache_storage_cache_name */,
        std::vector<std::string>() /* cors_exposed_header_names */,
        false /* is_in_cache_storage */, nullptr /* side_data_blob */);

    blink::mojom::BatchOperationPtr operation =
        blink::mojom::BatchOperation::New();
    operation->operation_type = blink::mojom::OperationType::kPut;
    operation->request = request;
    operation->response = std::move(response);

    std::vector<blink::mojom::BatchOperationPtr> operations;
    operations.emplace_back(std::move(operation));
    base::RunLoop loop;
    cache->BatchOperation(
        std::move(operations), true /* fail_on_duplicate */,
        base::BindOnce(&CacheStorageManagerTest::CachePutCallback,
                       base::Unretained(this), base::Unretained(&loop)),
        CacheStorageCache::BadMessageCallback());
    loop.Run();

    return callback_error_ == CacheStorageError::kSuccess;
  }

  bool CacheDelete(CacheStorageCache* cache, const GURL& url) {
    ServiceWorkerFetchRequest request;
    request.url = url;

    blink::mojom::BatchOperationPtr operation =
        blink::mojom::BatchOperation::New();
    operation->operation_type = blink::mojom::OperationType::kDelete;
    operation->request = request;
    operation->response = blink::mojom::FetchAPIResponse::New();

    std::vector<blink::mojom::BatchOperationPtr> operations;
    operations.emplace_back(std::move(operation));
    base::RunLoop loop;
    cache->BatchOperation(
        std::move(operations), true /* fail_on_duplicate */,
        base::BindOnce(&CacheStorageManagerTest::CacheDeleteCallback,
                       base::Unretained(this), base::Unretained(&loop)),
        CacheStorageCache::BadMessageCallback());
    loop.Run();

    return callback_error_ == CacheStorageError::kSuccess;
  }

  bool CacheMatch(CacheStorageCache* cache, const GURL& url) {
    std::unique_ptr<ServiceWorkerFetchRequest> request(
        new ServiceWorkerFetchRequest());
    request->url = url;
    base::RunLoop loop;
    cache->Match(
        std::move(request), nullptr,
        base::BindOnce(&CacheStorageManagerTest::CacheMatchCallback,
                       base::Unretained(this), base::Unretained(&loop)));
    loop.Run();

    return callback_error_ == CacheStorageError::kSuccess;
  }

  CacheStorage* CacheStorageForOrigin(const url::Origin& origin) {
    return cache_manager_->FindOrCreateCacheStorage(
        origin, CacheStorageOwner::kCacheAPI);
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

  std::vector<CacheStorageUsageInfo> GetAllOriginsUsage(
      CacheStorageOwner owner = CacheStorageOwner::kCacheAPI) {
    base::RunLoop loop;
    cache_manager_->GetAllOriginsUsage(
        owner, base::BindLambdaForTesting(
                   [&](const std::vector<CacheStorageUsageInfo>& usage) {
                     callback_all_origins_usage_ = usage;
                     loop.Quit();
                   }));
    loop.Run();
    return callback_all_origins_usage_;
  }

  int64_t GetSizeThenCloseAllCaches(const url::Origin& origin) {
    base::RunLoop loop;
    CacheStorage* cache_storage = CacheStorageForOrigin(origin);
    cache_storage->GetSizeThenCloseAllCaches(
        base::BindOnce(&CacheStorageManagerTest::UsageCallback,
                       base::Unretained(this), &loop));
    loop.Run();
    return callback_usage_;
  }

  int64_t Size(const url::Origin& origin) {
    base::RunLoop loop;
    CacheStorage* cache_storage = CacheStorageForOrigin(origin);
    cache_storage->Size(base::BindOnce(&CacheStorageManagerTest::UsageCallback,
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

  TestBrowserThreadBundle browser_thread_bundle_;
  TestBrowserContext browser_context_;
  std::unique_ptr<net::URLRequestJobFactoryImpl> url_request_job_factory_;
  storage::BlobStorageContext* blob_storage_context_;

  scoped_refptr<MockSpecialStoragePolicy> quota_policy_;
  scoped_refptr<MockQuotaManager> mock_quota_manager_;
  scoped_refptr<MockCacheStorageQuotaManagerProxy> quota_manager_proxy_;
  scoped_refptr<CacheStorageManager> cache_manager_;

  CacheStorageCacheHandle callback_cache_handle_;
  int callback_bool_;
  CacheStorageError callback_error_;
  blink::mojom::FetchAPIResponsePtr callback_cache_handle_response_;
  std::unique_ptr<storage::BlobDataHandle> callback_data_handle_;
  CacheStorageIndex callback_cache_index_;

  const url::Origin origin1_;
  const url::Origin origin2_;

  int64_t callback_usage_;
  std::vector<CacheStorageUsageInfo> callback_all_origins_usage_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CacheStorageManagerTest);
};

class CacheStorageManagerMemoryOnlyTest : public CacheStorageManagerTest {
 public:
  bool MemoryOnly() override { return true; }
};

class CacheStorageManagerTestP : public CacheStorageManagerTest,
                                 public testing::WithParamInterface<bool> {
 public:
  bool MemoryOnly() override { return !GetParam(); }
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

TEST_P(CacheStorageManagerTestP, OpenExistingCache) {
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
  std::unique_ptr<storage::BlobDataHandle> data_handle =
      std::move(callback_data_handle_);

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
  ServiceWorkerFetchRequest request;
  request.url = GURL("http://example.com/foo");
  EXPECT_TRUE(
      CachePutWithStatusCode(callback_cache_handle_.value(), request, 200));
  EXPECT_TRUE(Open(origin1_, "bar"));
  EXPECT_TRUE(
      CachePutWithStatusCode(callback_cache_handle_.value(), request, 201));

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

TEST_P(CacheStorageManagerTestP, Chinese) {
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
  cache_manager_ = CacheStorageManager::Create(cache_manager_.get());
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
  cache_manager_ = CacheStorageManager::Create(cache_manager_.get());
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

// With a persistent cache if the client drops its reference to a
// CacheStorageCache it should be deleted.
TEST_F(CacheStorageManagerTest, DropReference) {
  EXPECT_TRUE(Open(origin1_, "foo"));
  base::WeakPtr<CacheStorageCache> cache =
      callback_cache_handle_.value()->AsWeakPtr();
  // Run a cache operation to ensure that the cache has finished initializing so
  // that when the handle is dropped it can close immediately.
  EXPECT_FALSE(CacheMatch(callback_cache_handle_.value(),
                          GURL("http://example.com/foo")));

  callback_cache_handle_ = CacheStorageCacheHandle();
  EXPECT_FALSE(cache);
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

  CacheStorage* cache_storage = CacheStorageForOrigin(origin1_);
  auto cache_handle = cache_storage->GetLoadedCache(kCacheName);
  CacheStorageCache* cache = cache_handle.value();
  base::FilePath index_path = cache->path().AppendASCII("index");
  cache_handle = CacheStorageCacheHandle();

  DestroyStorageManager();

  // Truncate the SimpleCache index to force an error when next opened.
  ASSERT_FALSE(index_path.empty());
  ASSERT_EQ(5, base::WriteFile(index_path, "hello", 5));

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
  base::FilePath storage_dir = original_handle.value()->path().DirName();
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
  cache_manager_ = CacheStorageManager::Create(cache_manager_.get());
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
  const FetchResponseType response_type = FetchResponseType::kCORS;

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

  // GenerateNewKeyForTest isn't thread safe so
  base::RunLoop().RunUntilIdle();
  CacheStorage::GenerateNewKeyForTesting();

  // Create a new CacheStorageManager that hasn't yet loaded the origin.
  CreateStorageManager();
  quota_manager_proxy_->SimulateQuotaManagerDestroyed();
  cache_manager_ = CacheStorageManager::Create(cache_manager_.get());

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
  base::WeakPtr<CacheStorageCache> cache =
      callback_cache_handle_.value()->AsWeakPtr();
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

TEST_P(CacheStorageManagerTestP, OpenRunsSerially) {
  EXPECT_FALSE(Delete(origin1_, "tmp"));  // Init storage.
  CacheStorage* cache_storage = CacheStorageForOrigin(origin1_);
  cache_storage->StartAsyncOperationForTesting();

  base::RunLoop open_loop;
  cache_manager_->OpenCache(
      origin1_, CacheStorageOwner::kCacheAPI, "foo",
      base::BindOnce(&CacheStorageManagerTest::CacheAndErrorCallback,
                     base::Unretained(this), base::Unretained(&open_loop)));

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(callback_cache_handle_.value());

  cache_storage->CompleteAsyncOperationForTesting();
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

  std::vector<CacheStorageUsageInfo> usage = GetAllOriginsUsage();
  EXPECT_EQ(2ULL, usage.size());

  int origin1_index = url::Origin::Create(usage[0].origin) == origin1_ ? 0 : 1;
  int origin2_index = url::Origin::Create(usage[1].origin) == origin2_ ? 1 : 0;
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

  std::vector<CacheStorageUsageInfo> usage_cache =
      GetAllOriginsUsage(CacheStorageOwner::kCacheAPI);
  EXPECT_EQ(1ULL, usage_cache.size());
  std::vector<CacheStorageUsageInfo> usage_bgf =
      GetAllOriginsUsage(CacheStorageOwner::kBackgroundFetch);
  EXPECT_EQ(2ULL, usage_bgf.size());

  int origin1_index =
      url::Origin::Create(usage_bgf[0].origin) == origin1_ ? 0 : 1;
  int origin2_index =
      url::Origin::Create(usage_bgf[1].origin) == origin2_ ? 1 : 0;
  EXPECT_NE(origin1_index, origin2_index);

  EXPECT_EQ(usage_cache[0].origin, origin1_.GetURL());
  EXPECT_EQ(usage_bgf[origin1_index].origin, origin1_.GetURL());
  EXPECT_EQ(usage_bgf[origin2_index].origin, origin2_.GetURL());

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

TEST_F(CacheStorageManagerTest, GetAllOriginsUsageWithOldIndex) {
  // Write a single value (V1) to the cache.
  const GURL kFooURL = origin1_.GetURL().Resolve("foo");
  const std::string kCacheName = "foo";
  EXPECT_TRUE(Open(origin1_, kCacheName));
  CacheStorageCacheHandle original_handle = std::move(callback_cache_handle_);

  EXPECT_TRUE(CachePut(original_handle.value(), kFooURL));
  int64_t cache_size_v1 = Size(origin1_);
  base::FilePath storage_dir = original_handle.value()->path().DirName();
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
  cache_manager_ = CacheStorageManager::Create(cache_manager_.get());

  // Create a second value (V2) in the cache.
  EXPECT_TRUE(Open(origin1_, kCacheName));
  original_handle = std::move(callback_cache_handle_);
  const GURL kBarURL = origin1_.GetURL().Resolve("bar");
  EXPECT_TRUE(CachePut(original_handle.value(), kBarURL));
  original_handle = CacheStorageCacheHandle();

  std::vector<CacheStorageUsageInfo> usage = GetAllOriginsUsage();
  ASSERT_EQ(1ULL, usage.size());
  int64_t usage_before_close = usage[0].total_size_bytes;
  EXPECT_GT(usage_before_close, 0);

  // Close the caches and cache manager.
  DestroyStorageManager();

  // Restore the index to the V1 state. Make the access/mod times of index file
  // older than the other directories in the store to trigger size
  // recalculation.
  EXPECT_TRUE(base::CopyFile(backup_index_path, index_path));
  base::Time t = base::Time::Now() - base::TimeDelta::FromHours(1);
  EXPECT_TRUE(base::TouchFile(index_path, t, t));
  EXPECT_FALSE(IsIndexFileCurrent(storage_dir));

  CreateStorageManager();
  usage = GetAllOriginsUsage();
  ASSERT_EQ(1ULL, usage.size());

  EXPECT_EQ(usage_before_close, usage[0].total_size_bytes);

  EXPECT_FALSE(usage[0].last_modified.is_null());
}

TEST_F(CacheStorageManagerTest, GetOriginSizeWithOldIndex) {
  // Write a single value (V1) to the cache.
  const GURL kFooURL = origin1_.GetURL().Resolve("foo");
  const std::string kCacheName = "foo";
  EXPECT_TRUE(Open(origin1_, kCacheName));
  CacheStorageCacheHandle original_handle = std::move(callback_cache_handle_);

  EXPECT_TRUE(CachePut(original_handle.value(), kFooURL));
  int64_t cache_size_v1 = Size(origin1_);
  base::FilePath storage_dir = original_handle.value()->path().DirName();
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
  cache_manager_ = CacheStorageManager::Create(cache_manager_.get());

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

TEST_P(CacheStorageManagerTestP, GetSizeThenCloseAllCaches) {
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

TEST_P(CacheStorageManagerTestP, GetSizeThenCloseAllCachesTwoOwners) {
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

TEST_P(CacheStorageManagerTestP, GetSizeThenCloseAllCachesAfterDelete) {
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
  base::FilePath origin_path = CacheStorageManager::ConstructOriginPath(
      cache_manager_->root_path(), origin1_, CacheStorageOwner::kCacheAPI);
  base::FilePath unreferenced_path = origin_path.AppendASCII("bar");
  EXPECT_TRUE(CreateDirectory(unreferenced_path));
  EXPECT_TRUE(base::DirectoryExists(unreferenced_path));

  // Create a new StorageManager so that the next time the cache is opened
  // the unreferenced directory can be deleted.
  quota_manager_proxy_->SimulateQuotaManagerDestroyed();
  cache_manager_ = CacheStorageManager::Create(cache_manager_.get());

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

TEST_P(CacheStorageManagerTestP, SizeStorageAccessed) {
  EXPECT_EQ(0, Size(origin1_));
  // Size is not part of the web API and should not notify the quota manager of
  // an access.
  EXPECT_EQ(0, quota_manager_proxy_->notify_storage_accessed_count());
}

TEST_P(CacheStorageManagerTestP, SizeThenCloseStorageAccessed) {
  EXPECT_EQ(0, GetSizeThenCloseAllCaches(origin1_));
  // GetSizeThenCloseAllCaches is not part of the web API and should not notify
  // the quota manager of an access.
  EXPECT_EQ(0, quota_manager_proxy_->notify_storage_accessed_count());
}

TEST_P(CacheStorageManagerTestP, NotifyCacheListChanged_Created) {
  TestCacheStorageObserver observer;
  cache_manager_->AddObserver(&observer);

  EXPECT_EQ(0, observer.notify_list_changed_count);
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_EQ(1, observer.notify_list_changed_count);
  EXPECT_TRUE(
      CachePut(callback_cache_handle_.value(), GURL("http://example.com/foo")));
  EXPECT_EQ(1, observer.notify_list_changed_count);
}

TEST_P(CacheStorageManagerTestP, NotifyCacheListChanged_Deleted) {
  TestCacheStorageObserver observer;
  cache_manager_->AddObserver(&observer);

  EXPECT_EQ(0, observer.notify_list_changed_count);
  EXPECT_FALSE(Delete(origin1_, "foo"));
  EXPECT_EQ(0, observer.notify_list_changed_count);
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_EQ(1, observer.notify_list_changed_count);
  EXPECT_TRUE(Delete(origin1_, "foo"));
  EXPECT_EQ(2, observer.notify_list_changed_count);
}

TEST_P(CacheStorageManagerTestP, NotifyCacheListChanged_DeletedThenCreated) {
  TestCacheStorageObserver observer;
  cache_manager_->AddObserver(&observer);

  EXPECT_EQ(0, observer.notify_list_changed_count);
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_EQ(1, observer.notify_list_changed_count);
  EXPECT_TRUE(Delete(origin1_, "foo"));
  EXPECT_EQ(2, observer.notify_list_changed_count);
  EXPECT_TRUE(Open(origin2_, "foo2"));
  EXPECT_EQ(3, observer.notify_list_changed_count);
}

TEST_P(CacheStorageManagerTestP, NotifyCacheContentChanged_PutEntry) {
  TestCacheStorageObserver observer;
  cache_manager_->AddObserver(&observer);

  EXPECT_EQ(0, observer.notify_content_changed_count);
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_EQ(0, observer.notify_content_changed_count);
  EXPECT_TRUE(
      CachePut(callback_cache_handle_.value(), GURL("http://example.com/foo")));
  EXPECT_EQ(1, observer.notify_content_changed_count);
  EXPECT_TRUE(CachePut(callback_cache_handle_.value(),
                       GURL("http://example.com/foo1")));
  EXPECT_TRUE(CachePut(callback_cache_handle_.value(),
                       GURL("http://example.com/foo2")));
  EXPECT_EQ(3, observer.notify_content_changed_count);
}

TEST_P(CacheStorageManagerTestP, NotifyCacheContentChanged_DeleteEntry) {
  TestCacheStorageObserver observer;
  cache_manager_->AddObserver(&observer);

  EXPECT_EQ(0, observer.notify_content_changed_count);
  EXPECT_FALSE(Delete(origin1_, "foo"));
  EXPECT_EQ(0, observer.notify_content_changed_count);
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_EQ(0, observer.notify_content_changed_count);
  EXPECT_TRUE(
      CachePut(callback_cache_handle_.value(), GURL("http://example.com/foo")));
  EXPECT_EQ(1, observer.notify_content_changed_count);
  EXPECT_TRUE(CacheDelete(callback_cache_handle_.value(),
                          GURL("http://example.com/foo")));
  EXPECT_EQ(2, observer.notify_content_changed_count);
  EXPECT_FALSE(CacheDelete(callback_cache_handle_.value(),
                           GURL("http://example.com/foo")));
  EXPECT_EQ(2, observer.notify_content_changed_count);
}

TEST_P(CacheStorageManagerTestP, NotifyCacheContentChanged_DeleteThenPutEntry) {
  TestCacheStorageObserver observer;
  cache_manager_->AddObserver(&observer);

  EXPECT_EQ(0, observer.notify_content_changed_count);
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_EQ(0, observer.notify_content_changed_count);
  EXPECT_TRUE(
      CachePut(callback_cache_handle_.value(), GURL("http://example.com/foo")));
  EXPECT_EQ(1, observer.notify_content_changed_count);
  EXPECT_TRUE(CacheDelete(callback_cache_handle_.value(),
                          GURL("http://example.com/foo")));
  EXPECT_EQ(2, observer.notify_content_changed_count);
  EXPECT_TRUE(
      CachePut(callback_cache_handle_.value(), GURL("http://example.com/foo")));
  EXPECT_EQ(3, observer.notify_content_changed_count);
  EXPECT_TRUE(CacheDelete(callback_cache_handle_.value(),
                          GURL("http://example.com/foo")));
  EXPECT_EQ(4, observer.notify_content_changed_count);
}

TEST_P(CacheStorageManagerTestP, StorageMatch_IgnoreSearch) {
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(CachePut(callback_cache_handle_.value(),
                       GURL("http://example.com/foo?bar")));

  EXPECT_FALSE(StorageMatch(origin1_, "foo", GURL("http://example.com/foo")));

  blink::mojom::QueryParamsPtr match_params = blink::mojom::QueryParams::New();
  match_params->ignore_search = true;
  EXPECT_TRUE(StorageMatch(origin1_, "foo", GURL("http://example.com/foo"),
                           std::move(match_params)));
}

TEST_P(CacheStorageManagerTestP, StorageMatch_IgnoreMethod) {
  GURL url = GURL("http://example.com/foo");
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(CachePut(callback_cache_handle_.value(), url));

  ServiceWorkerFetchRequest post_request;
  post_request.url = url;
  post_request.method = "POST";
  EXPECT_FALSE(StorageMatchWithRequest(origin1_, "foo", post_request));

  blink::mojom::QueryParamsPtr match_params = blink::mojom::QueryParams::New();
  match_params->ignore_method = true;
  EXPECT_TRUE(StorageMatchWithRequest(origin1_, "foo", post_request,
                                      std::move(match_params)));
}

TEST_P(CacheStorageManagerTestP, StorageMatch_IgnoreVary) {
  GURL url = GURL("http://example.com/foo");
  EXPECT_TRUE(Open(origin1_, "foo"));

  ServiceWorkerFetchRequest request;
  request.url = url;
  request.headers["vary_foo"] = "foo";

  ResponseHeaderMap response_headers;
  response_headers["vary"] = "vary_foo";

  EXPECT_TRUE(CachePutWithRequestAndHeaders(
      callback_cache_handle_.value(), request, std::move(response_headers)));
  EXPECT_TRUE(StorageMatchWithRequest(origin1_, "foo", request));

  request.headers["vary_foo"] = "bar";
  EXPECT_FALSE(StorageMatchWithRequest(origin1_, "foo", request));

  blink::mojom::QueryParamsPtr match_params = blink::mojom::QueryParams::New();
  match_params->ignore_vary = true;
  EXPECT_TRUE(StorageMatchWithRequest(origin1_, "foo", request,
                                      std::move(match_params)));
}

TEST_P(CacheStorageManagerTestP, StorageMatchAll_IgnoreSearch) {
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(CachePut(callback_cache_handle_.value(),
                       GURL("http://example.com/foo?bar")));

  EXPECT_FALSE(StorageMatchAll(origin1_, GURL("http://example.com/foo")));

  blink::mojom::QueryParamsPtr match_params = blink::mojom::QueryParams::New();
  match_params->ignore_search = true;
  EXPECT_TRUE(StorageMatchAll(origin1_, GURL("http://example.com/foo"),
                              std::move(match_params)));
}

TEST_P(CacheStorageManagerTestP, StorageMatchAll_IgnoreMethod) {
  GURL url = GURL("http://example.com/foo");
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(CachePut(callback_cache_handle_.value(), url));

  ServiceWorkerFetchRequest post_request;
  post_request.url = url;
  post_request.method = "POST";
  EXPECT_FALSE(StorageMatchAllWithRequest(origin1_, post_request));

  blink::mojom::QueryParamsPtr match_params = blink::mojom::QueryParams::New();
  match_params->ignore_method = true;
  EXPECT_TRUE(StorageMatchAllWithRequest(origin1_, post_request,
                                         std::move(match_params)));
}

TEST_P(CacheStorageManagerTestP, StorageMatchAll_IgnoreVary) {
  GURL url = GURL("http://example.com/foo");
  EXPECT_TRUE(Open(origin1_, "foo"));

  ServiceWorkerFetchRequest request;
  request.url = url;
  request.headers["vary_foo"] = "foo";

  ResponseHeaderMap response_headers;
  response_headers["vary"] = "vary_foo";

  EXPECT_TRUE(CachePutWithRequestAndHeaders(callback_cache_handle_.value(),
                                            request, response_headers));
  EXPECT_TRUE(StorageMatchAllWithRequest(origin1_, request));

  request.headers["vary_foo"] = "bar";
  EXPECT_FALSE(StorageMatchAllWithRequest(origin1_, request));

  blink::mojom::QueryParamsPtr match_params = blink::mojom::QueryParams::New();
  match_params->ignore_vary = true;
  EXPECT_TRUE(
      StorageMatchAllWithRequest(origin1_, request, std::move(match_params)));
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

class CacheStorageQuotaClientTest : public CacheStorageManagerTest {
 protected:
  CacheStorageQuotaClientTest() {}

  void SetUp() override {
    CacheStorageManagerTest::SetUp();
    quota_client_.reset(new CacheStorageQuotaClient(
        cache_manager_->AsWeakPtr(), CacheStorageOwner::kCacheAPI));
  }

  void QuotaUsageCallback(base::RunLoop* run_loop, int64_t usage) {
    callback_quota_usage_ = usage;
    run_loop->Quit();
  }

  void OriginsCallback(base::RunLoop* run_loop,
                       const std::set<url::Origin>& origins) {
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

  bool QuotaDoesSupport(StorageType type) {
    return quota_client_->DoesSupport(type);
  }

  std::unique_ptr<CacheStorageQuotaClient> quota_client_;

  blink::mojom::QuotaStatusCode callback_status_;
  int64_t callback_quota_usage_ = 0;
  std::set<url::Origin> callback_origins_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CacheStorageQuotaClientTest);
};

class CacheStorageQuotaClientDiskOnlyTest : public CacheStorageQuotaClientTest {
 public:
  bool MemoryOnly() override { return false; }
};

class CacheStorageQuotaClientTestP : public CacheStorageQuotaClientTest,
                                     public testing::WithParamInterface<bool> {
  bool MemoryOnly() override { return !GetParam(); }
};

TEST_P(CacheStorageQuotaClientTestP, QuotaID) {
  EXPECT_EQ(storage::QuotaClient::kServiceWorkerCache, quota_client_->id());
}

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
  EXPECT_NE(
      callback_origins_.find(url::Origin::Create(GURL("http://example2.com"))),
      callback_origins_.end());
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
  callback_cache_handle_.value()->Close(run_loop.QuitClosure());
  run_loop.Run();

  // Create a new CacheStorageManager that hasn't yet loaded the origin.
  quota_manager_proxy_->SimulateQuotaManagerDestroyed();
  cache_manager_ = CacheStorageManager::Create(cache_manager_.get());
  quota_client_.reset(new CacheStorageQuotaClient(
      cache_manager_->AsWeakPtr(), CacheStorageOwner::kCacheAPI));

  EXPECT_TRUE(QuotaDeleteOriginData(origin1_));
  EXPECT_EQ(0, QuotaGetOriginUsage(origin1_));
}

TEST_P(CacheStorageQuotaClientTestP, QuotaDoesSupport) {
  EXPECT_TRUE(QuotaDoesSupport(StorageType::kTemporary));
  EXPECT_FALSE(QuotaDoesSupport(StorageType::kPersistent));
  EXPECT_FALSE(QuotaDoesSupport(StorageType::kSyncable));
  EXPECT_FALSE(QuotaDoesSupport(StorageType::kQuotaNotManaged));
  EXPECT_FALSE(QuotaDoesSupport(StorageType::kUnknown));
}

INSTANTIATE_TEST_CASE_P(CacheStorageManagerTests,
                        CacheStorageManagerTestP,
                        ::testing::Values(false, true));

INSTANTIATE_TEST_CASE_P(CacheStorageQuotaClientTests,
                        CacheStorageQuotaClientTestP,
                        ::testing::Values(false, true));

}  // namespace cache_storage_manager_unittest
}  // namespace content
