// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_data_manager.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/uuid.h"
#include "content/browser/background_fetch/background_fetch.pb.h"
#include "content/browser/background_fetch/background_fetch_data_manager_observer.h"
#include "content/browser/background_fetch/background_fetch_request_info.h"
#include "content/browser/background_fetch/background_fetch_request_match_params.h"
#include "content/browser/background_fetch/background_fetch_test_base.h"
#include "content/browser/background_fetch/background_fetch_test_data_manager.h"
#include "content/browser/background_fetch/storage/database_helpers.h"
#include "content/browser/background_fetch/storage/image_helpers.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/common/background_fetch/background_fetch_types.h"
#include "content/common/fetch/fetch_api_request_proto.h"
#include "content/public/browser/background_fetch_response.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_impl.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/test/blob_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/background_fetch/background_fetch.mojom.h"
#include "third_party/blink/public/mojom/cache_storage/cache_storage.mojom.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/origin.h"

namespace content {
namespace {

using background_fetch::BackgroundFetchInitializationData;
using ::testing::_;
using ::testing::UnorderedElementsAre;
using ::testing::IsEmpty;

const char kUserDataPrefix[] = "bgfetch_";

const char kExampleDeveloperId[] = "my-example-id";
const char kAlternativeDeveloperId[] = "my-other-id";
const char kExampleUniqueId[] = "7e57ab1e-c0de-a150-ca75-1e75f005ba11";
const char kAlternativeUniqueId[] = "bb48a9fb-c21f-4c2d-a9ae-58bd48a9fb53";

const char kInitialTitle[] = "Initial Title";

constexpr size_t kResponseSize = 42u;

void DidGetInitializationData(
    base::OnceClosure quit_closure,
    std::vector<BackgroundFetchInitializationData>* out_result,
    blink::mojom::BackgroundFetchError error,
    std::vector<BackgroundFetchInitializationData> result) {
  DCHECK_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  *out_result = std::move(result);
  std::move(quit_closure).Run();
}

void DidCreateRegistration(
    base::OnceClosure quit_closure,
    blink::mojom::BackgroundFetchError* out_error,
    blink::mojom::BackgroundFetchError error,
    blink::mojom::BackgroundFetchRegistrationDataPtr registration) {
  *out_error = error;
  std::move(quit_closure).Run();
}

void DidGetError(base::OnceClosure quit_closure,
                 blink::mojom::BackgroundFetchError* out_error,
                 blink::mojom::BackgroundFetchError error) {
  *out_error = error;
  std::move(quit_closure).Run();
}

void DidGetRegistrationUserDataByKeyPrefix(
    base::OnceClosure quit_closure,
    std::vector<std::string>* out_data,
    const std::vector<std::string>& data,
    blink::ServiceWorkerStatusCode status) {
  DCHECK(out_data);
  DCHECK_EQ(blink::ServiceWorkerStatusCode::kOk, status);
  *out_data = data;
  std::move(quit_closure).Run();
}

void DidStoreUserData(base::OnceClosure quit_closure,
                      blink::ServiceWorkerStatusCode status) {
  DCHECK_EQ(blink::ServiceWorkerStatusCode::kOk, status);
  std::move(quit_closure).Run();
}

void GetNumUserData(base::OnceClosure quit_closure,
                    size_t* out_size,
                    const std::vector<std::string>& data,
                    blink::ServiceWorkerStatusCode status) {
  DCHECK(out_size);
  DCHECK_EQ(blink::ServiceWorkerStatusCode::kOk, status);
  *out_size = data.size();
  std::move(quit_closure).Run();
}

struct ResponseStateStats {
  size_t pending_requests = 0;
  size_t active_requests = 0;
  size_t completed_requests = 0;
};

bool operator==(const ResponseStateStats& s1, const ResponseStateStats& s2) {
  return s1.pending_requests == s2.pending_requests &&
         s1.active_requests == s2.active_requests &&
         s1.completed_requests == s2.completed_requests;
}

std::vector<blink::mojom::FetchAPIRequestPtr> CreateValidRequests(
    const url::Origin& origin,
    size_t num_requests = 1u) {
  std::vector<blink::mojom::FetchAPIRequestPtr> requests(num_requests);
  for (size_t i = 0; i < requests.size(); i++) {
    requests[i] = blink::mojom::FetchAPIRequest::New();
    requests[i]->referrer = blink::mojom::Referrer::New();
    // Creates a URL of the form: `http://example.com/x`
    requests[i]->url = GURL(origin.GetURL().spec() + base::NumberToString(i));
  }
  return requests;
}

blink::mojom::FetchAPIRequestPtr CreateValidRequestWithMethod(
    const url::Origin& origin,
    const std::string& method) {
  auto request = blink::mojom::FetchAPIRequest::New();
  request->referrer = blink::mojom::Referrer::New();
  request->url = origin.GetURL();
  request->method = method;
  return request;
}

SkBitmap CreateTestIcon(int size = 42, SkColor color = SK_ColorGREEN) {
  SkBitmap icon;
  icon.allocN32Pixels(size, size);
  icon.eraseColor(SK_ColorGREEN);
  return icon;
}

void ExpectIconProperties(const SkBitmap& icon, int size, SkColor color) {
  EXPECT_FALSE(icon.isNull());
  EXPECT_EQ(icon.width(), size);
  EXPECT_EQ(icon.height(), size);
  for (int i = 0; i < icon.width(); i++) {
    for (int j = 0; j < icon.height(); j++)
      EXPECT_EQ(icon.getColor(i, j), color);
  }
}

std::vector<blink::mojom::FetchAPIRequestPtr> CloneRequestVector(
    const std::vector<blink::mojom::FetchAPIRequestPtr>& requests) {
  std::vector<blink::mojom::FetchAPIRequestPtr> requests_out;
  for (const auto& request : requests)
    requests_out.push_back(BackgroundFetchSettledFetch::CloneRequest(request));
  return requests_out;
}

}  // namespace

class BackgroundFetchDataManagerTest
    : public BackgroundFetchTestBase,
      public BackgroundFetchDataManagerObserver {
 public:
  BackgroundFetchDataManagerTest() {
    RestartDataManagerFromPersistentStorage();
  }

  ~BackgroundFetchDataManagerTest() override {
    background_fetch_data_manager_->RemoveObserver(this);
  }

  void TearDown() override {
    // Allow remaining tasks on the cache thread and main thread to run to clean
    // up all dangling file handles.
    base::ThreadPoolInstance::Get()->FlushForTesting();
    base::RunLoop().RunUntilIdle();
    BackgroundFetchTestBase::TearDown();
  }

  // Re-creates the data manager. Useful for testing that data was persisted.
  void RestartDataManagerFromPersistentStorage() {
    background_fetch_data_manager_ =
        std::make_unique<BackgroundFetchTestDataManager>(
            browser_context(), storage_partition(),
            embedded_worker_test_helper()->context_wrapper());

    background_fetch_data_manager_->AddObserver(this);
    background_fetch_data_manager_->Initialize();
  }

  // Synchronous version of BackgroundFetchDataManager::GetInitializationData().
  std::vector<BackgroundFetchInitializationData> GetInitializationData() {
    // Simulate browser restart. This re-initializes |data_manager_|, since
    // this DatabaseTask should only be called on browser startup.
    RestartDataManagerFromPersistentStorage();

    std::vector<BackgroundFetchInitializationData> result;

    base::RunLoop run_loop;
    background_fetch_data_manager_->GetInitializationData(base::BindOnce(
        &DidGetInitializationData, run_loop.QuitClosure(), &result));
    run_loop.Run();

    return result;
  }

  // Synchronous version of BackgroundFetchDataManager::CreateRegistration().
  void CreateRegistration(
      const BackgroundFetchRegistrationId& registration_id,
      std::vector<blink::mojom::FetchAPIRequestPtr> requests,
      blink::mojom::BackgroundFetchOptionsPtr options,
      const SkBitmap& icon,
      blink::mojom::BackgroundFetchError* out_error) {
    DCHECK(out_error);

    base::RunLoop run_loop;
    background_fetch_data_manager_->CreateRegistration(
        registration_id, std::move(requests), std::move(options), icon,
        /* start_paused= */ false, net::IsolationInfo(),
        base::BindOnce(&DidCreateRegistration, run_loop.QuitClosure(),
                       out_error));
    run_loop.Run();

    // Check that a cache was created.
    if (*out_error == blink::mojom::BackgroundFetchError::NONE)
      DCHECK(HasCache(registration_id.unique_id()));
  }

  blink::mojom::BackgroundFetchRegistrationDataPtr GetRegistration(
      int64_t service_worker_registration_id,
      const blink::StorageKey& storage_key,
      const std::string& developer_id,
      blink::mojom::BackgroundFetchError* out_error) {
    DCHECK(out_error);

    auto registration_data =
        blink::mojom::BackgroundFetchRegistrationData::New();
    base::RunLoop run_loop;
    background_fetch_data_manager_->GetRegistration(
        service_worker_registration_id, storage_key, developer_id,
        base::BindOnce(&BackgroundFetchDataManagerTest::DidGetRegistration,
                       base::Unretained(this), run_loop.QuitClosure(),
                       out_error, registration_data.get()));
    run_loop.Run();

    return registration_data;
  }

  std::unique_ptr<proto::BackgroundFetchMetadata> GetMetadata(
      int64_t service_worker_registration_id,
      const std::string& unique_id) {
    std::unique_ptr<proto::BackgroundFetchMetadata> metadata;

    base::RunLoop run_loop;
    embedded_worker_test_helper()->context_wrapper()->GetRegistrationUserData(
        service_worker_registration_id,
        {background_fetch::RegistrationKey(unique_id)},
        base::BindOnce(&BackgroundFetchDataManagerTest::DidGetMetadata,
                       base::Unretained(this), run_loop.QuitClosure(),
                       &metadata));
    run_loop.Run();

    return metadata;
  }

  std::vector<std::string> GetDeveloperIds(
      int64_t service_worker_registration_id,
      const blink::StorageKey& storage_key,
      blink::mojom::BackgroundFetchError* out_error) {
    DCHECK(out_error);

    std::vector<std::string> ids;
    base::RunLoop run_loop;
    background_fetch_data_manager_->GetDeveloperIdsForServiceWorker(
        service_worker_registration_id, storage_key,
        base::BindOnce(&BackgroundFetchDataManagerTest::DidGetDeveloperIds,
                       base::Unretained(this), run_loop.QuitClosure(),
                       out_error, &ids));
    run_loop.Run();

    return ids;
  }

  // Synchronous version of
  // BackgroundFetchDataManager::PopNextRequest().
  void PopNextRequest(
      const BackgroundFetchRegistrationId& registration_id,
      blink::mojom::BackgroundFetchError* out_error,
      scoped_refptr<BackgroundFetchRequestInfo>* out_request_info) {
    DCHECK(out_error);
    DCHECK(out_request_info);

    base::RunLoop run_loop;
    background_fetch_data_manager_->PopNextRequest(
        registration_id,
        base::BindOnce(&BackgroundFetchDataManagerTest::DidPopNextRequest,
                       base::Unretained(this), run_loop.QuitClosure(),
                       out_error, out_request_info));
    run_loop.Run();
  }

  // Synchronous version of
  // BackgroundFetchDataManager::GetRequestBlob().
  std::string GetRequestBlobAsString(
      const BackgroundFetchRegistrationId& registration_id,
      const scoped_refptr<BackgroundFetchRequestInfo>& request_info,
      blink::mojom::BackgroundFetchError* out_error) {
    DCHECK(out_error);

    blink::mojom::SerializedBlobPtr blob;

    base::RunLoop run_loop;
    background_fetch_data_manager_->GetRequestBlob(
        registration_id, request_info,
        base::BindOnce(&BackgroundFetchDataManagerTest::DidGetRequestBlob,
                       base::Unretained(this), run_loop.QuitClosure(),
                       out_error, &blob));
    run_loop.Run();

    if (blob && blob->blob) {
      mojo::Remote<blink::mojom::Blob> blob_remote(std::move(blob->blob));
      return storage::BlobToString(blob_remote.get());
    }

    return std::string();
  }

  // Synchronous version of
  // BackgroundFetchDataManager::MarkRegistrationForDeletion().
  void MarkRegistrationForDeletion(
      const BackgroundFetchRegistrationId& registration_id,
      bool check_for_failure,
      blink::mojom::BackgroundFetchError* out_error,
      blink::mojom::BackgroundFetchFailureReason* out_failure_reason) {
    DCHECK(out_error);
    DCHECK(out_failure_reason);

    base::RunLoop run_loop;
    background_fetch_data_manager_->MarkRegistrationForDeletion(
        registration_id, check_for_failure,
        base::BindOnce(
            &BackgroundFetchDataManagerTest::DidMarkRegistrationForDeletion,
            base::Unretained(this), run_loop.QuitClosure(), out_error,
            out_failure_reason));
    run_loop.Run();
  }

  // Synchronous version of BackgroundFetchDataManager::DeleteRegistration().
  void DeleteRegistration(const BackgroundFetchRegistrationId& registration_id,
                          blink::mojom::BackgroundFetchError* out_error) {
    DCHECK(out_error);

    base::RunLoop run_loop;
    background_fetch_data_manager_->DeleteRegistration(
        registration_id,
        base::BindOnce(&DidGetError, run_loop.QuitClosure(), out_error));
    run_loop.Run();
  }

  // Synchronous version of BackgroundFetchDataManager::MarkRequestAsComplete().
  void MarkRequestAsComplete(
      const BackgroundFetchRegistrationId& registration_id,
      BackgroundFetchRequestInfo* request_info,
      blink::mojom::BackgroundFetchError* out_error) {
    DCHECK(out_error);

    base::RunLoop run_loop;
    background_fetch_data_manager_->MarkRequestAsComplete(
        registration_id, request_info,
        base::BindOnce(
            &BackgroundFetchDataManagerTest::DidMarkRequestAsComplete,
            base::Unretained(this), run_loop.QuitClosure(), out_error));
    run_loop.Run();
  }

  // Synchronous version of
  // BackgroundFetchDataManager::MatchRequests().
  void MatchRequests(const BackgroundFetchRegistrationId& registration_id,
                     blink::mojom::FetchAPIRequestPtr request_to_match,
                     blink::mojom::CacheQueryOptionsPtr cache_query_options,
                     bool match_all,
                     blink::mojom::BackgroundFetchError* out_error,
                     std::vector<blink::mojom::BackgroundFetchSettledFetchPtr>*
                         out_settled_fetches) {
    DCHECK(out_error);
    DCHECK(out_settled_fetches);

    base::RunLoop run_loop;
    auto match_params = std::make_unique<BackgroundFetchRequestMatchParams>(
        std::move(request_to_match), std::move(cache_query_options), match_all);
    background_fetch_data_manager_->MatchRequests(
        registration_id, std::move(match_params),
        base::BindOnce(&BackgroundFetchDataManagerTest::DidMatchRequests,
                       base::Unretained(this), run_loop.QuitClosure(),
                       out_error, out_settled_fetches));
    run_loop.Run();
  }

  // Synchronous version of
  // ServiceWorkerContextWrapper::GetRegistrationUserDataByKeyPrefix.
  std::vector<std::string> GetRegistrationUserDataByKeyPrefix(
      int64_t service_worker_registration_id,
      const std::string& key_prefix) {
    std::vector<std::string> data;

    base::RunLoop run_loop;
    embedded_worker_test_helper()
        ->context_wrapper()
        ->GetRegistrationUserDataByKeyPrefix(
            service_worker_registration_id, key_prefix,
            base::BindOnce(&DidGetRegistrationUserDataByKeyPrefix,
                           run_loop.QuitClosure(), &data));
    run_loop.Run();

    return data;
  }

  // Synchronously writes data to the SW DB.
  void StoreUserData(int64_t service_worker_registration_id,
                     const std::string& key,
                     const std::string& value) {
    std::vector<std::string> data;

    base::RunLoop run_loop;
    embedded_worker_test_helper()->context_wrapper()->StoreRegistrationUserData(
        service_worker_registration_id, storage_key(), {{key, value}},
        base::BindOnce(&DidStoreUserData, run_loop.QuitClosure()));
    run_loop.Run();
  }

  // Synchronous version of CacheStorageManager::HasCache().
  bool HasCache(const std::string& cache_name) {
    bool result = false;

    base::RunLoop run_loop;
    background_fetch_data_manager_->HasCache(
        storage_key(), cache_name,
        /* trace_id= */ 0,
        base::BindLambdaForTesting([&](blink::mojom::CacheStorageError error) {
          result = error == blink::mojom::CacheStorageError::kSuccess;
          run_loop.Quit();
        }));

    run_loop.Run();

    return result;
  }

  // Synchronous version of CacheStorageManager::MatchCache().
  bool MatchCache(const blink::mojom::FetchAPIRequestPtr& request) {
    bool match_result = false;
    constexpr int64_t trace_id = 0;

    mojo::AssociatedRemote<blink::mojom::CacheStorageCache> cache;
    base::RunLoop run_loop;
    background_fetch_data_manager_->OpenCache(
        storage_key(), kExampleUniqueId, trace_id,
        base::BindLambdaForTesting([&](blink::mojom::OpenResultPtr result) {
          EXPECT_FALSE(result->is_status());

          auto match_options = blink::mojom::CacheQueryOptions::New();
          match_options->ignore_search = true;

          cache.Bind(std::move(result->get_cache()));
          cache->Match(
              BackgroundFetchSettledFetch::CloneRequest(request),
              std::move(match_options),
              /*in_related_fetch_event=*/false, /*in_range_fetch_event=*/false,
              trace_id,
              base::BindOnce(&BackgroundFetchDataManagerTest::DidMatchCache,
                             base::Unretained(this), run_loop.QuitClosure(),
                             &match_result));
        }));
    run_loop.Run();

    return match_result;
  }

  void DeleteFromCache(const blink::mojom::FetchAPIRequestPtr& request) {
    mojo::AssociatedRemote<blink::mojom::CacheStorageCache> cache;
    base::RunLoop run_loop;
    background_fetch_data_manager_->OpenCache(
        storage_key(),
        /* unique_id= */ kExampleUniqueId,
        /* trace_id= */ 0,
        base::BindLambdaForTesting([&](blink::mojom::OpenResultPtr result) {
          EXPECT_TRUE(result->is_cache());

          std::vector<blink::mojom::BatchOperationPtr> operation_ptr_vec;
          operation_ptr_vec.push_back(blink::mojom::BatchOperation::New());
          operation_ptr_vec[0]->operation_type =
              blink::mojom::OperationType::kDelete;
          operation_ptr_vec[0]->request =
              BackgroundFetchSettledFetch::CloneRequest(request);
          operation_ptr_vec[0]->match_options =
              blink::mojom::CacheQueryOptions::New();
          operation_ptr_vec[0]->match_options->ignore_search = true;

          cache.Bind(std::move(result->get_cache()));
          cache->Batch(
              std::move(operation_ptr_vec), /* trace_id= */ 0,
              base::BindOnce(&BackgroundFetchDataManagerTest::DidBatchOperation,
                             base::Unretained(this), run_loop.QuitClosure()));
        }));
    run_loop.Run();
  }

  void PutInCache(const blink::mojom::FetchAPIRequestPtr& request,
                  blink::mojom::FetchAPIResponsePtr response) {
    mojo::AssociatedRemote<blink::mojom::CacheStorageCache> cache;
    base::RunLoop run_loop;
    background_fetch_data_manager_->OpenCache(
        storage_key(),
        /* unique_id= */ kExampleUniqueId,
        /* trace_id= */ 0,
        base::BindLambdaForTesting([&](blink::mojom::OpenResultPtr result) {
          EXPECT_TRUE(result->is_cache());

          std::vector<blink::mojom::BatchOperationPtr> operation_ptr_vec;
          operation_ptr_vec.push_back(blink::mojom::BatchOperation::New());
          operation_ptr_vec[0]->operation_type =
              blink::mojom::OperationType::kPut;
          operation_ptr_vec[0]->request =
              BackgroundFetchSettledFetch::CloneRequest(request);
          operation_ptr_vec[0]->response = std::move(response);

          cache.Bind(std::move(result->get_cache()));
          cache->Batch(
              std::move(operation_ptr_vec), /* trace_id= */ 0,
              base::BindOnce(&BackgroundFetchDataManagerTest::DidBatchOperation,
                             base::Unretained(this), run_loop.QuitClosure()));
        }));
    run_loop.Run();
  }

  // Returns the title and the icon.
  std::pair<std::string, SkBitmap> GetUIOptions(
      int64_t service_worker_registration_id) {
    auto results = GetRegistrationUserDataByKeyPrefix(
        service_worker_registration_id, background_fetch::kUIOptionsKeyPrefix);
    DCHECK_LT(results.size(), 2u)
        << "Using GetUIOptions with multiple registrations is unimplemented";

    proto::BackgroundFetchUIOptions ui_options;
    if (results.empty())
      return {"", SkBitmap()};

    bool did_parse = ui_options.ParseFromString(results[0]);
    DCHECK(did_parse);

    std::pair<std::string, SkBitmap> result{ui_options.title(), SkBitmap()};

    if (ui_options.icon().empty())
      return result;

    // Deserialize icon.
    {
      base::RunLoop run_loop;
      background_fetch::DeserializeIcon(
          std::unique_ptr<std::string>(ui_options.release_icon()),
          base::BindOnce(
              [](base::OnceClosure quit_closure, SkBitmap* out_icon,
                 SkBitmap icon) {
                DCHECK(out_icon);
                *out_icon = std::move(icon);
                std::move(quit_closure).Run();
              },
              run_loop.QuitClosure(), &result.second));
      run_loop.Run();
    }

    return result;
  }

  // Gets information about the number of background fetch requests by state.
  ResponseStateStats GetRequestStats(int64_t service_worker_registration_id) {
    ResponseStateStats stats;
    {
      base::RunLoop run_loop;
      embedded_worker_test_helper()
          ->context_wrapper()
          ->GetRegistrationUserDataByKeyPrefix(
              service_worker_registration_id,
              background_fetch::kPendingRequestKeyPrefix,
              base::BindOnce(&GetNumUserData, run_loop.QuitClosure(),
                             &stats.pending_requests));
      run_loop.Run();
    }
    {
      base::RunLoop run_loop;
      embedded_worker_test_helper()
          ->context_wrapper()
          ->GetRegistrationUserDataByKeyPrefix(
              service_worker_registration_id,
              background_fetch::kActiveRequestKeyPrefix,
              base::BindOnce(&GetNumUserData, run_loop.QuitClosure(),
                             &stats.active_requests));
      run_loop.Run();
    }
    {
      base::RunLoop run_loop;
      embedded_worker_test_helper()
          ->context_wrapper()
          ->GetRegistrationUserDataByKeyPrefix(
              service_worker_registration_id,
              background_fetch::kCompletedRequestKeyPrefix,
              base::BindOnce(&GetNumUserData, run_loop.QuitClosure(),
                             &stats.completed_requests));
      run_loop.Run();
    }
    return stats;
  }

  void AnnotateRequestInfoWithFakeDownloadManagerData(
      BackgroundFetchRequestInfo* request_info,
      bool success = false,
      bool over_quota = false) {
    DCHECK(request_info);

    std::string headers =
        success ? "HTTP/1.1 200 OK\n" : "HTTP/1.1 404 Not found\n";
    auto response = std::make_unique<BackgroundFetchResponse>(
        std::vector<GURL>(1u, request_info->fetch_request()->url),
        base::MakeRefCounted<net::HttpResponseHeaders>(headers));

    if (!success) {
      // Fill |request_info| with a failed result.
      request_info->SetResult(std::make_unique<BackgroundFetchResult>(
          std::move(response), base::Time::Now(),
          BackgroundFetchResult::FailureReason::FETCH_ERROR));
      return;
    }

    std::string response_data(
        over_quota ? kBackgroundFetchMaxQuotaBytes + 1 : kResponseSize, 'x');
    auto blob_builder = std::make_unique<storage::BlobDataBuilder>(
        base::Uuid::GenerateRandomV4().AsLowercaseString());
    blob_builder->AppendData(response_data);
    auto handle = background_fetch_data_manager_->blob_storage_context()
                      ->context()
                      ->AddFinishedBlob(std::move(blob_builder));
    request_info->SetResult(std::make_unique<BackgroundFetchResult>(
        std::move(response), base::Time::Now(), base::FilePath(),
        std::move(*handle), /* file_size= */ 0u));
  }

  // BackgroundFetchDataManagerObserver mocks:
  MOCK_METHOD7(OnRegistrationCreated,
               void(const BackgroundFetchRegistrationId& registration_id,
                    const blink::mojom::BackgroundFetchRegistrationData&
                        registration_data,
                    blink::mojom::BackgroundFetchOptionsPtr options,
                    const SkBitmap& icon,
                    int num_requests,
                    bool start_paused,
                    net::IsolationInfo isolation_info));
  MOCK_METHOD8(OnRegistrationLoadedAtStartup,
               void(const BackgroundFetchRegistrationId& registration_id,
                    const blink::mojom::BackgroundFetchRegistrationData&
                        registration_data,
                    blink::mojom::BackgroundFetchOptionsPtr options,
                    const SkBitmap& icon,
                    int num_completed_requests,
                    int num_requests,
                    std::vector<scoped_refptr<BackgroundFetchRequestInfo>>
                        active_fetch_requests,
                    std::optional<net::IsolationInfo> isolation_info));
  MOCK_METHOD2(
      OnRegistrationQueried,
      void(const BackgroundFetchRegistrationId& registration_id,
           blink::mojom::BackgroundFetchRegistrationData* registration_data));
  MOCK_METHOD1(OnServiceWorkerDatabaseCorrupted,
               void(int64_t service_worker_registration_id));
  MOCK_METHOD3(OnRequestCompleted,
               void(const std::string& unique_id,
                    blink::mojom::FetchAPIRequestPtr request,
                    blink::mojom::FetchAPIResponsePtr response));

 protected:
  void DidGetRegistration(
      base::OnceClosure quit_closure,
      blink::mojom::BackgroundFetchError* out_error,
      blink::mojom::BackgroundFetchRegistrationData* out_registration_data,
      blink::mojom::BackgroundFetchError error,
      BackgroundFetchRegistrationId registration_id,
      blink::mojom::BackgroundFetchRegistrationDataPtr registration_data) {
    *out_error = error;
    *out_registration_data = std::move(*registration_data);

    std::move(quit_closure).Run();
  }

  void DidGetMetadata(
      base::OnceClosure quit_closure,
      std::unique_ptr<proto::BackgroundFetchMetadata>* out_metadata,
      const std::vector<std::string>& data,
      blink::ServiceWorkerStatusCode status) {
    if (status == blink::ServiceWorkerStatusCode::kOk) {
      DCHECK_EQ(data.size(), 1u);

      auto metadata = std::make_unique<proto::BackgroundFetchMetadata>();
      if (metadata->ParseFromString(data[0]))
        *out_metadata = std::move(metadata);
    }

    std::move(quit_closure).Run();
  }

  void DidGetDeveloperIds(base::OnceClosure quit_closure,
                          blink::mojom::BackgroundFetchError* out_error,
                          std::vector<std::string>* out_ids,
                          blink::mojom::BackgroundFetchError error,
                          const std::vector<std::string>& ids) {
    *out_error = error;
    *out_ids = ids;

    std::move(quit_closure).Run();
  }

  void DidPopNextRequest(
      base::OnceClosure quit_closure,
      blink::mojom::BackgroundFetchError* out_error,
      scoped_refptr<BackgroundFetchRequestInfo>* out_request_info,
      blink::mojom::BackgroundFetchError error,
      scoped_refptr<BackgroundFetchRequestInfo> request_info) {
    *out_error = error;
    *out_request_info = request_info;
    std::move(quit_closure).Run();
  }

  void DidGetRequestBlob(base::OnceClosure quit_closure,
                         blink::mojom::BackgroundFetchError* out_error,
                         blink::mojom::SerializedBlobPtr* out_blob,
                         blink::mojom::BackgroundFetchError error,
                         blink::mojom::SerializedBlobPtr blob) {
    *out_error = error;
    *out_blob = std::move(blob);
    std::move(quit_closure).Run();
  }

  void DidMarkRequestAsComplete(base::OnceClosure quit_closure,
                                blink::mojom::BackgroundFetchError* out_error,
                                blink::mojom::BackgroundFetchError error) {
    *out_error = error;
    std::move(quit_closure).Run();
  }

  void DidMarkRegistrationForDeletion(
      base::OnceClosure quit_closure,
      blink::mojom::BackgroundFetchError* out_error,
      blink::mojom::BackgroundFetchFailureReason* out_failure_reason,
      blink::mojom::BackgroundFetchError error,
      blink::mojom::BackgroundFetchFailureReason failure_reason) {
    *out_error = error;
    *out_failure_reason = failure_reason;
    std::move(quit_closure).Run();
  }

  void DidMatchRequests(
      base::OnceClosure quit_closure,
      blink::mojom::BackgroundFetchError* out_error,
      std::vector<blink::mojom::BackgroundFetchSettledFetchPtr>*
          out_settled_fetches,
      blink::mojom::BackgroundFetchError error,
      std::vector<blink::mojom::BackgroundFetchSettledFetchPtr>
          settled_fetches) {
    *out_error = error;
    *out_settled_fetches = std::move(settled_fetches);
    std::move(quit_closure).Run();
  }

  void DidMatchCache(base::OnceClosure quit_closure,
                     bool* out_result,
                     blink::mojom::MatchResultPtr result) {
    *out_result = false;

    // This counts as matched if an entry was found in the cache which
    // also has a non-empty response.
    if (result->is_eager_response()) {
      auto& response = result->get_eager_response()->response;
      *out_result = !response.is_null() && !response->url_list.empty();
    } else if (result->is_response()) {
      auto& response = result->get_response();
      *out_result = !response.is_null() && !response->url_list.empty();
    }
    std::move(quit_closure).Run();
  }

  void DidBatchOperation(base::OnceClosure quit_closure,
                         blink::mojom::CacheStorageVerboseErrorPtr error) {
    DCHECK_EQ(error->value, blink::mojom::CacheStorageError::kSuccess);
    std::move(quit_closure).Run();
  }

  blink::mojom::SerializedBlobPtr BuildBlob(const std::string data) {
    auto blob_data = std::make_unique<storage::BlobDataBuilder>(
        "blob-id:" + base::Uuid::GenerateRandomV4().AsLowercaseString());
    blob_data->AppendData(data);
    std::unique_ptr<storage::BlobDataHandle> blob_handle =
        background_fetch_data_manager_->blob_storage_context_->context()
            ->AddFinishedBlob(std::move(blob_data));

    auto blob = blink::mojom::SerializedBlob::New();
    blob->uuid = blob_handle->uuid();
    blob->size = blob_handle->size();
    storage::BlobImpl::Create(
        std::make_unique<storage::BlobDataHandle>(*blob_handle),
        blob->blob.InitWithNewPipeAndPassReceiver());
    return blob;
  }

  std::unique_ptr<BackgroundFetchTestDataManager>
      background_fetch_data_manager_;
};

TEST_F(BackgroundFetchDataManagerTest, NoDuplicateRegistrations) {
  // Tests that the BackgroundFetchDataManager correctly rejects creating a
  // registration with a |developer_id| for which there is already an active
  // registration.

  int64_t service_worker_registration_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId,
            service_worker_registration_id);

