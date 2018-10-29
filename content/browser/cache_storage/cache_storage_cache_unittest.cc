// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cache_storage/cache_storage_cache.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <set>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/cache_storage/cache_storage_cache_handle.h"
#include "content/browser/cache_storage/cache_storage_histogram_utils.h"
#include "content/browser/cache_storage/cache_storage_manager.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "content/public/common/referrer.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "crypto/symmetric_key.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"
#include "net/base/test_completion_callback.h"
#include "net/disk_cache/disk_cache.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_data_snapshot.h"
#include "storage/browser/blob/blob_impl.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/blob/blob_url_request_job_factory.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/mock_quota_manager_proxy.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "storage/common/blob_storage/blob_handle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

using blink::mojom::CacheStorageError;
using blink::mojom::CacheStorageVerboseErrorPtr;
using storage::BlobDataItem;

namespace content {
namespace cache_storage_cache_unittest {

const char kTestData[] = "Hello World";
// TODO(crbug.com/889590): Use helper for url::Origin creation from string.
const url::Origin kOrigin = url::Origin::Create(GURL("http://example.com"));
const char kCacheName[] = "test_cache";
const GURL kBodyUrl("http://example.com/body.html");
const GURL kBodyUrlWithQuery("http://example.com/body.html?query=test");
const GURL kNoBodyUrl("http://example.com/no_body.html");
const ServiceWorkerHeaderMap kHeaders({{"a", "a"}, {"b", "b"}});

// Returns a BlobProtocolHandler that uses |blob_storage_context|. Caller owns
// the memory.
std::unique_ptr<storage::BlobProtocolHandler> CreateMockBlobProtocolHandler(
    storage::BlobStorageContext* blob_storage_context) {
  return base::WrapUnique(
      new storage::BlobProtocolHandler(blob_storage_context));
}

void SizeCallback(base::RunLoop* run_loop,
                  bool* callback_called,
                  int64_t* out_size,
                  int64_t size) {
  *callback_called = true;
  *out_size = size;
  if (run_loop)
    run_loop->Quit();
}

// A disk_cache::Backend wrapper that can delay operations.
class DelayableBackend : public disk_cache::Backend {
 public:
  explicit DelayableBackend(std::unique_ptr<disk_cache::Backend> backend)
      : backend_(std::move(backend)), delay_open_entry_(false) {}

  // disk_cache::Backend overrides
  net::CacheType GetCacheType() const override {
    return backend_->GetCacheType();
  }
  int32_t GetEntryCount() const override { return backend_->GetEntryCount(); }
  net::Error OpenEntry(const std::string& key,
                       net::RequestPriority request_priority,
                       disk_cache::Entry** entry,
                       CompletionOnceCallback callback) override {
    if (delay_open_entry_ && open_entry_callback_.is_null()) {
      open_entry_callback_ = base::BindOnce(
          &DelayableBackend::OpenEntryDelayedImpl, base::Unretained(this), key,
          base::Unretained(entry), std::move(callback));
      return net::ERR_IO_PENDING;
    }
    return backend_->OpenEntry(key, request_priority, entry,
                               std::move(callback));
  }