  BackgroundFetchRegistrationId registration_id1(
      service_worker_registration_id, storage_key(), kExampleDeveloperId,
      kExampleUniqueId);

  std::vector<blink::mojom::FetchAPIRequestPtr> requests =
      CreateValidRequests(storage_key().origin());
  auto options = blink::mojom::BackgroundFetchOptions::New();

  blink::mojom::BackgroundFetchError error;
  blink::mojom::BackgroundFetchFailureReason failure_reason;

  // Deactivating the not-yet-created registration should fail.
  MarkRegistrationForDeletion(registration_id1, /* check_for_failure= */ true,
                              &error, &failure_reason);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::INVALID_ID);

  // Creating the initial registration should succeed.
  {
    EXPECT_CALL(*this,
                OnRegistrationCreated(registration_id1, _, _, _, _, _, _));

    CreateRegistration(registration_id1, CloneRequestVector(requests),
                       options.Clone(), SkBitmap(), &error);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  }

  // Different |unique_id|, since this is a new Background Fetch registration,
  // even though it shares the same |developer_id|.
  BackgroundFetchRegistrationId registration_id2(
      service_worker_registration_id, storage_key(), kExampleDeveloperId,
      kAlternativeUniqueId);

  // Attempting to create a second registration with the same |developer_id| and
  // |service_worker_registration_id| should yield an error.
  CreateRegistration(registration_id2, CloneRequestVector(requests),
                     options.Clone(), SkBitmap(), &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::DUPLICATED_DEVELOPER_ID);

  // Deactivating the second registration that failed to be created should fail.
  MarkRegistrationForDeletion(registration_id2, /* check_for_failure= */ true,
                              &error, &failure_reason);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::INVALID_ID);

  // Deactivating the initial registration should succeed.
  MarkRegistrationForDeletion(registration_id1, /* check_for_failure= */ true,
                              &error, &failure_reason);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

  // And now registering the second registration should work fine, since there
  // is no longer an *active* registration with the same |developer_id|, even
  // though the initial registration has not yet been deleted.
  {
    EXPECT_CALL(*this,
                OnRegistrationCreated(registration_id2, _, _, _, _, _, _));

    CreateRegistration(registration_id2, CloneRequestVector(requests),
                       std::move(options), SkBitmap(), &error);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  }
}

TEST_F(BackgroundFetchDataManagerTest, ExceedingQuotaFailsCreation) {
  // Tests that the BackgroundFetchDataManager correctly rejects creating a
  // registration where the provided download total exceeds the available quota.

  int64_t service_worker_registration_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId,
            service_worker_registration_id);

  BackgroundFetchRegistrationId registration_id(
      service_worker_registration_id, storage_key(), kExampleDeveloperId,
      kExampleUniqueId);
  std::vector<blink::mojom::FetchAPIRequestPtr> requests =
      CreateValidRequests(storage_key().origin());
  auto options = blink::mojom::BackgroundFetchOptions::New();
  options->download_total = kBackgroundFetchMaxQuotaBytes + 1;

  blink::mojom::BackgroundFetchError error;

  CreateRegistration(registration_id, std::move(requests), std::move(options),
                     SkBitmap(), &error);
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::QUOTA_EXCEEDED);
}

TEST_F(BackgroundFetchDataManagerTest, RegistrationLimitIsEnforced) {
  // Tests that the BackgroundFetchDataManager correctly rejects creating a
  // registration when an origin exceeds the allowed number of registrations.
  int64_t swid1 = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, swid1);
  int64_t swid2 = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, swid2);

  ASSERT_NE(swid1, swid2);

  blink::mojom::BackgroundFetchError error;

  // Create two registrations for every Service Worker.
  for (int i = 0; i < 2; i++) {
    // First Service Worker.
    BackgroundFetchRegistrationId registration_id1(
        swid1, storage_key(), kExampleDeveloperId + base::NumberToString(i),
        base::Uuid::GenerateRandomV4().AsLowercaseString());
    CreateRegistration(
        registration_id1, std::vector<blink::mojom::FetchAPIRequestPtr>(),
        blink::mojom::BackgroundFetchOptions::New(), SkBitmap(), &error);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

    // Second service Worker.
    BackgroundFetchRegistrationId registration_id2(
        swid2, storage_key(), kExampleDeveloperId + base::NumberToString(i),
        base::Uuid::GenerateRandomV4().AsLowercaseString());
    CreateRegistration(
        registration_id2, std::vector<blink::mojom::FetchAPIRequestPtr>(),
        blink::mojom::BackgroundFetchOptions::New(), SkBitmap(), &error);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  }

  // Create another registration in the first Service Worker,
  // bringing us to the limit.
  {
    BackgroundFetchRegistrationId registration_id(
        swid1, storage_key(), "developer_id1",
        base::Uuid::GenerateRandomV4().AsLowercaseString());
    CreateRegistration(
        registration_id, std::vector<blink::mojom::FetchAPIRequestPtr>(),
        blink::mojom::BackgroundFetchOptions::New(), SkBitmap(), &error);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  }

  // A registration this time should fail.
  {
    BackgroundFetchRegistrationId registration_id(
        swid1, storage_key(), "developer_id2",
        base::Uuid::GenerateRandomV4().AsLowercaseString());
    CreateRegistration(
        registration_id, std::vector<blink::mojom::FetchAPIRequestPtr>(),
        blink::mojom::BackgroundFetchOptions::New(), SkBitmap(), &error);
    ASSERT_EQ(error,
              blink::mojom::BackgroundFetchError::REGISTRATION_LIMIT_EXCEEDED);
  }

  // The registration should also fail for the other Service Worker.
  {
    BackgroundFetchRegistrationId registration_id(
        swid2, storage_key(), "developer_id3",
        base::Uuid::GenerateRandomV4().AsLowercaseString());
    CreateRegistration(
        registration_id, std::vector<blink::mojom::FetchAPIRequestPtr>(),
        blink::mojom::BackgroundFetchOptions::New(), SkBitmap(), &error);
    ASSERT_EQ(error,
              blink::mojom::BackgroundFetchError::REGISTRATION_LIMIT_EXCEEDED);
  }
}