  net::Error CreateEntry(const std::string& key,
                         net::RequestPriority request_priority,
                         disk_cache::Entry** entry,
                         CompletionOnceCallback callback) override {
    return backend_->CreateEntry(key, request_priority, entry,
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

  size_t DumpMemoryStats(
      base::trace_event::ProcessMemoryDump* pmd,
      const std::string& parent_absolute_name) const override {
    NOTREACHED();
    return 0u;
  }

  // Call to continue a delayed call to OpenEntry.
  bool OpenEntryContinue() {
    if (open_entry_callback_.is_null())
      return false;
    std::move(open_entry_callback_).Run();
    return true;
  }

  void set_delay_open_entry(bool value) { delay_open_entry_ = value; }

 private:
  void OpenEntryDelayedImpl(const std::string& key,
                            disk_cache::Entry** entry,
                            CompletionOnceCallback callback) {
    auto copyable_callback =
        base::AdaptCallbackForRepeating(std::move(callback));
    int rv = backend_->OpenEntry(key, net::HIGHEST, entry, copyable_callback);
    if (rv != net::ERR_IO_PENDING)
      copyable_callback.Run(rv);
  }

  std::unique_ptr<disk_cache::Backend> backend_;
  bool delay_open_entry_;
  base::OnceClosure open_entry_callback_;
};

class DataPipeDrainerClient : public mojo::DataPipeDrainer::Client {
 public:
  DataPipeDrainerClient(std::string* output) : output_(output) {}
  void Run() { run_loop_.Run(); }

  void OnDataAvailable(const void* data, size_t num_bytes) override {
    output_->append(reinterpret_cast<const char*>(data), num_bytes);
  }
  void OnDataComplete() override { run_loop_.Quit(); }

 private:
  base::RunLoop run_loop_;
  std::string* output_;
};

std::string CopyBody(blink::mojom::Blob* actual_blob) {
  std::string output;
  mojo::DataPipe pipe;
  actual_blob->ReadAll(std::move(pipe.producer_handle), nullptr);
  DataPipeDrainerClient client(&output);
  mojo::DataPipeDrainer drainer(&client, std::move(pipe.consumer_handle));
  client.Run();
  return output;
}

std::string CopySideData(blink::mojom::Blob* actual_blob) {
  std::string output;
  base::RunLoop loop;
  actual_blob->ReadSideData(base::BindLambdaForTesting(
      [&](const base::Optional<std::vector<uint8_t>>& data) {
        ASSERT_TRUE(data);
        output.append(data->begin(), data->end());
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

bool ResponseBodiesEqual(const std::string& expected_body,
                         blink::mojom::Blob* actual_blob) {
  std::string actual_body = CopyBody(actual_blob);
  return expected_body == actual_body;
}

bool ResponseSideDataEqual(const std::string& expected_side_data,
                           blink::mojom::Blob* actual_blob) {
  std::string actual_body = CopySideData(actual_blob);
  return expected_side_data == actual_body;
}

blink::mojom::FetchAPIResponsePtr SetCacheName(
    blink::mojom::FetchAPIResponsePtr response) {
  response->is_in_cache_storage = true;
  response->cache_storage_cache_name = kCacheName;
  return response;
}

std::unique_ptr<crypto::SymmetricKey> CreateTestPaddingKey() {
  return crypto::SymmetricKey::Import(crypto::SymmetricKey::HMAC_SHA1,
                                      "abc123");
}

void OnBadMessage(std::string* result) {
  *result = "CSDH_UNEXPECTED_OPERATION";
}

// A CacheStorageCache that can optionally delay during backend creation.
class TestCacheStorageCache : public CacheStorageCache {
 public:
  TestCacheStorageCache(
      const url::Origin& origin,
      const std::string& cache_name,
      const base::FilePath& path,
      CacheStorage* cache_storage,
      const scoped_refptr<net::URLRequestContextGetter>& request_context_getter,
      const scoped_refptr<storage::QuotaManagerProxy>& quota_manager_proxy,
      base::WeakPtr<storage::BlobStorageContext> blob_context)
      : CacheStorageCache(origin,
                          CacheStorageOwner::kCacheAPI,
                          cache_name,
                          path,
                          cache_storage,
                          request_context_getter,
                          quota_manager_proxy,
                          blob_context,
                          0 /* cache_size */,
                          0 /* cache_padding */,
                          CreateTestPaddingKey()),
        delay_backend_creation_(false) {}

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

  void Init() { InitBackend(); }

 private:
  CacheStorageCacheHandle CreateCacheHandle() override {
    // Returns an empty handle. There is no need for CacheStorage and its
    // handles in these tests.
    return CacheStorageCacheHandle();
  }

  bool delay_backend_creation_;
  ErrorCallback backend_creation_callback_;

  DISALLOW_COPY_AND_ASSIGN(TestCacheStorageCache);
};

class CacheStorageCacheTest : public testing::Test {
 public:
  CacheStorageCacheTest()
      : browser_thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP) {}

  void SetUp() override {
    ChromeBlobStorageContext* blob_storage_context =
        ChromeBlobStorageContext::GetFor(&browser_context_);
    // Wait for chrome_blob_storage_context to finish initializing.
    base::RunLoop().RunUntilIdle();
    blob_storage_context_ = blob_storage_context->context();

    const bool is_incognito = MemoryOnly();
    base::FilePath temp_dir_path;
    if (!is_incognito) {
      ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
      temp_dir_path = temp_dir_.GetPath();
    }

    quota_policy_ = new MockSpecialStoragePolicy;
    mock_quota_manager_ = new MockQuotaManager(
        is_incognito, temp_dir_path, base::ThreadTaskRunnerHandle::Get().get(),
        quota_policy_.get());
    mock_quota_manager_->SetQuota(
        kOrigin, blink::mojom::StorageType::kTemporary, 1024 * 1024 * 100);

    quota_manager_proxy_ = new MockQuotaManagerProxy(
        mock_quota_manager_.get(), base::ThreadTaskRunnerHandle::Get().get());

    url_request_job_factory_.reset(new net::URLRequestJobFactoryImpl);
    url_request_job_factory_->SetProtocolHandler(
        "blob", CreateMockBlobProtocolHandler(blob_storage_context->context()));

    net::URLRequestContext* url_request_context =
        BrowserContext::GetDefaultStoragePartition(&browser_context_)->
            GetURLRequestContext()->GetURLRequestContext();

    url_request_context->set_job_factory(url_request_job_factory_.get());

    CreateRequests(blob_storage_context);

    response_time_ = base::Time::Now();
    for (int i = 0; i < 100; ++i)
      expected_blob_data_ += kTestData;
    blob_handle_ = BuildBlobHandle("blob-id:myblob", expected_blob_data_);
    storage::BlobImpl::Create(
        std::make_unique<storage::BlobDataHandle>(*blob_handle_),
        MakeRequest(&blob_ptr_));

    cache_ = std::make_unique<TestCacheStorageCache>(
        kOrigin, kCacheName, temp_dir_path, nullptr /* CacheStorage */,
        BrowserContext::GetDefaultStoragePartition(&browser_context_)
            ->GetURLRequestContext(),
        quota_manager_proxy_, blob_storage_context->context()->AsWeakPtr());
    cache_->Init();
  }

  void TearDown() override {
    quota_manager_proxy_->SimulateQuotaManagerDestroyed();
    disk_cache::FlushCacheThreadForTesting();
    content::RunAllTasksUntilIdle();
  }

  void CreateRequests(ChromeBlobStorageContext* blob_storage_context) {
    body_request_ =
        ServiceWorkerFetchRequest(kBodyUrl, "GET", kHeaders, Referrer(), false);
    body_request_with_query_ = ServiceWorkerFetchRequest(
        kBodyUrlWithQuery, "GET", kHeaders, Referrer(), false);
    no_body_request_ = ServiceWorkerFetchRequest(kNoBodyUrl, "GET", kHeaders,
                                                 Referrer(), false);
    body_head_request_ = ServiceWorkerFetchRequest(kBodyUrl, "HEAD", kHeaders,
                                                   Referrer(), false);
  }

  blink::mojom::FetchAPIResponsePtr CreateBlobBodyResponse() {
    auto blob = blink::mojom::SerializedBlob::New();
    blob->uuid = blob_handle_->uuid();
    blob->size = expected_blob_data_.size();
    // Use cloned blob pointer for all responses with blob body.
    blob_ptr_->Clone(mojo::MakeRequest(&blob->blob));

    blink::mojom::FetchAPIResponsePtr response = CreateNoBodyResponse();
    response->url_list = {kBodyUrl};
    response->blob = std::move(blob);
    return response;
  }

  blink::mojom::FetchAPIResponsePtr CreateBlobBodyResponseWithQuery() {
    blink::mojom::FetchAPIResponsePtr response = CreateBlobBodyResponse();
    response->url_list = {kBodyUrlWithQuery};
    response->cors_exposed_header_names = {"a"};
    return response;
  }

  blink::mojom::FetchAPIResponsePtr CreateNoBodyResponse() {
    return blink::mojom::FetchAPIResponse::New(
        std::vector<GURL>({kNoBodyUrl}), 200, "OK",
        network::mojom::FetchResponseType::kDefault,
        base::flat_map<std::string, std::string>(kHeaders.cbegin(),
                                                 kHeaders.cend()),
        nullptr /* blob */, blink::mojom::ServiceWorkerResponseError::kUnknown,
        response_time_, std::string() /* cache_storage_cache_name */,
        std::vector<std::string>() /* cors_exposed_header_names */,
        false /* is_in_cache_storage */, nullptr /* side_data_blob */);
  }

  std::unique_ptr<storage::BlobDataHandle> BuildBlobHandle(
      const std::string& uuid,
      const std::string& data) {
    std::unique_ptr<storage::BlobDataBuilder> builder =
        std::make_unique<storage::BlobDataBuilder>(uuid);
    builder->AppendData(data);
    return blob_storage_context_->AddFinishedBlob(std::move(builder));
  }

  void CopySideDataToResponse(storage::BlobDataHandle* side_data_blob_handle,
                              blink::mojom::FetchAPIResponse* response) {
    response->side_data_blob = blink::mojom::SerializedBlob::New();
    response->side_data_blob->uuid = side_data_blob_handle->uuid();
    response->side_data_blob->size = side_data_blob_handle->size();
    storage::BlobImpl::Create(
        std::make_unique<storage::BlobDataHandle>(*side_data_blob_handle),
        MakeRequest(&response->side_data_blob->blob));
  }

  std::unique_ptr<ServiceWorkerFetchRequest> CopyFetchRequest(
      const ServiceWorkerFetchRequest& request) {
    return std::make_unique<ServiceWorkerFetchRequest>(
        request.url, request.method, request.headers, request.referrer,
        request.is_reload);
  }

  CacheStorageError BatchOperation(
      std::vector<blink::mojom::BatchOperationPtr> operations,
      bool fail_on_duplicates = true) {
    std::unique_ptr<base::RunLoop> loop(new base::RunLoop());

    cache_->BatchOperation(
        std::move(operations), fail_on_duplicates,
        base::BindOnce(&CacheStorageCacheTest::VerboseErrorTypeCallback,
                       base::Unretained(this), base::Unretained(loop.get())),
        base::BindOnce(&OnBadMessage, base::Unretained(&bad_message_reason_)));
    // TODO(jkarlin): These functions should use base::RunLoop().RunUntilIdle()
    // once the cache uses a passed in task runner instead of the CACHE thread.
    loop->Run();

    return callback_error_;
  }

  bool Put(const ServiceWorkerFetchRequest& request,
           blink::mojom::FetchAPIResponsePtr response) {
    blink::mojom::BatchOperationPtr operation =
        blink::mojom::BatchOperation::New();
    operation->operation_type = blink::mojom::OperationType::kPut;
    operation->request = request;
    operation->response = std::move(response);

    std::vector<blink::mojom::BatchOperationPtr> operations;
    operations.emplace_back(std::move(operation));
    CacheStorageError error = BatchOperation(std::move(operations));
    return error == CacheStorageError::kSuccess;
  }

  bool Match(const ServiceWorkerFetchRequest& request,
             blink::mojom::QueryParamsPtr match_params = nullptr) {
    std::unique_ptr<base::RunLoop> loop(new base::RunLoop());

    cache_->Match(
        CopyFetchRequest(request), std::move(match_params),
        base::BindOnce(&CacheStorageCacheTest::ResponseAndErrorCallback,
                       base::Unretained(this), base::Unretained(loop.get())));
    loop->Run();

    return callback_error_ == CacheStorageError::kSuccess;
  }

  bool MatchAll(const ServiceWorkerFetchRequest& request,
                blink::mojom::QueryParamsPtr match_params,
                std::vector<blink::mojom::FetchAPIResponsePtr>* responses) {
    base::RunLoop loop;
    cache_->MatchAll(
        CopyFetchRequest(request), std::move(match_params),
        base::BindOnce(&CacheStorageCacheTest::ResponsesAndErrorCallback,
                       base::Unretained(this), loop.QuitClosure(), responses));
    loop.Run();
    return callback_error_ == CacheStorageError::kSuccess;
  }

  bool GetAllMatchedEntries(
      std::vector<CacheStorageCache::CacheEntry>* cache_entries) {
    base::RunLoop loop;
    cache_->GetAllMatchedEntries(
        nullptr /* request */, nullptr /* options */,
        base::BindOnce(&CacheStorageCacheTest::CacheEntriesAndErrorCallback,
                       base::Unretained(this), loop.QuitClosure(),
                       cache_entries));
    loop.Run();
    return callback_error_ == CacheStorageError::kSuccess;
  }

  bool MatchAll(std::vector<blink::mojom::FetchAPIResponsePtr>* responses) {
    return MatchAll(ServiceWorkerFetchRequest(), nullptr, responses);
  }

  bool Delete(const ServiceWorkerFetchRequest& request,
              blink::mojom::QueryParamsPtr match_params = nullptr) {
    blink::mojom::BatchOperationPtr operation =
        blink::mojom::BatchOperation::New();
    operation->operation_type = blink::mojom::OperationType::kDelete;
    operation->request = request;
    operation->match_params = std::move(match_params);

    std::vector<blink::mojom::BatchOperationPtr> operations;
    operations.emplace_back(std::move(operation));
    CacheStorageError error = BatchOperation(std::move(operations));
    return error == CacheStorageError::kSuccess;
  }

  bool Keys(
      const ServiceWorkerFetchRequest& request = ServiceWorkerFetchRequest(),
      blink::mojom::QueryParamsPtr match_params = nullptr) {
    std::unique_ptr<base::RunLoop> loop(new base::RunLoop());

    cache_->Keys(
        CopyFetchRequest(request), std::move(match_params),
        base::BindOnce(&CacheStorageCacheTest::RequestsCallback,
                       base::Unretained(this), base::Unretained(loop.get())));
    loop->Run();

    return callback_error_ == CacheStorageError::kSuccess;
  }

  bool Close() {
    std::unique_ptr<base::RunLoop> loop(new base::RunLoop());

    cache_->Close(base::BindOnce(&CacheStorageCacheTest::CloseCallback,
                                 base::Unretained(this),
                                 base::Unretained(loop.get())));
    loop->Run();
    return callback_closed_;
  }

  bool WriteSideData(const GURL& url,
                     base::Time expected_response_time,
                     scoped_refptr<net::IOBuffer> buffer,
                     int buf_len) {
    base::RunLoop run_loop;
    cache_->WriteSideData(
        base::BindOnce(&CacheStorageCacheTest::ErrorTypeCallback,
                       base::Unretained(this), base::Unretained(&run_loop)),
        url, expected_response_time, buffer, buf_len);
    run_loop.Run();

    return callback_error_ == CacheStorageError::kSuccess;
  }

  int64_t Size() {
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
    return result;
  }

  int64_t GetSizeThenClose() {
    base::RunLoop run_loop;
    bool callback_called = false;
    int64_t result = 0;
    cache_->GetSizeThenClose(
        base::BindOnce(&SizeCallback, &run_loop, &callback_called, &result));
    run_loop.Run();
    EXPECT_TRUE(callback_called);
    return result;
  }

  void RequestsCallback(base::RunLoop* run_loop,
                        CacheStorageError error,
                        std::unique_ptr<CacheStorageCache::Requests> requests) {
    callback_error_ = error;
    callback_strings_.clear();
    if (requests) {
      for (size_t i = 0u; i < requests->size(); ++i)
        callback_strings_.push_back(requests->at(i).url.spec());
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
    callback_message_ = base::nullopt;
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
      std::vector<CacheStorageCache::CacheEntry>* cache_entries_out,
      CacheStorageError error,
      std::vector<CacheStorageCache::CacheEntry> cache_entries) {
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

  void SetMaxQuerySizeBytes(size_t max_bytes) {
    cache_->max_query_size_bytes_ = max_bytes;
  }

  size_t EstimatedResponseSizeWithoutBlob(
      const blink::mojom::FetchAPIResponse& response) {
    return CacheStorageCache::EstimatedResponseSizeWithoutBlob(response);
  }

 protected:
  base::ScopedTempDir temp_dir_;
  TestBrowserThreadBundle browser_thread_bundle_;
  TestBrowserContext browser_context_;
  std::unique_ptr<net::URLRequestJobFactoryImpl> url_request_job_factory_;
  scoped_refptr<MockSpecialStoragePolicy> quota_policy_;
  scoped_refptr<MockQuotaManager> mock_quota_manager_;
  scoped_refptr<MockQuotaManagerProxy> quota_manager_proxy_;
  storage::BlobStorageContext* blob_storage_context_;

  std::unique_ptr<TestCacheStorageCache> cache_;

  ServiceWorkerFetchRequest body_request_;
  ServiceWorkerFetchRequest body_request_with_query_;
  ServiceWorkerFetchRequest no_body_request_;
  ServiceWorkerFetchRequest body_head_request_;
  std::unique_ptr<storage::BlobDataHandle> blob_handle_;
  // Holds a Mojo connection to the BlobImpl containing |blob_handle_|.
  blink::mojom::BlobPtr blob_ptr_;
  base::Time response_time_;
  std::string expected_blob_data_;

  CacheStorageError callback_error_ = CacheStorageError::kSuccess;
  base::Optional<std::string> callback_message_ = base::nullopt;
  blink::mojom::FetchAPIResponsePtr callback_response_;
  std::vector<std::string> callback_strings_;
  std::string bad_message_reason_;
  bool callback_closed_ = false;
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
  operation1->request = body_request_;
  operation1->request.url = GURL("http://example.com/1");
  operation1->response = CreateBlobBodyResponse();
  operation1->response->url_list.push_back(GURL("http://example.com/1"));
  ServiceWorkerFetchRequest request1 = operation1->request;

  blink::mojom::BatchOperationPtr operation2 =
      blink::mojom::BatchOperation::New();
  operation2->operation_type = blink::mojom::OperationType::kPut;
  operation2->request = body_request_;
  operation2->request.url = GURL("http://example.com/2");
  operation2->response = CreateBlobBodyResponse();
  operation2->response->url_list.push_back(GURL("http://example.com/2"));
  ServiceWorkerFetchRequest request2 = operation2->request;

  blink::mojom::BatchOperationPtr operation3 =
      blink::mojom::BatchOperation::New();
  operation3->operation_type = blink::mojom::OperationType::kPut;
  operation3->request = body_request_;
  operation3->request.url = GURL("http://example.com/3");
  operation3->response = CreateBlobBodyResponse();
  operation3->response->url_list.push_back(GURL("http://example.com/3"));
  ServiceWorkerFetchRequest request3 = operation3->request;

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

  size_t max_size = no_body_request_.EstimatedStructSize() +
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
      body_request_.EstimatedStructSize() +
      EstimatedResponseSizeWithoutBlob(*callback_response_);
  size_t query_request_size =
      body_request_with_query_.EstimatedStructSize() +
      EstimatedResponseSizeWithoutBlob(*callback_response_);

  std::vector<blink::mojom::FetchAPIResponsePtr> responses;
  blink::mojom::QueryParamsPtr match_params = blink::mojom::QueryParams::New();

  // There is enough room for both requests and responses
  SetMaxQuerySizeBytes(body_request_size + query_request_size);
  EXPECT_TRUE(MatchAll(body_request_, match_params->Clone(), &responses));
  EXPECT_EQ(1u, responses.size());

  match_params->ignore_search = true;
  EXPECT_TRUE(MatchAll(body_request_, match_params->Clone(), &responses));
  EXPECT_EQ(2u, responses.size());

  // There is not enough room for both requests and responses
  SetMaxQuerySizeBytes(body_request_size);
  match_params->ignore_search = false;
  EXPECT_TRUE(MatchAll(body_request_, match_params->Clone(), &responses));
  EXPECT_EQ(1u, responses.size());

  match_params->ignore_search = true;
  EXPECT_FALSE(MatchAll(body_request_, match_params->Clone(), &responses));
  EXPECT_EQ(CacheStorageError::kErrorQueryTooLarge, callback_error_);
}

TEST_P(CacheStorageCacheTestP, KeysLimit) {
  EXPECT_TRUE(Put(no_body_request_, CreateNoBodyResponse()));
  EXPECT_TRUE(Put(body_request_, CreateBlobBodyResponse()));

  size_t max_size = no_body_request_.EstimatedStructSize() +
                    body_request_.EstimatedStructSize();
  SetMaxQuerySizeBytes(max_size);
  EXPECT_TRUE(Keys());

  SetMaxQuerySizeBytes(no_body_request_.EstimatedStructSize());
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
  no_body_response->url_list.push_back(GURL("http://example.com/foobar"));
  EXPECT_STRNE("http://example.com/foobar",
               no_body_request_.url.spec().c_str());
  EXPECT_TRUE(Put(no_body_request_, std::move(no_body_response)));
  EXPECT_TRUE(Match(no_body_request_));
  ASSERT_EQ(1u, callback_response_->url_list.size());
  EXPECT_STREQ("http://example.com/foobar",
               callback_response_->url_list[0].spec().c_str());
}

TEST_P(CacheStorageCacheTestP, ResponseURLEmpty) {
  blink::mojom::FetchAPIResponsePtr no_body_response = CreateNoBodyResponse();
  no_body_response->url_list.clear();
  EXPECT_STRNE("", no_body_request_.url.spec().c_str());
  EXPECT_TRUE(Put(no_body_request_, std::move(no_body_response)));
  EXPECT_TRUE(Match(no_body_request_));
  EXPECT_EQ(0u, callback_response_->url_list.size());
}

TEST_P(CacheStorageCacheTestP, PutBodyDropBlobRef) {
  blink::mojom::BatchOperationPtr operation =
      blink::mojom::BatchOperation::New();
  operation->operation_type = blink::mojom::OperationType::kPut;
  operation->request = body_request_;
  operation->response = CreateBlobBodyResponse();

  std::vector<blink::mojom::BatchOperationPtr> operations;
  operations.emplace_back(std::move(operation));
  std::unique_ptr<base::RunLoop> loop(new base::RunLoop());
  cache_->BatchOperation(
      std::move(operations), true /* fail_on_duplicate */,
      base::BindOnce(&CacheStorageCacheTestP::VerboseErrorTypeCallback,
                     base::Unretained(this), base::Unretained(loop.get())),
      CacheStorageCache::BadMessageCallback());
  // The handle should be held by the cache now so the deref here should be
  // okay.
  blob_handle_.reset();
  loop->Run();

  EXPECT_EQ(CacheStorageError::kSuccess, callback_error_);
}

TEST_P(CacheStorageCacheTestP, PutBadMessage) {
  base::HistogramTester histogram_tester;

  // Two unique puts that will collectively overflow unit64_t size of the
  // batch operation.
  blink::mojom::BatchOperationPtr operation1 =
      blink::mojom::BatchOperation::New(blink::mojom::OperationType::kPut,
                                        body_request_, CreateBlobBodyResponse(),
                                        nullptr /* match_params */);
  operation1->response->blob->size = std::numeric_limits<uint64_t>::max();
  blink::mojom::BatchOperationPtr operation2 =
      blink::mojom::BatchOperation::New(
          blink::mojom::OperationType::kPut, body_request_with_query_,
          CreateBlobBodyResponse(), nullptr /* match_params */);
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
  operation1->request = body_request_;
  operation1->response = CreateNoBodyResponse();

  blink::mojom::BatchOperationPtr operation2 =
      blink::mojom::BatchOperation::New();
  operation2->operation_type = blink::mojom::OperationType::kPut;
  operation2->request = body_request_;
  operation2->response = CreateBlobBodyResponse();

  std::vector<blink::mojom::BatchOperationPtr> operations;
  operations.push_back(std::move(operation1));
  operations.push_back(std::move(operation2));

  EXPECT_EQ(CacheStorageError::kErrorDuplicateOperation,
            BatchOperation(std::move(operations)));

  // A duplicate operation error should provide an informative message
  // containing the URL of the duplicate request.
  ASSERT_TRUE(callback_message_);
  EXPECT_NE(std::string::npos, callback_message_.value().find(kBodyUrl.spec()));

  // Neither operation should have completed.
  EXPECT_FALSE(Match(body_request_));
}

TEST_P(CacheStorageCacheTestP, PutReplaceInBatchWithDuplicateCheckingDisabled) {
  blink::mojom::BatchOperationPtr operation1 =
      blink::mojom::BatchOperation::New();
  operation1->operation_type = blink::mojom::OperationType::kPut;
  operation1->request = body_request_;
  operation1->response = CreateNoBodyResponse();

  blink::mojom::BatchOperationPtr operation2 =
      blink::mojom::BatchOperation::New();
  operation2->operation_type = blink::mojom::OperationType::kPut;
  operation2->request = body_request_;
  operation2->response = CreateBlobBodyResponse();

  std::vector<blink::mojom::BatchOperationPtr> operations;
  operations.push_back(std::move(operation1));
  operations.push_back(std::move(operation2));

  EXPECT_EQ(
      CacheStorageError::kSuccess,
      BatchOperation(std::move(operations), false /* fail_on_duplicates */));

  // Even when we don't fail on duplicates we should still provide an
  // informative message to the user that includes the duplicate URLs.
  ASSERT_TRUE(callback_message_);
  EXPECT_NE(std::string::npos, callback_message_.value().find(kBodyUrl.spec()));

  // |operation2| should win.
  EXPECT_TRUE(Match(body_request_));
  EXPECT_TRUE(callback_response_->blob);
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
  blink::mojom::BlobPtr blob(std::move(callback_response_->blob->blob));
  EXPECT_TRUE(ResponseBodiesEqual(expected_blob_data_, blob.get()));
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
  blink::mojom::BlobPtr blob(std::move(responses[0]->blob->blob));
  EXPECT_TRUE(ResponseBodiesEqual(expected_blob_data_, blob.get()));
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
  blink::mojom::BlobPtr blob(std::move(responses[1]->blob->blob));
  EXPECT_TRUE(ResponseBodiesEqual(expected_blob_data_, blob.get()));

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
  blink::mojom::QueryParamsPtr match_params = blink::mojom::QueryParams::New();
  match_params->ignore_search = true;
  EXPECT_TRUE(Match(body_request_, std::move(match_params)));
}

TEST_P(CacheStorageCacheTestP, Match_IgnoreMethod) {
  EXPECT_TRUE(Put(body_request_, CreateBlobBodyResponse()));

  ServiceWorkerFetchRequest post_request = body_request_;
  post_request.method = "POST";
  EXPECT_FALSE(Match(post_request));

  blink::mojom::QueryParamsPtr match_params = blink::mojom::QueryParams::New();
  match_params->ignore_method = true;
  EXPECT_TRUE(Match(post_request, std::move(match_params)));
}

TEST_P(CacheStorageCacheTestP, Match_IgnoreVary) {
  body_request_.headers["vary_foo"] = "foo";
  blink::mojom::FetchAPIResponsePtr body_response = CreateBlobBodyResponse();
  body_response->headers["vary"] = "vary_foo";
  EXPECT_TRUE(Put(body_request_, std::move(body_response)));
  EXPECT_TRUE(Match(body_request_));

  body_request_.headers["vary_foo"] = "bar";
  EXPECT_FALSE(Match(body_request_));

  blink::mojom::QueryParamsPtr match_params = blink::mojom::QueryParams::New();
  match_params->ignore_vary = true;
  EXPECT_TRUE(Match(body_request_, std::move(match_params)));
}

TEST_P(CacheStorageCacheTestP, GetAllMatchedEntries_RequestsIncluded) {
  EXPECT_TRUE(Put(body_request_, CreateBlobBodyResponse()));

  std::vector<CacheStorageCache::CacheEntry> cache_entries;
  EXPECT_TRUE(GetAllMatchedEntries(&cache_entries));

  ASSERT_EQ(1u, cache_entries.size());
  const auto& request = cache_entries[0].first;
  EXPECT_EQ(request->url, body_request_.url);
  EXPECT_EQ(request->headers, body_request_.headers);
  EXPECT_EQ(request->method, body_request_.method);

  auto& response = cache_entries[0].second;
  EXPECT_TRUE(ResponseMetadataEqual(*SetCacheName(CreateBlobBodyResponse()),
                                    *response));
  blink::mojom::BlobPtr blob(std::move(response->blob->blob));
  EXPECT_TRUE(ResponseBodiesEqual(expected_blob_data_, blob.get()));
}

TEST_P(CacheStorageCacheTestP, Keys_IgnoreSearch) {
  EXPECT_TRUE(Put(body_request_with_query_, CreateBlobBodyResponseWithQuery()));

  EXPECT_TRUE(Keys(body_request_));
  EXPECT_EQ(0u, callback_strings_.size());

  blink::mojom::QueryParamsPtr match_params = blink::mojom::QueryParams::New();
  match_params->ignore_search = true;
  EXPECT_TRUE(Keys(body_request_, std::move(match_params)));
  EXPECT_EQ(1u, callback_strings_.size());
}

TEST_P(CacheStorageCacheTestP, Keys_IgnoreMethod) {
  EXPECT_TRUE(Put(body_request_, CreateBlobBodyResponse()));

  ServiceWorkerFetchRequest post_request = body_request_;
  post_request.method = "POST";
  EXPECT_TRUE(Keys(post_request));
  EXPECT_EQ(0u, callback_strings_.size());

  blink::mojom::QueryParamsPtr match_params = blink::mojom::QueryParams::New();
  match_params->ignore_method = true;
  EXPECT_TRUE(Keys(post_request, std::move(match_params)));
  EXPECT_EQ(1u, callback_strings_.size());
}

TEST_P(CacheStorageCacheTestP, Keys_IgnoreVary) {
  body_request_.headers["vary_foo"] = "foo";
  blink::mojom::FetchAPIResponsePtr body_response = CreateBlobBodyResponse();
  body_response->headers["vary"] = "vary_foo";
  EXPECT_TRUE(Put(body_request_, std::move(body_response)));
  EXPECT_TRUE(Keys(body_request_));
  EXPECT_EQ(1u, callback_strings_.size());

  body_request_.headers["vary_foo"] = "bar";
  EXPECT_TRUE(Keys(body_request_));
  EXPECT_EQ(0u, callback_strings_.size());

  blink::mojom::QueryParamsPtr match_params = blink::mojom::QueryParams::New();
  match_params->ignore_vary = true;
  EXPECT_TRUE(Keys(body_request_, std::move(match_params)));
  EXPECT_EQ(1u, callback_strings_.size());
}

TEST_P(CacheStorageCacheTestP, Delete_IgnoreSearch) {
  EXPECT_TRUE(Put(body_request_with_query_, CreateBlobBodyResponseWithQuery()));

  EXPECT_FALSE(Delete(body_request_));
  blink::mojom::QueryParamsPtr match_params = blink::mojom::QueryParams::New();
  match_params->ignore_search = true;
  EXPECT_TRUE(Delete(body_request_, std::move(match_params)));
}

TEST_P(CacheStorageCacheTestP, Delete_IgnoreMethod) {
  EXPECT_TRUE(Put(body_request_, CreateBlobBodyResponse()));

  ServiceWorkerFetchRequest post_request = body_request_;
  post_request.method = "POST";
  EXPECT_FALSE(Delete(post_request));

  blink::mojom::QueryParamsPtr match_params = blink::mojom::QueryParams::New();
  match_params->ignore_method = true;
  EXPECT_TRUE(Delete(post_request, std::move(match_params)));
}

TEST_P(CacheStorageCacheTestP, Delete_IgnoreVary) {
  body_request_.headers["vary_foo"] = "foo";
  blink::mojom::FetchAPIResponsePtr body_response = CreateBlobBodyResponse();
  body_response->headers["vary"] = "vary_foo";
  EXPECT_TRUE(Put(body_request_, std::move(body_response)));

  body_request_.headers["vary_foo"] = "bar";
  EXPECT_FALSE(Delete(body_request_));

  blink::mojom::QueryParamsPtr match_params = blink::mojom::QueryParams::New();
  match_params->ignore_vary = true;
  EXPECT_TRUE(Delete(body_request_, std::move(match_params)));
}

TEST_P(CacheStorageCacheTestP, MatchAll_IgnoreMethod) {
  EXPECT_TRUE(Put(body_request_, CreateBlobBodyResponse()));

  ServiceWorkerFetchRequest post_request = body_request_;
  post_request.method = "POST";
  std::vector<blink::mojom::FetchAPIResponsePtr> responses;

  EXPECT_TRUE(MatchAll(post_request, nullptr, &responses));
  EXPECT_EQ(0u, responses.size());

  blink::mojom::QueryParamsPtr match_params = blink::mojom::QueryParams::New();
  match_params->ignore_method = true;
  EXPECT_TRUE(MatchAll(post_request, std::move(match_params), &responses));
  EXPECT_EQ(1u, responses.size());
}

TEST_P(CacheStorageCacheTestP, MatchAll_IgnoreVary) {
  body_request_.headers["vary_foo"] = "foo";
  blink::mojom::FetchAPIResponsePtr body_response = CreateBlobBodyResponse();
  body_response->headers["vary"] = "vary_foo";
  EXPECT_TRUE(Put(body_request_, std::move(body_response)));
  std::vector<blink::mojom::FetchAPIResponsePtr> responses;

  EXPECT_TRUE(MatchAll(body_request_, nullptr, &responses));
  EXPECT_EQ(1u, responses.size());
  body_request_.headers["vary_foo"] = "bar";

  EXPECT_TRUE(MatchAll(body_request_, nullptr, &responses));
  EXPECT_EQ(0u, responses.size());

  blink::mojom::QueryParamsPtr match_params = blink::mojom::QueryParams::New();
  match_params->ignore_vary = true;
  EXPECT_TRUE(MatchAll(body_request_, std::move(match_params), &responses));
  EXPECT_EQ(1u, responses.size());
}

TEST_P(CacheStorageCacheTestP, MatchAll_IgnoreSearch) {
  EXPECT_TRUE(Put(body_request_, CreateBlobBodyResponse()));
  EXPECT_TRUE(Put(body_request_with_query_, CreateBlobBodyResponseWithQuery()));
  EXPECT_TRUE(Put(no_body_request_, CreateNoBodyResponse()));

  std::vector<blink::mojom::FetchAPIResponsePtr> responses;
  blink::mojom::QueryParamsPtr match_params = blink::mojom::QueryParams::New();
  match_params->ignore_search = true;
  EXPECT_TRUE(MatchAll(body_request_, std::move(match_params), &responses));

  ASSERT_EQ(2u, responses.size());

  // Order of returned responses is not guaranteed.
  std::set<std::string> matched_set;
  for (const blink::mojom::FetchAPIResponsePtr& response : responses) {
    ASSERT_EQ(1u, response->url_list.size());
    if (response->url_list[0].spec() == kBodyUrlWithQuery.spec()) {
      EXPECT_TRUE(ResponseMetadataEqual(
          *SetCacheName(CreateBlobBodyResponseWithQuery()), *response));
      matched_set.insert(response->url_list[0].spec());
    } else if (response->url_list[0].spec() == kBodyUrl.spec()) {
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
  blink::mojom::QueryParamsPtr match_params = blink::mojom::QueryParams::New();
  match_params->ignore_search = true;
  EXPECT_TRUE(MatchAll(body_head_request_, match_params->Clone(), &responses));
  EXPECT_TRUE(responses.empty());

  match_params->ignore_method = true;
  EXPECT_TRUE(MatchAll(body_head_request_, match_params->Clone(), &responses));
  ASSERT_EQ(1u, responses.size());
  EXPECT_TRUE(ResponseMetadataEqual(*SetCacheName(CreateBlobBodyResponse()),
                                    *responses[0]));
  blink::mojom::BlobPtr blob(std::move(responses[0]->blob->blob));
  EXPECT_TRUE(ResponseBodiesEqual(expected_blob_data_, blob.get()));
}

TEST_P(CacheStorageCacheTestP, Vary) {
  body_request_.headers["vary_foo"] = "foo";
  blink::mojom::FetchAPIResponsePtr body_response = CreateBlobBodyResponse();
  body_response->headers["vary"] = "vary_foo";
  EXPECT_TRUE(Put(body_request_, std::move(body_response)));
  EXPECT_TRUE(Match(body_request_));

  body_request_.headers["vary_foo"] = "bar";
  EXPECT_FALSE(Match(body_request_));

  body_request_.headers.erase("vary_foo");
  EXPECT_FALSE(Match(body_request_));
}

TEST_P(CacheStorageCacheTestP, EmptyVary) {
  blink::mojom::FetchAPIResponsePtr body_response = CreateBlobBodyResponse();
  body_response->headers["vary"] = "";
  EXPECT_TRUE(Put(body_request_, std::move(body_response)));
  EXPECT_TRUE(Match(body_request_));

  body_request_.headers["zoo"] = "zoo";
  EXPECT_TRUE(Match(body_request_));
}

TEST_P(CacheStorageCacheTestP, NoVaryButDiffHeaders) {
  EXPECT_TRUE(Put(body_request_, CreateBlobBodyResponse()));
  EXPECT_TRUE(Match(body_request_));

  body_request_.headers["zoo"] = "zoo";
  EXPECT_TRUE(Match(body_request_));
}

TEST_P(CacheStorageCacheTestP, VaryMultiple) {
  body_request_.headers["vary_foo"] = "foo";
  body_request_.headers["vary_bar"] = "bar";
  blink::mojom::FetchAPIResponsePtr body_response = CreateBlobBodyResponse();
  body_response->headers["vary"] = " vary_foo    , vary_bar";
  EXPECT_TRUE(Put(body_request_, std::move(body_response)));
  EXPECT_TRUE(Match(body_request_));

  body_request_.headers["vary_bar"] = "foo";
  EXPECT_FALSE(Match(body_request_));

  body_request_.headers.erase("vary_bar");
  EXPECT_FALSE(Match(body_request_));
}

TEST_P(CacheStorageCacheTestP, VaryNewHeader) {
  body_request_.headers["vary_foo"] = "foo";
  blink::mojom::FetchAPIResponsePtr body_response = CreateBlobBodyResponse();
  body_response->headers["vary"] = " vary_foo, vary_bar";
  EXPECT_TRUE(Put(body_request_, std::move(body_response)));
  EXPECT_TRUE(Match(body_request_));

  body_request_.headers["vary_bar"] = "bar";
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
  std::vector<std::string> expected_keys{no_body_request_.url.spec(),
                                         body_request_.url.spec()};
  EXPECT_EQ(expected_keys, callback_strings_);
}

TEST_P(CacheStorageCacheTestP, TwoKeysThenOne) {
  EXPECT_TRUE(Put(no_body_request_, CreateNoBodyResponse()));
  EXPECT_TRUE(Put(body_request_, CreateBlobBodyResponse()));
  EXPECT_TRUE(Keys());
  std::vector<std::string> expected_keys{no_body_request_.url.spec(),
                                         body_request_.url.spec()};
  EXPECT_EQ(expected_keys, callback_strings_);

  EXPECT_TRUE(Delete(body_request_));
  EXPECT_TRUE(Keys());
  std::vector<std::string> expected_keys2{no_body_request_.url.spec()};
  EXPECT_EQ(expected_keys2, callback_strings_);
}

TEST_P(CacheStorageCacheTestP, KeysWithIgnoreSearchTrue) {
  EXPECT_TRUE(Put(no_body_request_, CreateNoBodyResponse()));
  EXPECT_TRUE(Put(body_request_, CreateBlobBodyResponse()));
  EXPECT_TRUE(Put(body_request_with_query_, CreateBlobBodyResponseWithQuery()));

  blink::mojom::QueryParamsPtr match_params = blink::mojom::QueryParams::New();
  match_params->ignore_search = true;

  EXPECT_TRUE(Keys(body_request_with_query_, std::move(match_params)));
  std::vector<std::string> expected_keys = {
      body_request_.url.spec(), body_request_with_query_.url.spec()};
  EXPECT_EQ(expected_keys, callback_strings_);
}

TEST_P(CacheStorageCacheTestP, KeysWithIgnoreSearchFalse) {
  EXPECT_TRUE(Put(no_body_request_, CreateNoBodyResponse()));
  EXPECT_TRUE(Put(body_request_, CreateBlobBodyResponse()));
  EXPECT_TRUE(Put(body_request_with_query_, CreateBlobBodyResponseWithQuery()));

  // Default value of ignore_search is false.
  blink::mojom::QueryParamsPtr match_params = blink::mojom::QueryParams::New();
  EXPECT_EQ(match_params->ignore_search, false);

  EXPECT_TRUE(Keys(body_request_with_query_, std::move(match_params)));
  std::vector<std::string> expected_keys = {
      body_request_with_query_.url.spec()};
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
  std::vector<std::string> expected_keys{no_body_request_.url.spec(),
                                         body_request_.url.spec(),
                                         body_request_with_query_.url.spec()};
  EXPECT_EQ(expected_keys, callback_strings_);

  // The following delete operation will remove both of body_request_ and
  // body_request_with_query_ from cache storage.
  blink::mojom::QueryParamsPtr match_params = blink::mojom::QueryParams::New();
  match_params->ignore_search = true;
  EXPECT_TRUE(Delete(body_request_with_query_, std::move(match_params)));

  EXPECT_TRUE(Keys());
  expected_keys.clear();
  std::vector<std::string> expected_keys2{no_body_request_.url.spec()};
  EXPECT_EQ(expected_keys2, callback_strings_);
}

TEST_P(CacheStorageCacheTestP, DeleteWithIgnoreSearchFalse) {
  EXPECT_TRUE(Put(no_body_request_, CreateNoBodyResponse()));
  EXPECT_TRUE(Put(body_request_, CreateBlobBodyResponse()));
  EXPECT_TRUE(Put(body_request_with_query_, CreateBlobBodyResponseWithQuery()));

  EXPECT_TRUE(Keys());
  std::vector<std::string> expected_keys{no_body_request_.url.spec(),
                                         body_request_.url.spec(),
                                         body_request_with_query_.url.spec()};
  EXPECT_EQ(expected_keys, callback_strings_);

  // Default value of ignore_search is false.
  blink::mojom::QueryParamsPtr match_params = blink::mojom::QueryParams::New();
  EXPECT_EQ(match_params->ignore_search, false);

  EXPECT_TRUE(Delete(body_request_with_query_, std::move(match_params)));

  EXPECT_TRUE(Keys());
  std::vector<std::string> expected_keys2{no_body_request_.url.spec(),
                                          body_request_.url.spec()};
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
  EXPECT_TRUE(TestResponseType(network::mojom::FetchResponseType::kCORS));
  EXPECT_TRUE(TestResponseType(network::mojom::FetchResponseType::kDefault));
  EXPECT_TRUE(TestResponseType(network::mojom::FetchResponseType::kError));
  EXPECT_TRUE(TestResponseType(network::mojom::FetchResponseType::kOpaque));
  EXPECT_TRUE(
      TestResponseType(network::mojom::FetchResponseType::kOpaqueRedirect));
}

TEST_P(CacheStorageCacheTestP, PutWithSideData) {
  blink::mojom::FetchAPIResponsePtr response = CreateBlobBodyResponse();

  const std::string expected_side_data = "SideData";
  std::unique_ptr<storage::BlobDataHandle> side_data_blob_handle =
      BuildBlobHandle("blob-id:mysideblob", expected_side_data);

  CopySideDataToResponse(side_data_blob_handle.get(), response.get());
  EXPECT_TRUE(Put(body_request_, std::move(response)));

  EXPECT_TRUE(Match(body_request_));
  ASSERT_TRUE(callback_response_->blob);
  blink::mojom::BlobPtr blob(std::move(callback_response_->blob->blob));
  EXPECT_TRUE(ResponseBodiesEqual(expected_blob_data_, blob.get()));
  EXPECT_TRUE(ResponseSideDataEqual(expected_side_data, blob.get()));
}

TEST_P(CacheStorageCacheTestP, PutWithSideData_QuotaExceeded) {
  mock_quota_manager_->SetQuota(kOrigin, blink::mojom::StorageType::kTemporary,
                                expected_blob_data_.size() - 1);
  blink::mojom::FetchAPIResponsePtr response = CreateBlobBodyResponse();
  const std::string expected_side_data = "SideData";
  std::unique_ptr<storage::BlobDataHandle> side_data_blob_handle =
      BuildBlobHandle("blob-id:mysideblob", expected_side_data);

  CopySideDataToResponse(side_data_blob_handle.get(), response.get());
  // When the available space is not enough for the body, Put operation must
  // fail.
  EXPECT_FALSE(Put(body_request_, std::move(response)));
  EXPECT_EQ(CacheStorageError::kErrorQuotaExceeded, callback_error_);
}

TEST_P(CacheStorageCacheTestP, PutWithSideData_QuotaExceededSkipSideData) {
  mock_quota_manager_->SetQuota(kOrigin, blink::mojom::StorageType::kTemporary,
                                expected_blob_data_.size());
  blink::mojom::FetchAPIResponsePtr response = CreateBlobBodyResponse();
  const std::string expected_side_data = "SideData";
  std::unique_ptr<storage::BlobDataHandle> side_data_blob_handle =
      BuildBlobHandle("blob-id:mysideblob", expected_side_data);

  CopySideDataToResponse(side_data_blob_handle.get(), response.get());
  // When the available space is enough for the body but not enough for the side
  // data, Put operation must succeed.
  EXPECT_TRUE(Put(body_request_, std::move(response)));

  EXPECT_TRUE(Match(body_request_));
  ASSERT_TRUE(callback_response_->blob);
  blink::mojom::BlobPtr blob(std::move(callback_response_->blob->blob));
  EXPECT_TRUE(ResponseBodiesEqual(expected_blob_data_, blob.get()));
  // The side data should not be written.
  EXPECT_TRUE(ResponseSideDataEqual("", blob.get()));
}

TEST_P(CacheStorageCacheTestP, PutWithSideData_BadMessage) {
  base::HistogramTester histogram_tester;
  blink::mojom::FetchAPIResponsePtr response = CreateBlobBodyResponse();

  const std::string expected_side_data = "SideData";
  std::unique_ptr<storage::BlobDataHandle> side_data_blob_handle =
      BuildBlobHandle("blob-id:mysideblob", expected_side_data);

  CopySideDataToResponse(side_data_blob_handle.get(), response.get());

  blink::mojom::BatchOperationPtr operation =
      blink::mojom::BatchOperation::New();
  operation->operation_type = blink::mojom::OperationType::kPut;
  operation->request = body_request_;
  operation->response = std::move(response);
  operation->response->blob->size = std::numeric_limits<uint64_t>::max();

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
  EXPECT_TRUE(WriteSideData(body_request_.url, response_time, buffer1,
                            expected_side_data1.length()));

  EXPECT_TRUE(Match(body_request_));
  ASSERT_TRUE(callback_response_->blob);
  blink::mojom::BlobPtr blob1(std::move(callback_response_->blob->blob));
  EXPECT_TRUE(ResponseBodiesEqual(expected_blob_data_, blob1.get()));
  EXPECT_TRUE(ResponseSideDataEqual(expected_side_data1, blob1.get()));

  const std::string expected_side_data2 = "New data";
  scoped_refptr<net::IOBuffer> buffer2 =
      base::MakeRefCounted<net::StringIOBuffer>(expected_side_data2);
  EXPECT_TRUE(WriteSideData(body_request_.url, response_time, buffer2,
                            expected_side_data2.length()));
  EXPECT_TRUE(Match(body_request_));
  ASSERT_TRUE(callback_response_->blob);
  blink::mojom::BlobPtr blob2(std::move(callback_response_->blob->blob));
  EXPECT_TRUE(ResponseBodiesEqual(expected_blob_data_, blob2.get()));
  EXPECT_TRUE(ResponseSideDataEqual(expected_side_data2, blob2.get()));

  ASSERT_TRUE(Delete(body_request_));
}

TEST_P(CacheStorageCacheTestP, WriteSideData_QuotaExceeded) {
  mock_quota_manager_->SetQuota(kOrigin, blink::mojom::StorageType::kTemporary,
                                1024 * 1023);
  base::Time response_time(base::Time::Now());
  blink::mojom::FetchAPIResponsePtr response(CreateNoBodyResponse());
  response->response_time = response_time;
  EXPECT_TRUE(Put(no_body_request_, std::move(response)));

  const size_t kSize = 1024 * 1024;
  scoped_refptr<net::IOBuffer> buffer =
      base::MakeRefCounted<net::IOBuffer>(kSize);
  memset(buffer->data(), 0, kSize);
  EXPECT_FALSE(
      WriteSideData(no_body_request_.url, response_time, buffer, kSize));
  EXPECT_EQ(CacheStorageError::kErrorQuotaExceeded, callback_error_);
  ASSERT_TRUE(Delete(no_body_request_));
}

TEST_P(CacheStorageCacheTestP, WriteSideData_QuotaManagerModified) {
  base::Time response_time(base::Time::Now());
  blink::mojom::FetchAPIResponsePtr response(CreateNoBodyResponse());
  response->response_time = response_time;
  EXPECT_EQ(0, quota_manager_proxy_->notify_storage_modified_count());
  EXPECT_TRUE(Put(no_body_request_, std::move(response)));
  // Storage notification happens after the operation returns, so continue the
  // event loop.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, quota_manager_proxy_->notify_storage_modified_count());

  const size_t kSize = 10;
  scoped_refptr<net::IOBuffer> buffer =
      base::MakeRefCounted<net::IOBuffer>(kSize);
  memset(buffer->data(), 0, kSize);
  EXPECT_TRUE(
      WriteSideData(no_body_request_.url, response_time, buffer, kSize));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, quota_manager_proxy_->notify_storage_modified_count());
  ASSERT_TRUE(Delete(no_body_request_));
}

TEST_P(CacheStorageCacheTestP, WriteSideData_DifferentTimeStamp) {
  base::Time response_time(base::Time::Now());
  blink::mojom::FetchAPIResponsePtr response(CreateNoBodyResponse());
  response->response_time = response_time;
  EXPECT_TRUE(Put(no_body_request_, std::move(response)));

  const size_t kSize = 10;
  scoped_refptr<net::IOBuffer> buffer =
      base::MakeRefCounted<net::IOBuffer>(kSize);
  memset(buffer->data(), 0, kSize);
  EXPECT_FALSE(WriteSideData(no_body_request_.url,
                             response_time + base::TimeDelta::FromSeconds(1),
                             buffer, kSize));
  EXPECT_EQ(CacheStorageError::kErrorNotFound, callback_error_);
  ASSERT_TRUE(Delete(no_body_request_));
}

TEST_P(CacheStorageCacheTestP, WriteSideData_NotFound) {
  const size_t kSize = 10;
  scoped_refptr<net::IOBuffer> buffer =
      base::MakeRefCounted<net::IOBuffer>(kSize);
  memset(buffer->data(), 0, kSize);
  EXPECT_FALSE(WriteSideData(GURL("http://www.example.com/not_exist"),
                             base::Time::Now(), buffer, kSize));
  EXPECT_EQ(CacheStorageError::kErrorNotFound, callback_error_);
}

TEST_F(CacheStorageCacheTest, CaselessServiceWorkerFetchRequestHeaders) {
  // CacheStorageCache depends on ServiceWorkerFetchRequest having caseless
  // headers so that it can quickly lookup vary headers.
  ServiceWorkerFetchRequest request(GURL("http://www.example.com"), "GET",
                                    ServiceWorkerHeaderMap(), Referrer(),
                                    false);
  request.headers["content-type"] = "foo";
  request.headers["Content-Type"] = "bar";
  EXPECT_EQ("bar", request.headers["content-type"]);
}

TEST_P(CacheStorageCacheTestP, QuotaManagerModified) {
  EXPECT_EQ(0, quota_manager_proxy_->notify_storage_modified_count());

  EXPECT_TRUE(Put(no_body_request_, CreateNoBodyResponse()));
  // Storage notification happens after the operation returns, so continue the
  // event loop.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, quota_manager_proxy_->notify_storage_modified_count());
  EXPECT_LT(0, quota_manager_proxy_->last_notified_delta());
  int64_t sum_delta = quota_manager_proxy_->last_notified_delta();

  EXPECT_TRUE(Put(body_request_, CreateBlobBodyResponse()));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, quota_manager_proxy_->notify_storage_modified_count());
  EXPECT_LT(sum_delta, quota_manager_proxy_->last_notified_delta());
  sum_delta += quota_manager_proxy_->last_notified_delta();

  EXPECT_TRUE(Delete(body_request_));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(3, quota_manager_proxy_->notify_storage_modified_count());
  sum_delta += quota_manager_proxy_->last_notified_delta();

  EXPECT_TRUE(Delete(no_body_request_));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(4, quota_manager_proxy_->notify_storage_modified_count());
  sum_delta += quota_manager_proxy_->last_notified_delta();

  EXPECT_EQ(0, sum_delta);
}

TEST_P(CacheStorageCacheTestP, PutObeysQuotaLimits) {
  mock_quota_manager_->SetQuota(kOrigin, blink::mojom::StorageType::kTemporary,
                                0);
  EXPECT_FALSE(Put(body_request_, CreateBlobBodyResponse()));
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

  ServiceWorkerFetchRequest non_opaque_request(body_request_);
  non_opaque_request.url = GURL("http://example.com/no-pad.html");
  blink::mojom::FetchAPIResponsePtr non_opaque_response =
      CreateBlobBodyResponse();
  non_opaque_response->response_time = response_time;
  EXPECT_EQ(0, CacheStorageCache::CalculateResponsePadding(
                   *non_opaque_response, CreateTestPaddingKey().get(),
                   0 /* side_data_size */));
  EXPECT_TRUE(Put(non_opaque_request, std::move(non_opaque_response)));
  int64_t unpadded_no_data_cache_size = Size();

  // Now write some side data to that cache.
  const std::string expected_side_data(2048, 'X');
  scoped_refptr<net::IOBuffer> side_data_buffer =
      base::MakeRefCounted<net::StringIOBuffer>(expected_side_data);
  EXPECT_TRUE(WriteSideData(non_opaque_request.url, response_time,
                            side_data_buffer, expected_side_data.length()));
  int64_t unpadded_total_resource_size = Size();
  int64_t unpadded_side_data_size =
      unpadded_total_resource_size - unpadded_no_data_cache_size;
  EXPECT_EQ(expected_side_data.size(),
            static_cast<size_t>(unpadded_side_data_size));
  blink::mojom::FetchAPIResponsePtr non_opaque_response_clone =
      CreateBlobBodyResponse();
  non_opaque_response_clone->response_time = response_time;
  EXPECT_EQ(0, CacheStorageCache::CalculateResponsePadding(
                   *non_opaque_response_clone, CreateTestPaddingKey().get(),
                   unpadded_side_data_size));

  // Now write an identically sized opaque response.
  ServiceWorkerFetchRequest opaque_request(non_opaque_request);
  opaque_request.url = GURL("http://example.com/opaque.html");
  // Same URL length means same cache sizes (ignoring padding).
  EXPECT_EQ(opaque_request.url.spec().length(),
            non_opaque_request.url.spec().length());
  blink::mojom::FetchAPIResponsePtr opaque_response(CreateBlobBodyResponse());
  opaque_response->response_type = network::mojom::FetchResponseType::kOpaque;
  opaque_response->response_time = response_time;

  EXPECT_TRUE(Put(opaque_request, std::move(opaque_response)));
  // This test is fragile. Right now it deterministically adds non-zero padding.
  // But if the url, padding key, or padding algorithm change it might become
  // zero.
  int64_t size_after_opaque_put = Size();
  int64_t opaque_padding = size_after_opaque_put -
                           2 * unpadded_no_data_cache_size -
                           unpadded_side_data_size;
  ASSERT_GT(opaque_padding, 0);

  // Now write side data and expect to see the padding change.
  EXPECT_TRUE(WriteSideData(opaque_request.url, response_time, side_data_buffer,
                            expected_side_data.length()));
  int64_t current_padding = Size() - 2 * unpadded_total_resource_size;
  EXPECT_NE(opaque_padding, current_padding);

  // Now reset opaque side data back to zero.
  const std::string expected_side_data2 = "";
  scoped_refptr<net::IOBuffer> buffer2 =
      base::MakeRefCounted<net::StringIOBuffer>(expected_side_data2);
  EXPECT_TRUE(WriteSideData(opaque_request.url, response_time, buffer2,
                            expected_side_data2.length()));
  EXPECT_EQ(size_after_opaque_put, Size());

  // And delete the opaque response entirely.
  EXPECT_TRUE(Delete(opaque_request));
  EXPECT_EQ(unpadded_total_resource_size, Size());
}

TEST_F(CacheStorageCacheTest, TestDifferentOpaqueSideDataSizes) {
  ServiceWorkerFetchRequest request(body_request_);

  blink::mojom::FetchAPIResponsePtr response(CreateBlobBodyResponse());
  response->response_type = network::mojom::FetchResponseType::kOpaque;
  base::Time response_time(base::Time::Now());
  response->response_time = response_time;
  EXPECT_TRUE(Put(request, std::move(response)));
  int64_t opaque_cache_size_no_side_data = Size();

  const std::string small_side_data(1024, 'X');
  scoped_refptr<net::IOBuffer> buffer1 =
      base::MakeRefCounted<net::StringIOBuffer>(small_side_data);
  EXPECT_TRUE(WriteSideData(request.url, response_time, buffer1,
                            small_side_data.length()));
  int64_t opaque_cache_size_with_side_data = Size();
  EXPECT_NE(opaque_cache_size_with_side_data, opaque_cache_size_no_side_data);

  // Write side data of a different size. The size should not affect the padding
  // at all.
  const std::string large_side_data(2048, 'X');
  EXPECT_NE(large_side_data.length(), small_side_data.length());
  scoped_refptr<net::IOBuffer> buffer2 =
      base::MakeRefCounted<net::StringIOBuffer>(large_side_data);
  EXPECT_TRUE(WriteSideData(request.url, response_time, buffer2,
                            large_side_data.length()));
  int side_data_delta = large_side_data.length() - small_side_data.length();
  EXPECT_EQ(opaque_cache_size_with_side_data + side_data_delta, Size());
}

TEST_F(CacheStorageCacheTest, TestDoubleOpaquePut) {
  ServiceWorkerFetchRequest request(body_request_);

  base::Time response_time(base::Time::Now());

  blink::mojom::FetchAPIResponsePtr response(CreateBlobBodyResponse());
  response->response_type = network::mojom::FetchResponseType::kOpaque;
  response->response_time = response_time;
  EXPECT_TRUE(Put(request, std::move(response)));
  int64_t size_after_first_put = Size();

  ServiceWorkerFetchRequest request2(body_request_);
  blink::mojom::FetchAPIResponsePtr response2(CreateBlobBodyResponse());
  response2->response_type = network::mojom::FetchResponseType::kOpaque;
  response2->response_time = response_time;
  EXPECT_TRUE(Put(request2, std::move(response2)));

  EXPECT_EQ(size_after_first_put, Size());
}

TEST_P(CacheStorageCacheTestP, GetSizeThenClose) {
  // Create the backend and put something in it.
  EXPECT_TRUE(Put(body_request_, CreateBlobBodyResponse()));
  // Get a reference to the response in the cache.
  EXPECT_TRUE(Match(body_request_));
  blink::mojom::BlobPtr blob(std::move(callback_response_->blob->blob));
  callback_response_ = nullptr;

  int64_t cache_size = Size();
  EXPECT_EQ(cache_size, GetSizeThenClose());
  VerifyAllOpsFail();

  // Reading blob should fail.
  EXPECT_TRUE(ResponseBodiesEqual("", blob.get()));
}

TEST_P(CacheStorageCacheTestP, OpsFailOnClosedBackend) {
  // Create the backend and put something in it.
  EXPECT_TRUE(Put(body_request_, CreateBlobBodyResponse()));
  EXPECT_TRUE(Close());
  VerifyAllOpsFail();
}

TEST_P(CacheStorageCacheTestP, BlobReferenceDelaysClose) {
  // Create the backend and put something in it.
  EXPECT_TRUE(Put(body_request_, CreateBlobBodyResponse()));
  // Get a reference to the response in the cache.
  EXPECT_TRUE(Match(body_request_));
  blink::mojom::BlobPtr blob(std::move(callback_response_->blob->blob));
  callback_response_ = nullptr;

  base::RunLoop loop;
  cache_->Close(base::BindOnce(&CacheStorageCacheTest::CloseCallback,
                               base::Unretained(this),
                               base::Unretained(&loop)));
  browser_thread_bundle_.RunUntilIdle();
  // If MemoryOnly closing does succeed right away.
  EXPECT_EQ(MemoryOnly(), callback_closed_);

  // Reading blob should succeed.
  EXPECT_TRUE(ResponseBodiesEqual(expected_blob_data_, blob.get()));
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

  int sequence_out = -1;

  blink::mojom::BatchOperationPtr operation1 =
      blink::mojom::BatchOperation::New();
  operation1->operation_type = blink::mojom::OperationType::kPut;
  operation1->request = body_request_;
  operation1->response = CreateBlobBodyResponse();

  std::unique_ptr<base::RunLoop> close_loop1(new base::RunLoop());
  std::vector<blink::mojom::BatchOperationPtr> operations1;
  operations1.emplace_back(std::move(operation1));
  cache_->BatchOperation(
      std::move(operations1), true /* fail_on_duplicate */,
      base::BindOnce(&CacheStorageCacheTest::SequenceCallback,
                     base::Unretained(this), 1, &sequence_out,
                     close_loop1.get()),
      CacheStorageCache::BadMessageCallback());

  // Blocks on creating the cache entry.
  base::RunLoop().RunUntilIdle();

  blink::mojom::BatchOperationPtr operation2 =
      blink::mojom::BatchOperation::New();
  operation2->operation_type = blink::mojom::OperationType::kPut;
  operation2->request = body_request_;
  operation2->response = CreateBlobBodyResponse();

  delayable_backend->set_delay_open_entry(false);
  std::unique_ptr<base::RunLoop> close_loop2(new base::RunLoop());
  std::vector<blink::mojom::BatchOperationPtr> operations2;
  operations2.emplace_back(std::move(operation2));
  cache_->BatchOperation(
      std::move(operations2), true /* fail_on_duplicate */,
      base::BindOnce(&CacheStorageCacheTest::SequenceCallback,
                     base::Unretained(this), 2, &sequence_out,
                     close_loop2.get()),
      CacheStorageCache::BadMessageCallback());

  // The second put operation should wait for the first to complete.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(callback_response_);

  EXPECT_TRUE(delayable_backend->OpenEntryContinue());
  close_loop1->Run();
  EXPECT_EQ(1, sequence_out);
  close_loop2->Run();
  EXPECT_EQ(2, sequence_out);
}

INSTANTIATE_TEST_CASE_P(CacheStorageCacheTest,
                        CacheStorageCacheTestP,
                        ::testing::Values(false, true));

}  // namespace cache_storage_cache_unittest
}  // namespace content