TEST_F(BackgroundFetchDataManagerTest, GetDeveloperIds) {
  int64_t sw_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, sw_id);

  std::vector<blink::mojom::FetchAPIRequestPtr> requests =
      CreateValidRequests(storage_key().origin(), 2u);
  auto options = blink::mojom::BackgroundFetchOptions::New();
  blink::mojom::BackgroundFetchError error;

  // Verify that no developer IDs can be found.
  auto developer_ids = GetDeveloperIds(sw_id, storage_key(), &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  EXPECT_THAT(developer_ids, IsEmpty());

  // Create a single registration.
  BackgroundFetchRegistrationId registration_id1(
      sw_id, storage_key(), kExampleDeveloperId, kExampleUniqueId);
  {
    EXPECT_CALL(*this,
                OnRegistrationCreated(registration_id1, _, _, _, _, _, _));

    CreateRegistration(registration_id1, CloneRequestVector(requests),
                       options.Clone(), SkBitmap(), &error);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  }

  // Verify that the developer ID can be found.
  developer_ids = GetDeveloperIds(sw_id, storage_key(), &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  EXPECT_THAT(developer_ids, UnorderedElementsAre(kExampleDeveloperId));

  RestartDataManagerFromPersistentStorage();

  // After a restart, GetDeveloperIds should still find the IDs.
  developer_ids = GetDeveloperIds(sw_id, storage_key(), &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  EXPECT_THAT(developer_ids, UnorderedElementsAre(kExampleDeveloperId));

  // Create another registration.
  BackgroundFetchRegistrationId registration_id2(
      sw_id, storage_key(), kAlternativeDeveloperId, kAlternativeUniqueId);
  {
    EXPECT_CALL(*this,
                OnRegistrationCreated(registration_id2, _, _, _, _, _, _));

    CreateRegistration(registration_id2, CloneRequestVector(requests),
                       std::move(options), SkBitmap(), &error);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  }
  // Verify that both developer IDs can be found.
  developer_ids = GetDeveloperIds(sw_id, storage_key(), &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  EXPECT_THAT(developer_ids, UnorderedElementsAre(kExampleDeveloperId,
                                                  kAlternativeDeveloperId));
  RestartDataManagerFromPersistentStorage();

  // After a restart, GetDeveloperIds should still find the IDs.
  developer_ids = GetDeveloperIds(sw_id, storage_key(), &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  EXPECT_THAT(developer_ids, UnorderedElementsAre(kExampleDeveloperId,
                                                  kAlternativeDeveloperId));
}

TEST_F(BackgroundFetchDataManagerTest, StorageVersionIsPersisted) {
  int64_t sw_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, sw_id);

  {
    // Create a single registration.
    BackgroundFetchRegistrationId registration_id(
        sw_id, storage_key(), kExampleDeveloperId, kExampleUniqueId);
    EXPECT_CALL(*this,
                OnRegistrationCreated(registration_id, _, _, _, _, _, _));

    std::vector<blink::mojom::FetchAPIRequestPtr> requests =
        CreateValidRequests(storage_key().origin(), 2u);
    auto options = blink::mojom::BackgroundFetchOptions::New();
    blink::mojom::BackgroundFetchError error;
    CreateRegistration(registration_id, CloneRequestVector(requests),
                       options.Clone(), SkBitmap(), &error);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  }

  auto storage_versions = GetRegistrationUserDataByKeyPrefix(
      sw_id, background_fetch::StorageVersionKey(kExampleUniqueId));
  ASSERT_EQ(storage_versions.size(), 1u);
  EXPECT_EQ(storage_versions[0], base::NumberToString(proto::SV_CURRENT));
}

TEST_F(BackgroundFetchDataManagerTest, GetRegistration) {
  int64_t sw_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, sw_id);

  BackgroundFetchRegistrationId registration_id(
      sw_id, storage_key(), kExampleDeveloperId, kExampleUniqueId);

  std::vector<blink::mojom::FetchAPIRequestPtr> requests =
      CreateValidRequests(storage_key().origin(), 2u);
  auto options = blink::mojom::BackgroundFetchOptions::New();
  blink::mojom::BackgroundFetchError error;

  // Create a single registration.
  {
    EXPECT_CALL(*this,
                OnRegistrationCreated(registration_id, _, _, _, _, _, _));

    CreateRegistration(registration_id, std::move(requests), std::move(options),
                       SkBitmap(), &error);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  }

  // Verify that the registration can be retrieved.
  auto registration =
      GetRegistration(sw_id, storage_key(), kExampleDeveloperId, &error);
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

  EXPECT_EQ(kExampleDeveloperId, registration->developer_id);
  EXPECT_EQ(0u, registration->upload_total);

  // Verify that retrieving using the wrong developer id doesn't work.
  registration =
      GetRegistration(sw_id, storage_key(), kAlternativeDeveloperId, &error);
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::INVALID_ID);

  RestartDataManagerFromPersistentStorage();

  // After a restart, GetRegistration should still find the registration.
  registration =
      GetRegistration(sw_id, storage_key(), kExampleDeveloperId, &error);
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  EXPECT_EQ(kExampleDeveloperId, registration->developer_id);
}

TEST_F(BackgroundFetchDataManagerTest, GetMetadata) {
  int64_t sw_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, sw_id);

  BackgroundFetchRegistrationId registration_id(
      sw_id, storage_key(), kExampleDeveloperId, kExampleUniqueId);

  size_t num_requests = 2u;
  std::vector<blink::mojom::FetchAPIRequestPtr> requests =
      CreateValidRequests(storage_key().origin(), num_requests);
  auto options = blink::mojom::BackgroundFetchOptions::New();
  blink::mojom::BackgroundFetchError error;

  // Create a single registration.
  {
    EXPECT_CALL(*this,
                OnRegistrationCreated(registration_id, _, _, _, _, _, _));

    CreateRegistration(registration_id, std::move(requests), std::move(options),
                       SkBitmap(), &error);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  }
  // Verify that the metadata can be retrieved.
  auto metadata = GetMetadata(sw_id, kExampleUniqueId);
  ASSERT_TRUE(metadata);
  EXPECT_EQ(metadata->storage_key(), storage_key().Serialize());
  EXPECT_NE(metadata->creation_microseconds_since_unix_epoch(), 0);
  EXPECT_EQ(metadata->num_fetches(), static_cast<int>(num_requests));

  RestartDataManagerFromPersistentStorage();

  // After a restart, GetMetadata should still find the registration.
  metadata = GetMetadata(sw_id, kExampleUniqueId);
  ASSERT_TRUE(metadata);
  EXPECT_EQ(metadata->storage_key(), storage_key().Serialize());
  EXPECT_NE(metadata->creation_microseconds_since_unix_epoch(), 0);
  EXPECT_EQ(metadata->num_fetches(), static_cast<int>(num_requests));
}

TEST_F(BackgroundFetchDataManagerTest, RegistrationUploadInfo) {
  int64_t sw_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, sw_id);

  BackgroundFetchRegistrationId registration_id(
      sw_id, storage_key(), kExampleDeveloperId, kExampleUniqueId);
  auto options = blink::mojom::BackgroundFetchOptions::New();
  blink::mojom::BackgroundFetchError error;

  const std::string upload_data = "Upload!";
  // Create a single registration.
  {
    // One upload and one download.
    std::vector<blink::mojom::FetchAPIRequestPtr> requests =
        CreateValidRequests(storage_key().origin(), 2u);
    requests[0]->blob = BuildBlob(upload_data);
    EXPECT_CALL(*this,
                OnRegistrationCreated(registration_id, _, _, _, _, _, _));

    CreateRegistration(registration_id, std::move(requests), std::move(options),
                       SkBitmap(), &error);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  }

  auto registration =
      GetRegistration(sw_id, storage_key(), kExampleDeveloperId, &error);
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

  EXPECT_EQ(registration->developer_id, kExampleDeveloperId);
  EXPECT_EQ(registration->upload_total, upload_data.size());
}

TEST_F(BackgroundFetchDataManagerTest, LargeIconNotPersisted) {
  int64_t sw_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, sw_id);

  BackgroundFetchRegistrationId registration_id(
      sw_id, storage_key(), kExampleDeveloperId, kExampleUniqueId);

  size_t num_requests = 2u;
  std::vector<blink::mojom::FetchAPIRequestPtr> requests =
      CreateValidRequests(storage_key().origin(), num_requests);
  auto options = blink::mojom::BackgroundFetchOptions::New();
  blink::mojom::BackgroundFetchError error;

  SkBitmap icon = CreateTestIcon(/* size= */ 512);
  ASSERT_FALSE(background_fetch::ShouldPersistIcon(icon));

  // Create a single registration.
  {
    EXPECT_CALL(*this,
                OnRegistrationCreated(registration_id, _, _, _, _, _, _));

    CreateRegistration(registration_id, std::move(requests), std::move(options),
                       icon, &error);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  }

  // Verify that the metadata can be retrieved.
  auto metadata = GetMetadata(sw_id, kExampleUniqueId);
  ASSERT_TRUE(metadata);
  EXPECT_EQ(metadata->storage_key(), storage_key().Serialize());
  EXPECT_NE(metadata->creation_microseconds_since_unix_epoch(), 0);
  EXPECT_EQ(metadata->num_fetches(), static_cast<int>(num_requests));
  EXPECT_TRUE(GetUIOptions(sw_id).second.isNull());
}

TEST_F(BackgroundFetchDataManagerTest, CreateAndDeleteRegistration) {
  int64_t sw_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, sw_id);

  BackgroundFetchRegistrationId registration_id1(
      sw_id, storage_key(), kExampleDeveloperId, kExampleUniqueId);

  std::vector<blink::mojom::FetchAPIRequestPtr> requests =
      CreateValidRequests(storage_key().origin(), 2u);
  auto options = blink::mojom::BackgroundFetchOptions::New();
  blink::mojom::BackgroundFetchError error;
  blink::mojom::BackgroundFetchFailureReason failure_reason;

  {
    EXPECT_CALL(*this,
                OnRegistrationCreated(registration_id1, _, _, _, _, _, _));

    CreateRegistration(registration_id1, CloneRequestVector(requests),
                       options.Clone(), SkBitmap(), &error);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  }

  RestartDataManagerFromPersistentStorage();

  // Different |unique_id|, since this is a new Background Fetch registration,
  // even though it shares the same |developer_id|.
  BackgroundFetchRegistrationId registration_id2(
      sw_id, storage_key(), kExampleDeveloperId, kAlternativeUniqueId);

  // Attempting to create a second registration with the same |developer_id| and
  // |service_worker_registration_id| should yield an error, even after
  // restarting.
  CreateRegistration(registration_id2, CloneRequestVector(requests),
                     options.Clone(), SkBitmap(), &error);
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::DUPLICATED_DEVELOPER_ID);

  // Verify that the registration can be retrieved before deletion.
  auto registration =
      GetRegistration(sw_id, storage_key(), kExampleDeveloperId, &error);
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  EXPECT_EQ(kExampleDeveloperId, registration->developer_id);

  // Deactivating the registration should succeed.
  MarkRegistrationForDeletion(registration_id1, /* check_for_failure= */ true,
                              &error, &failure_reason);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  EXPECT_EQ(failure_reason, blink::mojom::BackgroundFetchFailureReason::NONE);

  // Verify that the registration cannot be retrieved after deletion
  registration =
      GetRegistration(sw_id, storage_key(), kExampleDeveloperId, &error);
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::INVALID_ID);

  RestartDataManagerFromPersistentStorage();

  // Verify again that the registration cannot be retrieved after deletion and
  // a restart.
  registration =
      GetRegistration(sw_id, storage_key(), kExampleDeveloperId, &error);
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::INVALID_ID);

  // And now registering the second registration should work fine, even after
  // restarting, since there is no longer an *active* registration with the same
  // |developer_id|, even though the initial registration has not yet been
  // deleted.
  {
    EXPECT_CALL(*this,
                OnRegistrationCreated(registration_id2, _, _, _, _, _, _));

    CreateRegistration(registration_id2, CloneRequestVector(requests),
                       std::move(options), SkBitmap(), &error);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  }

  RestartDataManagerFromPersistentStorage();

  // Deleting the inactive first registration should succeed.
  DeleteRegistration(registration_id1, &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
}

TEST_F(BackgroundFetchDataManagerTest, MarkRegistrationForDeletion) {
  int64_t sw_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, sw_id);

  BackgroundFetchRegistrationId registration_id1(
      sw_id, storage_key(), kExampleDeveloperId, kExampleUniqueId);

  std::vector<blink::mojom::FetchAPIRequestPtr> requests =
      CreateValidRequests(storage_key().origin(), 2u);
  auto options = blink::mojom::BackgroundFetchOptions::New();
  blink::mojom::BackgroundFetchError error;
  blink::mojom::BackgroundFetchFailureReason failure_reason;

  CreateRegistration(registration_id1, CloneRequestVector(requests),
                     options.Clone(), SkBitmap(), &error);
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

  // Create a |developer_id| such that the other one is a substring.
  std::string developer_id2 = std::string(kExampleDeveloperId) + "!";
  BackgroundFetchRegistrationId registration_id2(
      sw_id, storage_key(), developer_id2, kAlternativeUniqueId);

  CreateRegistration(registration_id2, CloneRequestVector(requests),
                     std::move(options), SkBitmap(), &error);
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

  // Get all active registration mappings.
  {
    auto registrations = GetRegistrationUserDataByKeyPrefix(
        sw_id, background_fetch::ActiveRegistrationUniqueIdKey(""));
    EXPECT_EQ(registrations.size(), 2u);
  }

  // Deactivate the first registration.
  MarkRegistrationForDeletion(registration_id1, /* check_for_failure= */ true,
                              &error, &failure_reason);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  EXPECT_EQ(failure_reason, blink::mojom::BackgroundFetchFailureReason::NONE);

  // The second registration should still exist.
  {
    auto registrations = GetRegistrationUserDataByKeyPrefix(
        sw_id, background_fetch::ActiveRegistrationUniqueIdKey(""));
    ASSERT_EQ(registrations.size(), 1u);
    EXPECT_EQ(registrations[0], kAlternativeUniqueId);
  }
}

TEST_F(BackgroundFetchDataManagerTest,
       MarkRegistrationForDeletionFailureReason) {
  int64_t sw_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, sw_id);

  BackgroundFetchRegistrationId registration_id1(
      sw_id, storage_key(), kExampleDeveloperId, kExampleUniqueId);

  std::vector<blink::mojom::FetchAPIRequestPtr> requests =
      CreateValidRequests(storage_key().origin(), 1u);
  auto options = blink::mojom::BackgroundFetchOptions::New();
  blink::mojom::BackgroundFetchError error;
  blink::mojom::BackgroundFetchFailureReason failure_reason;

  // Complete the fetch successfully.
  {
    CreateRegistration(registration_id1, CloneRequestVector(requests),
                       options.Clone(), SkBitmap(), &error);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
    scoped_refptr<BackgroundFetchRequestInfo> request_info;
    PopNextRequest(registration_id1, &error, &request_info);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
    AnnotateRequestInfoWithFakeDownloadManagerData(request_info.get(),
                                                   /* succeeded= */ true);
    MarkRequestAsComplete(registration_id1, request_info.get(), &error);
    EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

    // Mark the Registration for deletion.
    MarkRegistrationForDeletion(registration_id1, /* check_for_failure= */ true,
                                &error, &failure_reason);
    EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
    EXPECT_EQ(failure_reason, blink::mojom::BackgroundFetchFailureReason::NONE);
  }

  BackgroundFetchRegistrationId registration_id2(
      sw_id, storage_key(), kAlternativeDeveloperId, kAlternativeUniqueId);

  // Complete the fetch with a BAD_STATUS.
  {
    CreateRegistration(registration_id2, CloneRequestVector(requests),
                       std::move(options), SkBitmap(), &error);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
    scoped_refptr<BackgroundFetchRequestInfo> request_info;
    PopNextRequest(registration_id2, &error, &request_info);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
    AnnotateRequestInfoWithFakeDownloadManagerData(request_info.get());
    MarkRequestAsComplete(registration_id2, request_info.get(), &error);
    EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

    // Mark the Registration for deletion.
    MarkRegistrationForDeletion(registration_id2, /* check_for_failure= */ true,
                                &error, &failure_reason);
    EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
    // The fetch resulted in a 404.
    EXPECT_EQ(failure_reason,
              blink::mojom::BackgroundFetchFailureReason::BAD_STATUS);
  }
}

TEST_F(BackgroundFetchDataManagerTest, PopNextRequestAndMarkAsComplete) {
  int64_t sw_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, sw_id);

  blink::mojom::BackgroundFetchError error;
  scoped_refptr<BackgroundFetchRequestInfo> request_info;

  BackgroundFetchRegistrationId registration_id(
      sw_id, storage_key(), kExampleDeveloperId, kExampleUniqueId);

  // There registration hasn't been created yet, so there are no pending
  // requests.
  PopNextRequest(registration_id, &error, &request_info);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  EXPECT_FALSE(request_info);
  EXPECT_EQ(
      GetRequestStats(sw_id),
      (ResponseStateStats{/* pending_requests= */ 0, /* active_requests= */ 0,
                          /* completed_requests= */ 0}));

  std::vector<blink::mojom::FetchAPIRequestPtr> requests =
      CreateValidRequests(storage_key().origin(), 2u);
  auto options = blink::mojom::BackgroundFetchOptions::New();

  {
    EXPECT_CALL(*this,
                OnRegistrationCreated(registration_id, _, _, _, _, _, _));

    CreateRegistration(registration_id, std::move(requests), std::move(options),
                       SkBitmap(), &error);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  }

  EXPECT_EQ(
      GetRequestStats(sw_id),
      (ResponseStateStats{/* pending_requests= */ 2, /* active_requests= */ 0,
                          /* completed_requests= */ 0}));

  // Popping should work now.
  PopNextRequest(registration_id, &error, &request_info);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  EXPECT_TRUE(request_info);
  EXPECT_EQ(request_info->request_index(), 0);
  EXPECT_FALSE(request_info->download_guid().empty());
  EXPECT_EQ(
      GetRequestStats(sw_id),
      (ResponseStateStats{/* pending_requests= */ 1, /* active_requests= */ 1,
                          /* completed_requests= */ 0}));

  // Mark as complete.
  AnnotateRequestInfoWithFakeDownloadManagerData(request_info.get());
  MarkRequestAsComplete(registration_id, request_info.get(), &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  ASSERT_EQ(
      GetRequestStats(sw_id),
      (ResponseStateStats{/* pending_requests= */ 1, /* active_requests= */ 0,
                          /* completed_requests= */ 1}));

  RestartDataManagerFromPersistentStorage();

  PopNextRequest(registration_id, &error, &request_info);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  EXPECT_TRUE(request_info);
  EXPECT_EQ(request_info->request_index(), 1);
  EXPECT_FALSE(request_info->download_guid().empty());
  EXPECT_EQ(
      GetRequestStats(sw_id),
      (ResponseStateStats{/* pending_requests= */ 0, /* active_requests= */ 1,
                          /* completed_requests= */ 1}));

  // Mark as complete.
  AnnotateRequestInfoWithFakeDownloadManagerData(request_info.get());
  MarkRequestAsComplete(registration_id, request_info.get(), &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  ASSERT_EQ(
      GetRequestStats(sw_id),
      (ResponseStateStats{/* pending_requests= */ 0, /* active_requests= */ 0,
                          /* completed_requests= */ 2}));

  // We are out of pending requests.
  PopNextRequest(registration_id, &error, &request_info);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  EXPECT_FALSE(request_info);
  EXPECT_EQ(
      GetRequestStats(sw_id),
      (ResponseStateStats{/* pending_requests= */ 0, /* active_requests= */ 0,
                          /* completed_requests= */ 2}));
}

TEST_F(BackgroundFetchDataManagerTest, GetUploadBody) {
  int64_t sw_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, sw_id);

  std::vector<blink::mojom::FetchAPIRequestPtr> requests;
  {
    auto request = blink::mojom::FetchAPIRequest::New();
    request->url = GURL("https://example.com/upload");
    request->method = "POST";
    request->referrer = blink::mojom::Referrer::New();
    requests.push_back(BackgroundFetchSettledFetch::CloneRequest(request));
    requests.push_back(BackgroundFetchSettledFetch::CloneRequest(request));
  }
  requests[0]->blob = BuildBlob("upload1");
  requests[1]->blob = BuildBlob("upload2");

  auto options = blink::mojom::BackgroundFetchOptions::New();
  blink::mojom::BackgroundFetchError error;

  BackgroundFetchRegistrationId registration_id(
      sw_id, storage_key(), kExampleDeveloperId, kExampleUniqueId);

  {
    EXPECT_CALL(*this,
                OnRegistrationCreated(registration_id, _, _, _, _, _, _));

    CreateRegistration(registration_id, std::move(requests), std::move(options),
                       SkBitmap(), &error);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  }

  scoped_refptr<BackgroundFetchRequestInfo> request_info1;
  PopNextRequest(registration_id, &error, &request_info1);
  ASSERT_TRUE(request_info1);
  EXPECT_EQ(request_info1->request_index(), 0);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

  scoped_refptr<BackgroundFetchRequestInfo> request_info2;
  PopNextRequest(registration_id, &error, &request_info2);
  ASSERT_TRUE(request_info2);
  EXPECT_EQ(request_info2->request_index(), 1);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

  auto payload1 =
      GetRequestBlobAsString(registration_id, request_info1, &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  EXPECT_EQ(payload1, "upload1");

  auto payload2 =
      GetRequestBlobAsString(registration_id, request_info2, &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  EXPECT_EQ(payload2, "upload2");
}

TEST_F(BackgroundFetchDataManagerTest, RegistrationBytesUpdated) {
  int64_t sw_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, sw_id);

  BackgroundFetchRegistrationId registration_id(
      sw_id, storage_key(), kExampleDeveloperId, kExampleUniqueId);
  auto requests =
      CreateValidRequests(storage_key().origin(), /* num_requests= */ 3u);

  const std::string upload_payload = "Upload Data";
  requests[0]->blob = BuildBlob(upload_payload);
  requests[1]->blob = BuildBlob(upload_payload);

  auto options = blink::mojom::BackgroundFetchOptions::New();
  blink::mojom::BackgroundFetchError error;
  {
    EXPECT_CALL(*this,
                OnRegistrationCreated(registration_id, _, _, _, _, _, _));

    CreateRegistration(registration_id, std::move(requests), std::move(options),
                       SkBitmap(), &error);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  }

  auto registration =
      GetRegistration(sw_id, storage_key(), kExampleDeveloperId, &error);
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  EXPECT_EQ(registration->downloaded, 0u);
  EXPECT_EQ(registration->uploaded, 0u);
  EXPECT_EQ(registration->upload_total, 2 * upload_payload.size());

  scoped_refptr<BackgroundFetchRequestInfo> request_info;
  PopNextRequest(registration_id, &error, &request_info);
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  AnnotateRequestInfoWithFakeDownloadManagerData(request_info.get(),
                                                 /* succeeded= */ true);
  MarkRequestAsComplete(registration_id, request_info.get(), &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

  registration =
      GetRegistration(sw_id, storage_key(), kExampleDeveloperId, &error);
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  EXPECT_EQ(registration->downloaded, kResponseSize);
  EXPECT_EQ(registration->uploaded, upload_payload.size());

  RestartDataManagerFromPersistentStorage();

  PopNextRequest(registration_id, &error, &request_info);
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  AnnotateRequestInfoWithFakeDownloadManagerData(request_info.get(),
                                                 /* succeeded= */ true);
  MarkRequestAsComplete(registration_id, request_info.get(), &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

  registration =
      GetRegistration(sw_id, storage_key(), kExampleDeveloperId, &error);
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  EXPECT_EQ(registration->downloaded, 2 * kResponseSize);
  EXPECT_EQ(registration->uploaded, 2 * upload_payload.size());

  PopNextRequest(registration_id, &error, &request_info);
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  AnnotateRequestInfoWithFakeDownloadManagerData(request_info.get(),
                                                 /* succeeded= */ false);
  MarkRequestAsComplete(registration_id, request_info.get(), &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

  registration =
      GetRegistration(sw_id, storage_key(), kExampleDeveloperId, &error);
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

  EXPECT_EQ(registration->downloaded, 2 * kResponseSize);
  EXPECT_EQ(registration->uploaded, 2 * upload_payload.size());
}

TEST_F(BackgroundFetchDataManagerTest, ExceedingQuotaIsReported) {
  int64_t sw_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, sw_id);

  BackgroundFetchRegistrationId registration_id(
      sw_id, storage_key(), kExampleDeveloperId, kExampleUniqueId);
  auto requests =
      CreateValidRequests(storage_key().origin(), /* num_requests= */ 3u);

  auto options = blink::mojom::BackgroundFetchOptions::New();
  blink::mojom::BackgroundFetchError error;
  {
    EXPECT_CALL(*this,
                OnRegistrationCreated(registration_id, _, _, _, _, _, _));

    CreateRegistration(registration_id, std::move(requests), std::move(options),
                       SkBitmap(), &error);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  }

  scoped_refptr<BackgroundFetchRequestInfo> request_info;
  PopNextRequest(registration_id, &error, &request_info);
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  AnnotateRequestInfoWithFakeDownloadManagerData(
      request_info.get(), /* succeeded= */ true, /* over_quota= */ true);
  MarkRequestAsComplete(registration_id, request_info.get(), &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::QUOTA_EXCEEDED);
}

TEST_F(BackgroundFetchDataManagerTest, WriteToCache) {
  int64_t sw_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, sw_id);

  BackgroundFetchRegistrationId registration_id(
      sw_id, storage_key(), kExampleDeveloperId, kExampleUniqueId);
  auto requests =
      CreateValidRequests(storage_key().origin(), /* num_requests= */ 2u);

  auto options = blink::mojom::BackgroundFetchOptions::New();
  blink::mojom::BackgroundFetchError error;
  {
    EXPECT_CALL(*this,
                OnRegistrationCreated(registration_id, _, _, _, _, _, _));

    CreateRegistration(registration_id, CloneRequestVector(requests),
                       std::move(options), SkBitmap(), &error);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  }

  scoped_refptr<BackgroundFetchRequestInfo> request_info;
  PopNextRequest(registration_id, &error, &request_info);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  ASSERT_TRUE(request_info);

  AnnotateRequestInfoWithFakeDownloadManagerData(request_info.get(),
                                                 /* success= */ true);
  MarkRequestAsComplete(registration_id, request_info.get(), &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

  EXPECT_TRUE(HasCache(kExampleUniqueId));
  EXPECT_FALSE(HasCache("foo"));

  EXPECT_TRUE(MatchCache(requests[0]));
  EXPECT_FALSE(MatchCache(requests[1]));

  PopNextRequest(registration_id, &error, &request_info);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  ASSERT_TRUE(request_info);

  AnnotateRequestInfoWithFakeDownloadManagerData(request_info.get(),
                                                 /* success= */ true);
  MarkRequestAsComplete(registration_id, request_info.get(), &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  EXPECT_TRUE(MatchCache(requests[0]));
  EXPECT_TRUE(MatchCache(requests[1]));

  RestartDataManagerFromPersistentStorage();
  EXPECT_TRUE(MatchCache(requests[0]));
  EXPECT_TRUE(MatchCache(requests[1]));
}

TEST_F(BackgroundFetchDataManagerTest, CacheDeleted) {
  int64_t sw_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, sw_id);

  BackgroundFetchRegistrationId registration_id(
      sw_id, storage_key(), kExampleDeveloperId, kExampleUniqueId);

  auto request = blink::mojom::FetchAPIRequest::New();
  request->referrer = blink::mojom::Referrer::New();
  request->url = GURL(storage_key().origin().GetURL().spec());

  auto options = blink::mojom::BackgroundFetchOptions::New();
  blink::mojom::BackgroundFetchError error;
  blink::mojom::BackgroundFetchFailureReason failure_reason;
  {
    EXPECT_CALL(*this,
                OnRegistrationCreated(registration_id, _, _, _, _, _, _));
    std::vector<blink::mojom::FetchAPIRequestPtr> request_vec;
    request_vec.push_back(BackgroundFetchSettledFetch::CloneRequest(request));
    CreateRegistration(registration_id, std::move(request_vec),
                       std::move(options), SkBitmap(), &error);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  }

  scoped_refptr<BackgroundFetchRequestInfo> request_info;
  PopNextRequest(registration_id, &error, &request_info);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  ASSERT_TRUE(request_info);

  AnnotateRequestInfoWithFakeDownloadManagerData(request_info.get(),
                                                 /* success= */ true);
  MarkRequestAsComplete(registration_id, request_info.get(), &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

  EXPECT_TRUE(HasCache(kExampleUniqueId));
  EXPECT_TRUE(MatchCache(request));

  MarkRegistrationForDeletion(registration_id, /* check_for_failure= */ true,
                              &error, &failure_reason);
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  DeleteRegistration(registration_id, &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

  EXPECT_FALSE(HasCache(kExampleUniqueId));
}

TEST_F(BackgroundFetchDataManagerTest, MatchRequests) {
  int64_t sw_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, sw_id);

  constexpr size_t kNumRequests = 2u;
  std::vector<blink::mojom::FetchAPIRequestPtr> requests =
      CreateValidRequests(storage_key().origin(), kNumRequests);
  auto options = blink::mojom::BackgroundFetchOptions::New();
  blink::mojom::BackgroundFetchError error;
  BackgroundFetchRegistrationId registration_id(
      sw_id, storage_key(), kExampleDeveloperId, kExampleUniqueId);

  {
    EXPECT_CALL(*this,
                OnRegistrationCreated(registration_id, _, _, _, _, _, _));

    CreateRegistration(registration_id, std::move(requests), std::move(options),
                       SkBitmap(), &error);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  }

  EXPECT_EQ(
      GetRequestStats(sw_id),
      (ResponseStateStats{/* pending_requests= */ 2, /* active_requests= */ 0,
                          /* completed_requests= */ 0}));

  // Nothing is downloaded yet.
  std::vector<blink::mojom::BackgroundFetchSettledFetchPtr> settled_fetches;
  MatchRequests(registration_id, /* request_to_match= */ nullptr,
                /* cache_query_options= */ nullptr, /* match_all= */ true,
                &error, &settled_fetches);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  EXPECT_EQ(settled_fetches.size(), kNumRequests);

  for (size_t i = 0; i < kNumRequests; i++) {
    scoped_refptr<BackgroundFetchRequestInfo> request_info;
    PopNextRequest(registration_id, &error, &request_info);
    EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
    ASSERT_TRUE(request_info);
    AnnotateRequestInfoWithFakeDownloadManagerData(request_info.get());
    MarkRequestAsComplete(registration_id, request_info.get(), &error);
    EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  }

  RestartDataManagerFromPersistentStorage();

  EXPECT_EQ(
      GetRequestStats(sw_id),
      (ResponseStateStats{/* pending_requests= */ 0, /* active_requests= */ 0,
                          /* completed_requests= */ kNumRequests}));

  MatchRequests(registration_id, /* request_to_match= */ nullptr,
                /* cache_query_options= */ nullptr, /* match_all= */ true,
                &error, &settled_fetches);

  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  // We are marking the responses as failed in Download Manager.
  EXPECT_EQ(settled_fetches.size(), kNumRequests);
}

TEST_F(BackgroundFetchDataManagerTest, MatchRequestsWithBody) {
  int64_t sw_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, sw_id);

  std::vector<blink::mojom::FetchAPIRequestPtr> requests =
      CreateValidRequests(storage_key().origin(), 2u);
  const std::string upload_data = "Upload data!";
  requests[1]->blob = BuildBlob(upload_data);

  auto options = blink::mojom::BackgroundFetchOptions::New();
  blink::mojom::BackgroundFetchError error;
  BackgroundFetchRegistrationId registration_id(
      sw_id, storage_key(), kExampleDeveloperId, kExampleUniqueId);

  {
    EXPECT_CALL(*this,
                OnRegistrationCreated(registration_id, _, _, _, _, _, _));

    CreateRegistration(registration_id, std::move(requests), std::move(options),
                       SkBitmap(), &error);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  }

  std::vector<blink::mojom::BackgroundFetchSettledFetchPtr> settled_fetches;
  MatchRequests(registration_id, /* request_to_match= */ nullptr,
                /* cache_query_options= */ nullptr, /* match_all= */ true,
                &error, &settled_fetches);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  ASSERT_EQ(settled_fetches.size(), 2u);
  EXPECT_FALSE(settled_fetches[0]->request->blob);

  auto& request = settled_fetches[1]->request;
  ASSERT_TRUE(request->blob);
  EXPECT_EQ(request->blob->size, upload_data.size());

  ASSERT_TRUE(request->blob->blob);
  mojo::Remote<blink::mojom::Blob> blob(std::move(request->blob->blob));
  EXPECT_EQ(storage::BlobToString(blob.get()), upload_data);
}

TEST_F(BackgroundFetchDataManagerTest, MatchRequestsFromCache) {
  int64_t sw_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, sw_id);

  BackgroundFetchRegistrationId registration_id(
      sw_id, storage_key(), kExampleDeveloperId, kExampleUniqueId);
  auto requests =
      CreateValidRequests(storage_key().origin(), /* num_requests= */ 2u);

  auto options = blink::mojom::BackgroundFetchOptions::New();
  blink::mojom::BackgroundFetchError error;
  {
    EXPECT_CALL(*this,
                OnRegistrationCreated(registration_id, _, _, _, _, _, _));

    CreateRegistration(registration_id, CloneRequestVector(requests),
                       std::move(options), SkBitmap(), &error);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  }

  std::vector<blink::mojom::BackgroundFetchSettledFetchPtr> settled_fetches;
  // Nothing is downloaded yet.
  MatchRequests(registration_id, /* request_to_match= */ nullptr,
                /* cache_query_options= */ nullptr, /* match_all= */ true,
                &error, &settled_fetches);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  EXPECT_EQ(settled_fetches.size(), requests.size());

  for (size_t i = 0; i < requests.size(); i++) {
    scoped_refptr<BackgroundFetchRequestInfo> request_info;
    PopNextRequest(registration_id, &error, &request_info);
    EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
    ASSERT_TRUE(request_info);
    AnnotateRequestInfoWithFakeDownloadManagerData(request_info.get(),
                                                   /* success= */ true);
    MarkRequestAsComplete(registration_id, request_info.get(), &error);
    EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  }

  RestartDataManagerFromPersistentStorage();

  MatchRequests(registration_id, /* request_to_match= */ nullptr,
                /* cache_query_options= */ nullptr, /* match_all= */ true,
                &error, &settled_fetches);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  ASSERT_EQ(settled_fetches.size(), requests.size());
  ASSERT_TRUE(settled_fetches[0]->response &&
              settled_fetches[0]->response->blob);
  ASSERT_TRUE(settled_fetches[1]->response &&
              settled_fetches[1]->response->blob);
  EXPECT_EQ(settled_fetches[0]->response->blob->size, kResponseSize);
  EXPECT_EQ(settled_fetches[1]->response->blob->size, kResponseSize);

  // Sanity check that the responses are written to / read from the cache.
  EXPECT_TRUE(MatchCache(requests[0]));
  EXPECT_TRUE(MatchCache(requests[1]));
  EXPECT_EQ(settled_fetches[0]->response->cache_storage_cache_name,
            kExampleUniqueId);
  EXPECT_EQ(settled_fetches[1]->response->cache_storage_cache_name,
            kExampleUniqueId);
}

TEST_F(BackgroundFetchDataManagerTest, MatchRequestsForASpecificRequest) {
  int64_t sw_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, sw_id);

  auto requests =
      CreateValidRequests(storage_key().origin(), /* num_requests= */ 2u);
  auto options = blink::mojom::BackgroundFetchOptions::New();
  blink::mojom::BackgroundFetchError error;
  BackgroundFetchRegistrationId registration_id(
      sw_id, storage_key(), kExampleDeveloperId, kExampleUniqueId);

  {
    EXPECT_CALL(*this,
                OnRegistrationCreated(registration_id, _, _, _, _, _, _));

    CreateRegistration(registration_id, CloneRequestVector(requests),
                       std::move(options), SkBitmap(), &error);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  }

  for (size_t i = 0; i < requests.size(); i++) {
    SCOPED_TRACE(i);
    scoped_refptr<BackgroundFetchRequestInfo> request_info;
    PopNextRequest(registration_id, &error, &request_info);
    EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
    ASSERT_TRUE(request_info);
    AnnotateRequestInfoWithFakeDownloadManagerData(request_info.get());
    MarkRequestAsComplete(registration_id, request_info.get(), &error);
    EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  }

  EXPECT_EQ(
      GetRequestStats(sw_id),
      (ResponseStateStats{/* pending_requests= */ 0, /* active_requests= */ 0,
                          /* completed_requests= */ requests.size()}));

  std::vector<blink::mojom::BackgroundFetchSettledFetchPtr> settled_fetches;
  MatchRequests(registration_id, /* request_to_match= */ std::move(requests[0]),
                /* cache_query_options= */ nullptr, /* match_all= */ false,
                &error, &settled_fetches);
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  // We are marking the responses as failed in Download Manager.
  EXPECT_EQ(settled_fetches.size(), 1u);

  // Try matching a non existing request.
  auto non_existing_request = blink::mojom::FetchAPIRequest::New();
  non_existing_request->url = GURL("https://example.com/missing-file.txt");
  MatchRequests(registration_id,
                /* request_to_match= */ std::move(non_existing_request),
                /* cache_query_options= */ nullptr, /* match_all= */ false,
                &error, &settled_fetches);
  EXPECT_TRUE(settled_fetches.empty());
}

TEST_F(BackgroundFetchDataManagerTest, MatchRequestsForAnIncompleteRequest) {
  int64_t sw_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, sw_id);

  auto requests =
      CreateValidRequests(storage_key().origin(), /* num_requests= */ 3u);
  auto options = blink::mojom::BackgroundFetchOptions::New();
  blink::mojom::BackgroundFetchError error;
  BackgroundFetchRegistrationId registration_id(
      sw_id, storage_key(), kExampleDeveloperId, kExampleUniqueId);

  {
    EXPECT_CALL(*this,
                OnRegistrationCreated(registration_id, _, _, _, _, _, _));

    CreateRegistration(registration_id, CloneRequestVector(requests),
                       std::move(options), SkBitmap(), &error);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  }

  for (size_t i = 0; i < requests.size() - 1; i++) {
    SCOPED_TRACE(i);
    scoped_refptr<BackgroundFetchRequestInfo> request_info;
    PopNextRequest(registration_id, &error, &request_info);
    EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
    ASSERT_TRUE(request_info);
    AnnotateRequestInfoWithFakeDownloadManagerData(request_info.get());
    MarkRequestAsComplete(registration_id, request_info.get(), &error);
    EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  }

  EXPECT_EQ(
      GetRequestStats(sw_id),
      (ResponseStateStats{/* pending_requests= */ 1, /* active_requests= */ 0,
                          /* completed_requests= */ requests.size() - 1}));

  std::vector<blink::mojom::BackgroundFetchSettledFetchPtr> settled_fetches;
  MatchRequests(registration_id, /* request_to_match= */ std::move(requests[2]),
                /* cache_query_options= */ nullptr, /* match_all= */ false,
                &error, &settled_fetches);
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  ASSERT_EQ(settled_fetches.size(), 1u);
  EXPECT_TRUE(settled_fetches[0]->response.is_null());
}

TEST_F(BackgroundFetchDataManagerTest, IgnoreMethodAndMatchAll) {
  int64_t sw_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, sw_id);

  std::vector<blink::mojom::FetchAPIRequestPtr> requests;
  requests.push_back(
      CreateValidRequestWithMethod(storage_key().origin(), "GET"));
  requests.push_back(
      CreateValidRequestWithMethod(storage_key().origin(), "POST"));

  auto options = blink::mojom::BackgroundFetchOptions::New();
  blink::mojom::BackgroundFetchError error;
  BackgroundFetchRegistrationId registration_id(
      sw_id, storage_key(), kExampleDeveloperId, kExampleUniqueId);

  {
    EXPECT_CALL(*this,
                OnRegistrationCreated(registration_id, _, _, _, _, _, _));

    CreateRegistration(registration_id, CloneRequestVector(requests),
                       std::move(options), SkBitmap(), &error);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  }

  for (size_t i = 0; i < requests.size(); i++) {
    SCOPED_TRACE(i);
    scoped_refptr<BackgroundFetchRequestInfo> request_info;
    PopNextRequest(registration_id, &error, &request_info);
    EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
    ASSERT_TRUE(request_info);
    AnnotateRequestInfoWithFakeDownloadManagerData(request_info.get());
    MarkRequestAsComplete(registration_id, request_info.get(), &error);
    EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  }

  EXPECT_EQ(
      GetRequestStats(sw_id),
      (ResponseStateStats{/* pending_requests= */ 0, /* active_requests= */ 0,
                          /* completed_requests= */ requests.size()}));

  std::vector<blink::mojom::BackgroundFetchSettledFetchPtr> settled_fetches;
  blink::mojom::CacheQueryOptionsPtr cache_query_options =
      blink::mojom::CacheQueryOptions::New();
  cache_query_options->ignore_method = true;
  MatchRequests(registration_id, /* request_to_match= */ std::move(requests[0]),
                std::move(cache_query_options), /* match_all= */ true, &error,
                &settled_fetches);

  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  ASSERT_EQ(settled_fetches.size(), 2u);
}

TEST_F(BackgroundFetchDataManagerTest, MatchRequestsWithDuplicates) {
  int64_t sw_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, sw_id);

  std::vector<blink::mojom::FetchAPIRequestPtr> requests;
  const std::string base_path = "https://example.com/foo";

  // Add base request.
  {
    auto request = blink::mojom::FetchAPIRequest::New();
    request->url = GURL(base_path);
    request->method = "GET";
    request->referrer = blink::mojom::Referrer::New();
    requests.push_back(BackgroundFetchSettledFetch::CloneRequest(request));
    requests.push_back(BackgroundFetchSettledFetch::CloneRequest(request));
  }

  // Add base request with a query.
  {
    auto request = blink::mojom::FetchAPIRequest::New();
    request->method = "GET";
    request->url = GURL(base_path + "?a=b");
    request->referrer = blink::mojom::Referrer::New();
    requests.push_back(BackgroundFetchSettledFetch::CloneRequest(request));
    requests.push_back(BackgroundFetchSettledFetch::CloneRequest(request));
    request->url = GURL(base_path + "?c=d");
    requests.push_back(BackgroundFetchSettledFetch::CloneRequest(request));
  }

  // Add base request with different method.
  {
    auto request = blink::mojom::FetchAPIRequest::New();
    request->url = GURL(base_path);
    request->method = "POST";
    request->referrer = blink::mojom::Referrer::New();
    requests.push_back(BackgroundFetchSettledFetch::CloneRequest(request));
    requests.push_back(BackgroundFetchSettledFetch::CloneRequest(request));
  }

  blink::mojom::BackgroundFetchError error;
  BackgroundFetchRegistrationId registration_id(
      sw_id, storage_key(), kExampleDeveloperId, kExampleUniqueId);

  {
    auto options = blink::mojom::BackgroundFetchOptions::New();
    EXPECT_CALL(*this,
                OnRegistrationCreated(registration_id, _, _, _, _, _, _));

    CreateRegistration(registration_id, CloneRequestVector(requests),
                       std::move(options), SkBitmap(), &error);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  }

  std::vector<blink::mojom::BackgroundFetchSettledFetchPtr> settled_fetches;
  auto request_to_match =
      BackgroundFetchSettledFetch::CloneRequest(requests[0]);
  auto query_options = blink::mojom::CacheQueryOptions::New();

  MatchRequests(registration_id,
                BackgroundFetchSettledFetch::CloneRequest(request_to_match),
                query_options->Clone(), /* match_all= */ true, &error,
                &settled_fetches);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  // Match only the GETs with the same url.
  EXPECT_EQ(settled_fetches.size(), 2u);

  query_options->ignore_search = true;
  MatchRequests(registration_id,
                BackgroundFetchSettledFetch::CloneRequest(request_to_match),
                query_options->Clone(), /* match_all= */ true, &error,
                &settled_fetches);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  // Match only the GETs with the same url path.
  EXPECT_EQ(settled_fetches.size(), 5u);

  query_options->ignore_method = true;
  MatchRequests(registration_id,
                BackgroundFetchSettledFetch::CloneRequest(request_to_match),
                query_options->Clone(), /* match_all= */ true, &error,
                &settled_fetches);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  // Match everything.
  EXPECT_EQ(settled_fetches.size(), requests.size());
}

TEST_F(BackgroundFetchDataManagerTest, Cleanup) {
  // Tests that the BackgroundFetchDataManager cleans up registrations
  // marked for deletion.
  int64_t sw_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, sw_id);

  BackgroundFetchRegistrationId registration_id(
      sw_id, storage_key(), kExampleDeveloperId, kExampleUniqueId);

  std::vector<blink::mojom::FetchAPIRequestPtr> requests =
      CreateValidRequests(storage_key().origin(), 2u);
  auto options = blink::mojom::BackgroundFetchOptions::New();
  blink::mojom::BackgroundFetchError error;
  blink::mojom::BackgroundFetchFailureReason failure_reason;

  EXPECT_EQ(0u,
            GetRegistrationUserDataByKeyPrefix(sw_id, kUserDataPrefix).size());
  // Create a registration.
  {
    EXPECT_CALL(*this,
                OnRegistrationCreated(registration_id, _, _, _, _, _, _));

    CreateRegistration(registration_id, CloneRequestVector(requests),
                       std::move(options), SkBitmap(), &error);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  }

  // We expect as many pending entries as there are requests.
  EXPECT_EQ(requests.size(),
            GetRegistrationUserDataByKeyPrefix(
                sw_id, background_fetch::kPendingRequestKeyPrefix)
                .size());

  // And deactivate it.
  MarkRegistrationForDeletion(registration_id, /* check_for_failure= */ true,
                              &error, &failure_reason);
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

  RestartDataManagerFromPersistentStorage();

  // Metadata proto + UI options + storage version + remaining pending fetches.
  EXPECT_EQ(5u,
            GetRegistrationUserDataByKeyPrefix(sw_id, kUserDataPrefix).size());

  // Cleanup should delete the registration.
  background_fetch_data_manager_->Cleanup();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0u,
            GetRegistrationUserDataByKeyPrefix(sw_id, kUserDataPrefix).size());

  RestartDataManagerFromPersistentStorage();

  // The deletion should have been persisted.
  EXPECT_EQ(0u,
            GetRegistrationUserDataByKeyPrefix(sw_id, kUserDataPrefix).size());
}

TEST_F(BackgroundFetchDataManagerTest, GetInitializationData) {
  {
    // No registered ServiceWorkers.
    std::vector<BackgroundFetchInitializationData> data =
        GetInitializationData();
    EXPECT_TRUE(data.empty());
  }

  int64_t sw_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, sw_id);
  {
    // Register ServiceWorker with no Background Fetch Registrations.
    std::vector<BackgroundFetchInitializationData> data =
        GetInitializationData();
    EXPECT_TRUE(data.empty());
  }

  std::vector<blink::mojom::FetchAPIRequestPtr> requests =
      CreateValidRequests(storage_key().origin(), 2u);
  auto options = blink::mojom::BackgroundFetchOptions::New();
  options->title = kInitialTitle;
  options->download_total = 42u;
  blink::mojom::BackgroundFetchError error;
  // Register a Background Fetch.
  BackgroundFetchRegistrationId registration_id(
      sw_id, storage_key(), kExampleDeveloperId, kExampleUniqueId);

  {
    EXPECT_CALL(*this,
                OnRegistrationCreated(registration_id, _, _, _, _, _, _));

    CreateRegistration(registration_id, CloneRequestVector(requests),
                       options.Clone(), CreateTestIcon(), &error);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  }

  {
    std::vector<BackgroundFetchInitializationData> data =
        GetInitializationData();
    ASSERT_EQ(data.size(), 1u);
    const BackgroundFetchInitializationData& init = data[0];

    EXPECT_EQ(init.registration_id, registration_id);
    EXPECT_EQ(init.registration_data->developer_id, kExampleDeveloperId);
    EXPECT_EQ(init.options->title, kInitialTitle);
    EXPECT_EQ(init.options->download_total, 42u);
    EXPECT_EQ(init.ui_title, kInitialTitle);
    EXPECT_EQ(init.num_requests, requests.size());
    EXPECT_EQ(init.num_completed_requests, 0u);
    EXPECT_TRUE(init.active_fetch_requests.empty());

    // Check icon.
    ASSERT_FALSE(init.icon.drawsNothing());
    EXPECT_NO_FATAL_FAILURE(ExpectIconProperties(init.icon, 42, SK_ColorGREEN));
  }

  // Mark one request as complete and start another.
  scoped_refptr<BackgroundFetchRequestInfo> request_info;
  PopNextRequest(registration_id, &error, &request_info);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  ASSERT_TRUE(request_info);

  AnnotateRequestInfoWithFakeDownloadManagerData(request_info.get());
  MarkRequestAsComplete(registration_id, request_info.get(), &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

  PopNextRequest(registration_id, &error, &request_info);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  ASSERT_TRUE(request_info);
  {
    std::vector<BackgroundFetchInitializationData> data =
        GetInitializationData();
    ASSERT_EQ(data.size(), 1u);

    EXPECT_EQ(data[0].num_requests, requests.size());
    EXPECT_EQ(data[0].num_completed_requests, 1u);
    ASSERT_EQ(data[0].active_fetch_requests.size(), 1u);

    const auto& init_request_info = data[0].active_fetch_requests[0];
    ASSERT_TRUE(init_request_info);
    EXPECT_EQ(request_info->download_guid(),
              init_request_info->download_guid());
    EXPECT_EQ(request_info->request_index(),
              init_request_info->request_index());
    EXPECT_EQ(
        SerializeFetchRequestToString(*(request_info->fetch_request())),
        SerializeFetchRequestToString(*(init_request_info->fetch_request())));
  }

  // Create another registration.
  BackgroundFetchRegistrationId registration_id2(
      sw_id, storage_key(), kAlternativeDeveloperId, kAlternativeUniqueId);
  {
    EXPECT_CALL(*this,
                OnRegistrationCreated(registration_id2, _, _, _, _, _, _));

    CreateRegistration(registration_id2, CloneRequestVector(requests),
                       std::move(options), SkBitmap(), &error);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  }

  {
    std::vector<BackgroundFetchInitializationData> data =
        GetInitializationData();
    ASSERT_EQ(data.size(), 2u);
  }
}

TEST_F(BackgroundFetchDataManagerTest, CreateInParallel) {
  // Tests that multiple parallel calls to the BackgroundFetchDataManager are
  // linearized and handled one at a time, rather than producing inconsistent
  // results due to interleaving.
  int64_t service_worker_registration_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId,
            service_worker_registration_id);

  auto options = blink::mojom::BackgroundFetchOptions::New();

  std::vector<blink::mojom::BackgroundFetchError> errors(5);

  // We expect a single successful registration to be created.
  EXPECT_CALL(*this, OnRegistrationCreated(_, _, _, _, _, _, _));

  const int num_parallel_creates = 5;

  base::RunLoop run_loop;
  base::RepeatingClosure quit_once_all_finished_closure =
      base::BarrierClosure(num_parallel_creates, run_loop.QuitClosure());

  for (int i = 0; i < num_parallel_creates; i++) {
    std::vector<blink::mojom::FetchAPIRequestPtr> requests =
        CreateValidRequests(storage_key().origin());
    // New |unique_id| per iteration, since each is a distinct registration.
    BackgroundFetchRegistrationId registration_id(
        service_worker_registration_id, storage_key(), kExampleDeveloperId,
        base::Uuid::GenerateRandomV4().AsLowercaseString());

    background_fetch_data_manager_->CreateRegistration(
        registration_id, std::move(requests), options.Clone(), SkBitmap(),
        /* start_paused = */ false, net::IsolationInfo(),
        base::BindOnce(&DidCreateRegistration, quit_once_all_finished_closure,
                       &errors[i]));
  }
  run_loop.Run();

  int success_count = 0;
  int duplicated_developer_id_count = 0;
  for (auto error : errors) {
    switch (error) {
      case blink::mojom::BackgroundFetchError::NONE:
        success_count++;
        break;
      case blink::mojom::BackgroundFetchError::DUPLICATED_DEVELOPER_ID:
        duplicated_developer_id_count++;
        break;
      default:
        break;
    }
  }
  // Exactly one of the calls should have succeeded in creating a registration,
  // and all the others should have failed with DUPLICATED_DEVELOPER_ID.
  EXPECT_EQ(1, success_count);
  EXPECT_EQ(num_parallel_creates - 1, duplicated_developer_id_count);
}

TEST_F(BackgroundFetchDataManagerTest, StorageErrorsReported) {
  int64_t sw_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, sw_id);

  auto requests =
      CreateValidRequests(storage_key().origin(), /* num_requests= */ 3u);
  auto options = blink::mojom::BackgroundFetchOptions::New();
  blink::mojom::BackgroundFetchError error;
  BackgroundFetchRegistrationId registration_id(
      sw_id, storage_key(), kExampleDeveloperId, kExampleUniqueId);

  {
    CreateRegistration(registration_id, CloneRequestVector(requests),
                       options.Clone(), SkBitmap(), &error);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  }

  BackgroundFetchRegistrationId registration_id2(
      sw_id,
      blink::StorageKey::CreateFromStringForTesting("https://examplebad.com"),
      kAlternativeDeveloperId, kAlternativeUniqueId);

  {
    // This should fail because the Service Worker doesn't exist.
    CreateRegistration(registration_id2, CloneRequestVector(requests),
                       std::move(options), SkBitmap(), &error);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::STORAGE_ERROR);
  }

  scoped_refptr<BackgroundFetchRequestInfo> request_info;
  PopNextRequest(registration_id, &error, &request_info);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  ASSERT_TRUE(request_info);
  AnnotateRequestInfoWithFakeDownloadManagerData(request_info.get(),
                                                 /* success= */ true);
  MarkRequestAsComplete(registration_id, request_info.get(), &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

  std::vector<blink::mojom::BackgroundFetchSettledFetchPtr> settled_fetches;

  {
    MatchRequests(registration_id, /* request_to_match= */ nullptr,
                  /* cache_query_options= */ nullptr, /* match_all= */ false,
                  &error, &settled_fetches);

    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  }

  // Delete all entries to get a CachStorageError.
  for (const auto& request : requests) {
    DeleteFromCache(request);
    ASSERT_FALSE(MatchCache(request));
  }

  {
    MatchRequests(registration_id, /* request_to_match= */ nullptr,
                  /* cache_query_options= */ nullptr, /* match_all= */ true,
                  &error, &settled_fetches);

    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::STORAGE_ERROR);
  }
}

TEST_F(BackgroundFetchDataManagerTest, NotifyObserversOnRequestCompletion) {
  int64_t sw_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, sw_id);

  BackgroundFetchRegistrationId registration_id1(
      sw_id, storage_key(), kExampleDeveloperId, kExampleUniqueId);

  std::vector<blink::mojom::FetchAPIRequestPtr> requests =
      CreateValidRequests(storage_key().origin(), 1u);
  auto options = blink::mojom::BackgroundFetchOptions::New();
  blink::mojom::BackgroundFetchError error;
  blink::mojom::BackgroundFetchFailureReason failure_reason;

  // Complete the fetch successfully, and expect an OnRequestCompleted() call.
  {
    CreateRegistration(registration_id1, CloneRequestVector(requests),
                       options.Clone(), SkBitmap(), &error);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
    scoped_refptr<BackgroundFetchRequestInfo> request_info;
    PopNextRequest(registration_id1, &error, &request_info);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
    AnnotateRequestInfoWithFakeDownloadManagerData(request_info.get(),
                                                   /* succeeded= */ true);
    EXPECT_CALL(*this, OnRequestCompleted(kExampleUniqueId, _, _));
    MarkRequestAsComplete(registration_id1, request_info.get(), &error);
    EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

    // Mark the Registration for deletion.
    MarkRegistrationForDeletion(registration_id1, /* check_for_failure= */ true,
                                &error, &failure_reason);
    EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
    EXPECT_EQ(failure_reason, blink::mojom::BackgroundFetchFailureReason::NONE);
  }

  BackgroundFetchRegistrationId registration_id2(
      sw_id, storage_key(), kAlternativeDeveloperId, kAlternativeUniqueId);

  // Complete the fetch with a BAD_STATUS, and expect an OnRequestCompleted()
  // call.
  {
    CreateRegistration(registration_id2, CloneRequestVector(requests),
                       std::move(options), SkBitmap(), &error);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
    scoped_refptr<BackgroundFetchRequestInfo> request_info;
    PopNextRequest(registration_id2, &error, &request_info);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
    AnnotateRequestInfoWithFakeDownloadManagerData(request_info.get());
    EXPECT_CALL(*this, OnRequestCompleted(kAlternativeUniqueId, _, _));
    MarkRequestAsComplete(registration_id2, request_info.get(), &error);
    EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

    // Mark the Registration for deletion.
    MarkRegistrationForDeletion(registration_id2, /* check_for_failure= */ true,
                                &error, &failure_reason);
    EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
    // The fetch resulted in a 404.
    EXPECT_EQ(failure_reason,
              blink::mojom::BackgroundFetchFailureReason::BAD_STATUS);
  }
}

TEST_F(BackgroundFetchDataManagerTest, IsolationInfo) {
  int64_t sw_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, sw_id);

  BackgroundFetchRegistrationId registration_id(
      sw_id, storage_key(), kExampleDeveloperId, kExampleUniqueId);
  blink::mojom::BackgroundFetchError error;

  auto isolation_info = net::IsolationInfo::Create(
      net::IsolationInfo::RequestType::kOther, storage_key().origin(),
      storage_key().origin(), net::SiteForCookies());

  {
    net::IsolationInfo captured;

    EXPECT_CALL(*this, OnRegistrationCreated(registration_id, _, _, _, _, _, _))
        .WillOnce(testing::SaveArg<6>(&captured));

    base::RunLoop run_loop;
    background_fetch_data_manager_->CreateRegistration(
        registration_id, CreateValidRequests(storage_key().origin(), 1u),
        blink::mojom::BackgroundFetchOptions::New(), SkBitmap(),
        /* start_paused= */ false, isolation_info,
        base::BindOnce(&DidCreateRegistration, run_loop.QuitClosure(), &error));
    run_loop.Run();

    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
    EXPECT_TRUE(isolation_info.IsEqualForTesting(captured));
  }

  {
    std::vector<BackgroundFetchInitializationData> data =
        GetInitializationData();
    ASSERT_EQ(data.size(), 1u);
    ASSERT_TRUE(data[0].isolation_info);
    ASSERT_TRUE(data[0].isolation_info->IsEqualForTesting(isolation_info));
  }
}

}  // namespace content
