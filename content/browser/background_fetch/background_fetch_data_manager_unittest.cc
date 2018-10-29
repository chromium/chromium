// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_data_manager.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/guid.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "content/browser/background_fetch/background_fetch.pb.h"
#include "content/browser/background_fetch/background_fetch_data_manager_observer.h"
#include "content/browser/background_fetch/background_fetch_request_info.h"
#include "content/browser/background_fetch/background_fetch_request_match_params.h"
#include "content/browser/background_fetch/background_fetch_test_base.h"
#include "content/browser/background_fetch/background_fetch_test_data_manager.h"
#include "content/browser/background_fetch/storage/database_helpers.h"
#include "content/browser/background_fetch/storage/image_helpers.h"
#include "content/browser/cache_storage/cache_storage_cache_handle.h"
#include "content/browser/cache_storage/cache_storage_manager.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/browser/background_fetch_response.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/platform/modules/background_fetch/background_fetch.mojom.h"
#include "third_party/blink/public/platform/modules/cache_storage/cache_storage.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"

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
const char kUpdatedTitle[] = "Updated Title";

constexpr size_t kResponseFileSize = 42u;

void DidGetInitializationData(
    base::Closure quit_closure,
    std::vector<BackgroundFetchInitializationData>* out_result,
    blink::mojom::BackgroundFetchError error,
    std::vector<BackgroundFetchInitializationData> result) {
  DCHECK_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  *out_result = std::move(result);
  std::move(quit_closure).Run();
}

void DidCreateRegistration(base::OnceClosure quit_closure,
                           blink::mojom::BackgroundFetchError* out_error,
                           blink::mojom::BackgroundFetchError error,
                           const BackgroundFetchRegistration& registration) {
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

void AnnotateRequestInfoWithFakeDownloadManagerData(
    BackgroundFetchRequestInfo* request_info,
    bool success = false,
    bool over_quota = false) {
  DCHECK(request_info);

  std::string headers =
      success ? "HTTP/1.1 200 OK\n" : "HTTP/1.1 404 Not found\n";
  auto response = std::make_unique<BackgroundFetchResponse>(
      std::vector<GURL>(1u, request_info->fetch_request().url),
      base::MakeRefCounted<net::HttpResponseHeaders>(headers));

  if (!success) {
    // Fill |request_info| with a failed result.
    request_info->SetResult(std::make_unique<BackgroundFetchResult>(
        std::move(response), base::Time::Now(),
        BackgroundFetchResult::FailureReason::FETCH_ERROR));
    return;
  }

  // This is treated as an empty response, but the size is set to
  // |kResponseFileSize| for tests that use filesize.
  request_info->SetResult(std::make_unique<BackgroundFetchResult>(
      std::move(response), base::Time::Now(), base::FilePath(),
      base::nullopt /* blob_handle */,
      over_quota ? kBackgroundFetchMaxQuotaBytes + 1 : kResponseFileSize));
}

void GetNumUserData(base::Closure quit_closure,
                    int* out_size,
                    const std::vector<std::string>& data,
                    blink::ServiceWorkerStatusCode status) {
  DCHECK(out_size);
  DCHECK_EQ(blink::ServiceWorkerStatusCode::kOk, status);
  *out_size = data.size();
  std::move(quit_closure).Run();
}

struct ResponseStateStats {
  int pending_requests = 0;
  int active_requests = 0;
  int completed_requests = 0;
};

bool operator==(const ResponseStateStats& s1, const ResponseStateStats& s2) {
  return s1.pending_requests == s2.pending_requests &&
         s1.active_requests == s2.active_requests &&
         s1.completed_requests == s2.completed_requests;
}

std::vector<ServiceWorkerFetchRequest> CreateValidRequests(
    const url::Origin& origin,
    size_t num_requests = 1u) {
  std::vector<ServiceWorkerFetchRequest> requests(num_requests);
  for (size_t i = 0; i < requests.size(); i++) {
    // Creates a URL of the form: `http://example.com/x`
    requests[i].url = GURL(origin.GetURL().spec() + base::NumberToString(i));
  }
  return requests;
}

ServiceWorkerFetchRequest CreateValidRequestWithMethod(
    const url::Origin& origin,
    const std::string& method) {
  ServiceWorkerFetchRequest request;
  request.url = origin.GetURL();
  request.method = method;
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

  // Re-creates the data manager. Useful for testing that data was persisted.
  void RestartDataManagerFromPersistentStorage() {
    background_fetch_data_manager_ =
        std::make_unique<BackgroundFetchTestDataManager>(
            browser_context(), storage_partition(),
            embedded_worker_test_helper()->context_wrapper(),
            true /* mock_fill_response */);

    background_fetch_data_manager_->AddObserver(this);
    background_fetch_data_manager_->InitializeOnIOThread();
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
      const std::vector<ServiceWorkerFetchRequest>& requests,
      const BackgroundFetchOptions& options,
      const SkBitmap& icon,
      blink::mojom::BackgroundFetchError* out_error) {
    DCHECK(out_error);

    base::RunLoop run_loop;
    background_fetch_data_manager_->CreateRegistration(
        registration_id, requests, options, icon, /* start_paused = */ false,
        base::BindOnce(&DidCreateRegistration, run_loop.QuitClosure(),
                       out_error));
    run_loop.Run();

    // Check that a cache was created.
    if (*out_error == blink::mojom::BackgroundFetchError::NONE)
      DCHECK(HasCache(registration_id.unique_id()));
  }

  BackgroundFetchRegistration GetRegistration(
      int64_t service_worker_registration_id,
      const url::Origin& origin,
      const std::string developer_id,
      blink::mojom::BackgroundFetchError* out_error) {
    DCHECK(out_error);

    BackgroundFetchRegistration registration;
    base::RunLoop run_loop;
    background_fetch_data_manager_->GetRegistration(
        service_worker_registration_id, origin, developer_id,
        base::BindOnce(&BackgroundFetchDataManagerTest::DidGetRegistration,
                       base::Unretained(this), run_loop.QuitClosure(),
                       out_error, &registration));
    run_loop.Run();

    return registration;
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

  void UpdateRegistrationUI(
      const BackgroundFetchRegistrationId& registration_id,
      const base::Optional<std::string>& updated_title,
      const base::Optional<SkBitmap>& updated_icon,
      blink::mojom::BackgroundFetchError* out_error) {
    DCHECK(out_error);

    base::RunLoop run_loop;
    background_fetch_data_manager_->UpdateRegistrationUI(
        registration_id, updated_title, updated_icon,
        base::BindOnce(&BackgroundFetchDataManagerTest::DidUpdateRegistrationUI,
                       base::Unretained(this), run_loop.QuitClosure(),
                       out_error));
    run_loop.Run();
  }

  std::vector<std::string> GetDeveloperIds(
      int64_t service_worker_registration_id,
      const url::Origin& origin,
      blink::mojom::BackgroundFetchError* out_error) {
    DCHECK(out_error);

    std::vector<std::string> ids;
    base::RunLoop run_loop;
    background_fetch_data_manager_->GetDeveloperIdsForServiceWorker(
        service_worker_registration_id, origin,
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
  void MatchRequests(
      const BackgroundFetchRegistrationId& registration_id,
      base::Optional<ServiceWorkerFetchRequest> request_to_match,
      blink::mojom::QueryParamsPtr cache_query_params,
      bool match_all,
      blink::mojom::BackgroundFetchError* out_error,
      std::vector<BackgroundFetchSettledFetch>* out_settled_fetches) {
    DCHECK(out_error);
    DCHECK(out_settled_fetches);

    base::RunLoop run_loop;
    auto match_params = std::make_unique<BackgroundFetchRequestMatchParams>(
        request_to_match, std::move(cache_query_params), match_all);
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

  // Synchronous version of CacheStorageManager::HasCache().
  bool HasCache(const std::string& cache_name) {
    bool result = false;

    base::RunLoop run_loop;
    background_fetch_data_manager_->cache_manager()->HasCache(
        origin(), CacheStorageOwner::kBackgroundFetch, cache_name,
        base::BindOnce(&BackgroundFetchDataManagerTest::DidFindCache,
                       base::Unretained(this), run_loop.QuitClosure(),
                       &result));
    run_loop.Run();

    return result;
  }

  // Synchronous version of CacheStorageManager::MatchCache().
  bool MatchCache(const ServiceWorkerFetchRequest& request) {
    bool result = false;
    auto request_ptr = std::make_unique<ServiceWorkerFetchRequest>(request);

    base::RunLoop run_loop;
    background_fetch_data_manager_->cache_manager()->MatchCache(
        origin(), CacheStorageOwner::kBackgroundFetch, kExampleUniqueId,
        std::move(request_ptr), nullptr /* match_params */,
        base::BindOnce(&BackgroundFetchDataManagerTest::DidMatchCache,
                       base::Unretained(this), run_loop.QuitClosure(),
                       &result));
    run_loop.Run();

    return result;
  }

  void DeleteFromCache(const ServiceWorkerFetchRequest& request) {
    CacheStorageCacheHandle handle;
    {
      base::RunLoop run_loop;
      background_fetch_data_manager_->cache_manager()->OpenCache(
          origin(), CacheStorageOwner::kBackgroundFetch,
          kExampleUniqueId /* cache_name */,
          base::BindOnce(&BackgroundFetchDataManagerTest::DidOpenCache,
                         base::Unretained(this), run_loop.QuitClosure(),
                         &handle));
      run_loop.Run();
    }

    DCHECK(handle.value());

    {
      base::RunLoop run_loop;
      std::vector<blink::mojom::BatchOperationPtr> operation_ptr_vec;
      operation_ptr_vec.push_back(blink::mojom::BatchOperation::New());
      operation_ptr_vec[0]->operation_type =
          blink::mojom::OperationType::kDelete;
      operation_ptr_vec[0]->request = request;

      handle.value()->BatchOperation(
          std::move(operation_ptr_vec), true /* fail_on_duplicates */,
          base::BindOnce(&BackgroundFetchDataManagerTest::DidDeleteFromCache,
                         base::Unretained(this), run_loop.QuitClosure()),
          base::DoNothing());

      run_loop.Run();
    }
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

  // BackgroundFetchDataManagerObserver mocks:
  MOCK_METHOD6(OnRegistrationCreated,
               void(const BackgroundFetchRegistrationId& registration_id,
                    const BackgroundFetchRegistration& registration,
                    const BackgroundFetchOptions& options,
                    const SkBitmap& icon,
                    int num_requests,
                    bool start_paused));
  MOCK_METHOD3(OnUpdatedUI,
               void(const BackgroundFetchRegistrationId& registration,
                    const base::Optional<std::string>& title,
                    const base::Optional<SkBitmap>& icon));
  MOCK_METHOD1(OnServiceWorkerDatabaseCorrupted,
               void(int64_t service_worker_registration_id));
  MOCK_METHOD1(OnQuotaExceeded,
               void(const BackgroundFetchRegistrationId& registration_id));
  MOCK_METHOD1(OnFetchStorageError,
               void(const BackgroundFetchRegistrationId& registration_id));

 protected:
  void DidGetRegistration(base::OnceClosure quit_closure,
                          blink::mojom::BackgroundFetchError* out_error,
                          BackgroundFetchRegistration* out_registration,
                          blink::mojom::BackgroundFetchError error,
                          const BackgroundFetchRegistration& registration) {
    *out_error = error;
    *out_registration = registration;

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

  void DidUpdateRegistrationUI(base::OnceClosure quit_closure,
                               blink::mojom::BackgroundFetchError* out_error,
                               blink::mojom::BackgroundFetchError error) {
    *out_error = error;
    std::move(quit_closure).Run();
  }

  void DidGetDeveloperIds(base::Closure quit_closure,
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
      std::vector<BackgroundFetchSettledFetch>* out_settled_fetches,
      blink::mojom::BackgroundFetchError error,
      std::vector<BackgroundFetchSettledFetch> settled_fetches) {
    *out_error = error;
    *out_settled_fetches = std::move(settled_fetches);
    std::move(quit_closure).Run();
  }

  void DidFindCache(base::OnceClosure quit_closure,
                    bool* out_result,
                    bool has_cache,
                    blink::mojom::CacheStorageError error) {
    DCHECK_EQ(error, blink::mojom::CacheStorageError::kSuccess);
    *out_result = has_cache;
    std::move(quit_closure).Run();
  }

  void DidMatchCache(base::OnceClosure quit_closure,
                     bool* out_result,
                     blink::mojom::CacheStorageError error,
                     blink::mojom::FetchAPIResponsePtr response) {
    // This counts as matched if an entry was found in the cache which
    // also has a non-empty response.
    *out_result = !response.is_null() && !response->url_list.empty();
    std::move(quit_closure).Run();
  }

  void DidOpenCache(base::OnceClosure quit_closure,
                    CacheStorageCacheHandle* out_handle,
                    CacheStorageCacheHandle handle,
                    blink::mojom::CacheStorageError error) {
    DCHECK(out_handle);
    DCHECK_EQ(error, blink::mojom::CacheStorageError::kSuccess);
    *out_handle = std::move(handle);
    std::move(quit_closure).Run();
  }

  void DidDeleteFromCache(base::OnceClosure quit_closure,
                          blink::mojom::CacheStorageVerboseErrorPtr error) {
    DCHECK_EQ(error->value, blink::mojom::CacheStorageError::kSuccess);
    std::move(quit_closure).Run();
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

  BackgroundFetchRegistrationId registration_id1(service_worker_registration_id,
                                                 origin(), kExampleDeveloperId,
                                                 kExampleUniqueId);

  std::vector<ServiceWorkerFetchRequest> requests =
      CreateValidRequests(origin());
  BackgroundFetchOptions options;

  blink::mojom::BackgroundFetchError error;
  blink::mojom::BackgroundFetchFailureReason failure_reason;

  // Deactivating the not-yet-created registration should fail.
  MarkRegistrationForDeletion(registration_id1, /* check_for_failure= */ true,
                              &error, &failure_reason);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::INVALID_ID);

  // Creating the initial registration should succeed.
  {
    EXPECT_CALL(*this, OnRegistrationCreated(registration_id1, _, _, _, _, _));

    CreateRegistration(registration_id1, requests, options, SkBitmap(), &error);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  }

  // Different |unique_id|, since this is a new Background Fetch registration,
  // even though it shares the same |developer_id|.
  BackgroundFetchRegistrationId registration_id2(service_worker_registration_id,
                                                 origin(), kExampleDeveloperId,
                                                 kAlternativeUniqueId);

  // Attempting to create a second registration with the same |developer_id| and
  // |service_worker_registration_id| should yield an error.
  CreateRegistration(registration_id2, requests, options, SkBitmap(), &error);
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
    EXPECT_CALL(*this, OnRegistrationCreated(registration_id2, _, _, _, _, _));

    CreateRegistration(registration_id2, requests, options, SkBitmap(), &error);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  }
}

TEST_F(BackgroundFetchDataManagerTest, ExceedingQuotaFailsCreation) {
  // Tests that the BackgroundFetchDataManager correctly rejects creating a
  // registration where the provided download total exceeds the available quota.

  int64_t service_worker_registration_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId,
            service_worker_registration_id);

  BackgroundFetchRegistrationId registration_id(service_worker_registration_id,
                                                origin(), kExampleDeveloperId,
                                                kExampleUniqueId);
  std::vector<ServiceWorkerFetchRequest> requests =
      CreateValidRequests(origin());
  BackgroundFetchOptions options;
  options.download_total = kBackgroundFetchMaxQuotaBytes + 1;

  blink::mojom::BackgroundFetchError error;

  CreateRegistration(registration_id, requests, options, SkBitmap(), &error);
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
        swid1, origin(), kExampleDeveloperId + base::IntToString(i),
        base::GenerateGUID());
    CreateRegistration(registration_id1,
                       std::vector<ServiceWorkerFetchRequest>(),
                       BackgroundFetchOptions(), SkBitmap(), &error);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

    // Second service Worker.
    BackgroundFetchRegistrationId registration_id2(
        swid2, origin(), kExampleDeveloperId + base::IntToString(i),
        base::GenerateGUID());
    CreateRegistration(registration_id2,
                       std::vector<ServiceWorkerFetchRequest>(),
                       BackgroundFetchOptions(), SkBitmap(), &error);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  }

  // Create another registration in the first Service Worker,
  // bringing us to the limit.
  {
    BackgroundFetchRegistrationId registration_id(
        swid1, origin(), "developer_id1", base::GenerateGUID());
    CreateRegistration(registration_id,
                       std::vector<ServiceWorkerFetchRequest>(),
                       BackgroundFetchOptions(), SkBitmap(), &error);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  }

  // A registration this time should fail.
  {
    BackgroundFetchRegistrationId registration_id(
        swid1, origin(), "developer_id2", base::GenerateGUID());
    CreateRegistration(registration_id,
                       std::vector<ServiceWorkerFetchRequest>(),
                       BackgroundFetchOptions(), SkBitmap(), &error);
    ASSERT_EQ(error,
              blink::mojom::BackgroundFetchError::REGISTRATION_LIMIT_EXCEEDED);
  }

  // The registration should also fail for the other Service Worker.
  {
    BackgroundFetchRegistrationId registration_id(
        swid2, origin(), "developer_id3", base::GenerateGUID());
    CreateRegistration(registration_id,
                       std::vector<ServiceWorkerFetchRequest>(),
                       BackgroundFetchOptions(), SkBitmap(), &error);
    ASSERT_EQ(error,
              blink::mojom::BackgroundFetchError::REGISTRATION_LIMIT_EXCEEDED);
  }
}

TEST_F(BackgroundFetchDataManagerTest, GetDeveloperIds) {
  int64_t sw_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, sw_id);

  std::vector<ServiceWorkerFetchRequest> requests =
      CreateValidRequests(origin(), 2u);
  BackgroundFetchOptions options;
  blink::mojom::BackgroundFetchError error;

  // Verify that no developer IDs can be found.
  auto developer_ids = GetDeveloperIds(sw_id, origin(), &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  EXPECT_THAT(developer_ids, IsEmpty());

  // Create a single registration.
  BackgroundFetchRegistrationId registration_id1(
      sw_id, origin(), kExampleDeveloperId, kExampleUniqueId);
  {
    EXPECT_CALL(*this, OnRegistrationCreated(registration_id1, _, _, _, _, _));

    CreateRegistration(registration_id1, requests, options, SkBitmap(), &error);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  }

  // Verify that the developer ID can be found.
  developer_ids = GetDeveloperIds(sw_id, origin(), &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  EXPECT_THAT(developer_ids, UnorderedElementsAre(kExampleDeveloperId));

  RestartDataManagerFromPersistentStorage();

  // After a restart, GetDeveloperIds should still find the IDs.
  developer_ids = GetDeveloperIds(sw_id, origin(), &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  EXPECT_THAT(developer_ids, UnorderedElementsAre(kExampleDeveloperId));

  // Create another registration.
  BackgroundFetchRegistrationId registration_id2(
      sw_id, origin(), kAlternativeDeveloperId, kAlternativeUniqueId);
  {
    EXPECT_CALL(*this, OnRegistrationCreated(registration_id2, _, _, _, _, _));

    CreateRegistration(registration_id2, requests, options, SkBitmap(), &error);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  }
  // Verify that both developer IDs can be found.
  developer_ids = GetDeveloperIds(sw_id, origin(), &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  EXPECT_THAT(developer_ids, UnorderedElementsAre(kExampleDeveloperId,
                                                  kAlternativeDeveloperId));
  RestartDataManagerFromPersistentStorage();

  // After a restart, GetDeveloperIds should still find the IDs.
  developer_ids = GetDeveloperIds(sw_id, origin(), &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  EXPECT_THAT(developer_ids, UnorderedElementsAre(kExampleDeveloperId,
                                                  kAlternativeDeveloperId));
}

TEST_F(BackgroundFetchDataManagerTest, GetRegistration) {
  int64_t sw_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, sw_id);

  BackgroundFetchRegistrationId registration_id(
      sw_id, origin(), kExampleDeveloperId, kExampleUniqueId);

  std::vector<ServiceWorkerFetchRequest> requests =
      CreateValidRequests(origin(), 2u);
  BackgroundFetchOptions options;
  blink::mojom::BackgroundFetchError error;

  // Create a single registration.
  {
    EXPECT_CALL(*this, OnRegistrationCreated(registration_id, _, _, _, _, _));

    CreateRegistration(registration_id, requests, options, SkBitmap(), &error);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  }

  // Verify that the registration can be retrieved.
  auto registration =
      GetRegistration(sw_id, origin(), kExampleDeveloperId, &error);
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

  EXPECT_EQ(kExampleUniqueId, registration.unique_id);
  EXPECT_EQ(kExampleDeveloperId, registration.developer_id);

  // Verify that retrieving using the wrong developer id doesn't work.
  registration =
      GetRegistration(sw_id, origin(), kAlternativeDeveloperId, &error);
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::INVALID_ID);

  RestartDataManagerFromPersistentStorage();

  // After a restart, GetRegistration should still find the registration.
  registration = GetRegistration(sw_id, origin(), kExampleDeveloperId, &error);
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

  EXPECT_EQ(kExampleUniqueId, registration.unique_id);
  EXPECT_EQ(kExampleDeveloperId, registration.developer_id);
}

TEST_F(BackgroundFetchDataManagerTest, GetMetadata) {
  int64_t sw_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, sw_id);

  BackgroundFetchRegistrationId registration_id(
      sw_id, origin(), kExampleDeveloperId, kExampleUniqueId);

  std::vector<ServiceWorkerFetchRequest> requests =
      CreateValidRequests(origin(), 2u);
  BackgroundFetchOptions options;
  blink::mojom::BackgroundFetchError error;

  // Create a single registration.
  {
    EXPECT_CALL(*this, OnRegistrationCreated(registration_id, _, _, _, _, _));

    CreateRegistration(registration_id, requests, options, SkBitmap(), &error);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  }
  // Verify that the metadata can be retrieved.
  auto metadata = GetMetadata(sw_id, kExampleUniqueId);
  ASSERT_TRUE(metadata);
  EXPECT_EQ(metadata->origin(), origin().Serialize());
  EXPECT_NE(metadata->creation_microseconds_since_unix_epoch(), 0);
  EXPECT_EQ(metadata->num_fetches(), static_cast<int>(requests.size()));

  RestartDataManagerFromPersistentStorage();

  // After a restart, GetMetadata should still find the registration.
  metadata = GetMetadata(sw_id, kExampleUniqueId);
  ASSERT_TRUE(metadata);
  EXPECT_EQ(metadata->origin(), origin().Serialize());
  EXPECT_NE(metadata->creation_microseconds_since_unix_epoch(), 0);
  EXPECT_EQ(metadata->num_fetches(), static_cast<int>(requests.size()));
}

TEST_F(BackgroundFetchDataManagerTest, LargeIconNotPersisted) {
  int64_t sw_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, sw_id);

  BackgroundFetchRegistrationId registration_id(
      sw_id, origin(), kExampleDeveloperId, kExampleUniqueId);

  std::vector<ServiceWorkerFetchRequest> requests =
      CreateValidRequests(origin(), 2u);
  BackgroundFetchOptions options;
  blink::mojom::BackgroundFetchError error;

  SkBitmap icon = CreateTestIcon(512 /* size */);
  ASSERT_FALSE(background_fetch::ShouldPersistIcon(icon));

  // Create a single registration.
  {
    EXPECT_CALL(*this, OnRegistrationCreated(registration_id, _, _, _, _, _));

    CreateRegistration(registration_id, requests, options, icon, &error);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  }

  // Verify that the metadata can be retrieved.
  auto metadata = GetMetadata(sw_id, kExampleUniqueId);
  ASSERT_TRUE(metadata);
  EXPECT_EQ(metadata->origin(), origin().Serialize());
  EXPECT_NE(metadata->creation_microseconds_since_unix_epoch(), 0);
  EXPECT_EQ(metadata->num_fetches(), static_cast<int>(requests.size()));
  EXPECT_TRUE(GetUIOptions(sw_id).second.isNull());
}

TEST_F(BackgroundFetchDataManagerTest, UpdateRegistrationUI) {
  int64_t sw_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, sw_id);

  BackgroundFetchRegistrationId registration_id(
      sw_id, origin(), kExampleDeveloperId, kExampleUniqueId);

  std::vector<ServiceWorkerFetchRequest> requests =
      CreateValidRequests(origin(), 2u);
  BackgroundFetchOptions options;
  options.title = kInitialTitle;
  blink::mojom::BackgroundFetchError error;

  // There should be no title before the registration.
  EXPECT_TRUE(GetUIOptions(sw_id).first.empty());

  // Create a single registration.
  {
    EXPECT_CALL(*this, OnRegistrationCreated(registration_id, _, _, _, _, _));

    CreateRegistration(registration_id, requests, options, CreateTestIcon(),
                       &error);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  }

  // Verify that the UI Options can be retrieved.
  {
    auto ui_options = GetUIOptions(sw_id);
    EXPECT_EQ(ui_options.first, kInitialTitle);
    EXPECT_NO_FATAL_FAILURE(
        ExpectIconProperties(ui_options.second, 42, SK_ColorGREEN));
  }

  // Update only the title.
  {
    EXPECT_CALL(*this,
                OnUpdatedUI(registration_id,
                            base::Optional<std::string>(kUpdatedTitle), _));

    UpdateRegistrationUI(registration_id, kUpdatedTitle, base::nullopt, &error);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

    auto ui_options = GetUIOptions(sw_id);
    // Expect new title.
    EXPECT_EQ(ui_options.first, kUpdatedTitle);
    // Expect same icon as before.
    EXPECT_NO_FATAL_FAILURE(
        ExpectIconProperties(ui_options.second, 42, SK_ColorGREEN));
  }

  // Update only the icon.
  {
    EXPECT_CALL(*this, OnUpdatedUI(registration_id, _, _));

    UpdateRegistrationUI(registration_id, base::nullopt,
                         CreateTestIcon(24 /* size */), &error);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

    auto ui_options = GetUIOptions(sw_id);
    // Expect the same title as before.
    EXPECT_EQ(ui_options.first, kUpdatedTitle);
    // Expect the new icon with the different size.
    EXPECT_NO_FATAL_FAILURE(
        ExpectIconProperties(ui_options.second, 24, SK_ColorGREEN));
  }

  // Update both the title and icon.
  {
    EXPECT_CALL(*this,
                OnUpdatedUI(registration_id,
                            base::Optional<std::string>(kInitialTitle), _));

    UpdateRegistrationUI(registration_id, kInitialTitle,
                         CreateTestIcon(66 /* size */), &error);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

    auto ui_options = GetUIOptions(sw_id);
    // Expect the initial title again.
    EXPECT_EQ(ui_options.first, kInitialTitle);
    // Expect the new icon with the different size.
    EXPECT_NO_FATAL_FAILURE(
        ExpectIconProperties(ui_options.second, 66, SK_ColorGREEN));
  }

  // New title and an icon that's too large.
  {
    EXPECT_CALL(*this,
                OnUpdatedUI(registration_id,
                            base::Optional<std::string>(kUpdatedTitle), _));

    UpdateRegistrationUI(registration_id, kUpdatedTitle,
                         CreateTestIcon(512 /* size */), &error);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

    auto ui_options = GetUIOptions(sw_id);
    // Expect the new title.
    EXPECT_EQ(ui_options.first, kUpdatedTitle);
    // Expect same icon as before.
    EXPECT_NO_FATAL_FAILURE(
        ExpectIconProperties(ui_options.second, 66, SK_ColorGREEN));
  }
}

TEST_F(BackgroundFetchDataManagerTest, CreateAndDeleteRegistration) {
  int64_t sw_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, sw_id);

  BackgroundFetchRegistrationId registration_id1(
      sw_id, origin(), kExampleDeveloperId, kExampleUniqueId);

  std::vector<ServiceWorkerFetchRequest> requests =
      CreateValidRequests(origin(), 2u);
  BackgroundFetchOptions options;
  blink::mojom::BackgroundFetchError error;
  blink::mojom::BackgroundFetchFailureReason failure_reason;

  {
    EXPECT_CALL(*this, OnRegistrationCreated(registration_id1, _, _, _, _, _));

    CreateRegistration(registration_id1, requests, options, SkBitmap(), &error);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  }

  RestartDataManagerFromPersistentStorage();

  // Different |unique_id|, since this is a new Background Fetch registration,
  // even though it shares the same |developer_id|.
  BackgroundFetchRegistrationId registration_id2(
      sw_id, origin(), kExampleDeveloperId, kAlternativeUniqueId);

  // Attempting to create a second registration with the same |developer_id| and
  // |service_worker_registration_id| should yield an error, even after
  // restarting.
  CreateRegistration(registration_id2, requests, options, SkBitmap(), &error);
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::DUPLICATED_DEVELOPER_ID);

  // Verify that the registration can be retrieved before deletion.
  auto registration =
      GetRegistration(sw_id, origin(), kExampleDeveloperId, &error);
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

  EXPECT_EQ(kExampleUniqueId, registration.unique_id);
  EXPECT_EQ(kExampleDeveloperId, registration.developer_id);

  // Deactivating the registration should succeed.
  MarkRegistrationForDeletion(registration_id1, /* check_for_failure= */ true,
                              &error, &failure_reason);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  EXPECT_EQ(failure_reason, blink::mojom::BackgroundFetchFailureReason::NONE);

  // Verify that the registration cannot be retrieved after deletion
  registration = GetRegistration(sw_id, origin(), kExampleDeveloperId, &error);
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::INVALID_ID);

  RestartDataManagerFromPersistentStorage();

  // Verify again that the registration cannot be retrieved after deletion and
  // a restart.
  registration = GetRegistration(sw_id, origin(), kExampleDeveloperId, &error);
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::INVALID_ID);

  // And now registering the second registration should work fine, even after
  // restarting, since there is no longer an *active* registration with the same
  // |developer_id|, even though the initial registration has not yet been
  // deleted.
  {
    EXPECT_CALL(*this, OnRegistrationCreated(registration_id2, _, _, _, _, _));

    CreateRegistration(registration_id2, requests, options, SkBitmap(), &error);
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
      sw_id, origin(), kExampleDeveloperId, kExampleUniqueId);

  std::vector<ServiceWorkerFetchRequest> requests =
      CreateValidRequests(origin(), 2u);
  BackgroundFetchOptions options;
  blink::mojom::BackgroundFetchError error;
  blink::mojom::BackgroundFetchFailureReason failure_reason;

  CreateRegistration(registration_id1, requests, options, SkBitmap(), &error);
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

  // Create a |developer_id| such that the other one is a substring.
  std::string developer_id2 = std::string(kExampleDeveloperId) + "!";
  BackgroundFetchRegistrationId registration_id2(sw_id, origin(), developer_id2,
                                                 kAlternativeUniqueId);

  CreateRegistration(registration_id2, requests, options, SkBitmap(), &error);
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
      sw_id, origin(), kExampleDeveloperId, kExampleUniqueId);

  std::vector<ServiceWorkerFetchRequest> requests =
      CreateValidRequests(origin(), 1u);
  BackgroundFetchOptions options;
  blink::mojom::BackgroundFetchError error;
  blink::mojom::BackgroundFetchFailureReason failure_reason;

  // Complete the fetch successfully.
  {
    CreateRegistration(registration_id1, requests, options, SkBitmap(), &error);
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
      sw_id, origin(), kAlternativeDeveloperId, kAlternativeUniqueId);

  // Complete the fetch with a BAD_STATUS.
  {
    CreateRegistration(registration_id2, requests, options, SkBitmap(), &error);
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
      sw_id, origin(), kExampleDeveloperId, kExampleUniqueId);

  // There registration hasn't been created yet, so there are no pending
  // requests.
  PopNextRequest(registration_id, &error, &request_info);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  EXPECT_FALSE(request_info);
  EXPECT_EQ(
      GetRequestStats(sw_id),
      (ResponseStateStats{0 /* pending_requests */, 0 /* active_requests */,
                          0 /* completed_requests */}));

  std::vector<ServiceWorkerFetchRequest> requests =
      CreateValidRequests(origin(), 2u);
  BackgroundFetchOptions options;

  {
    EXPECT_CALL(*this, OnRegistrationCreated(registration_id, _, _, _, _, _));

    CreateRegistration(registration_id, requests, options, SkBitmap(), &error);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  }

  EXPECT_EQ(
      GetRequestStats(sw_id),
      (ResponseStateStats{2 /* pending_requests */, 0 /* active_requests */,
                          0 /* completed_requests */}));

  // Popping should work now.
  PopNextRequest(registration_id, &error, &request_info);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  EXPECT_TRUE(request_info);
  EXPECT_EQ(request_info->request_index(), 0);
  EXPECT_FALSE(request_info->download_guid().empty());
  EXPECT_EQ(
      GetRequestStats(sw_id),
      (ResponseStateStats{1 /* pending_requests */, 1 /* active_requests */,
                          0 /* completed_requests */}));

  // Mark as complete.
  AnnotateRequestInfoWithFakeDownloadManagerData(request_info.get());
  MarkRequestAsComplete(registration_id, request_info.get(), &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  ASSERT_EQ(
      GetRequestStats(sw_id),
      (ResponseStateStats{1 /* pending_requests */, 0 /* active_requests */,
                          1 /* completed_requests */}));

  RestartDataManagerFromPersistentStorage();

  PopNextRequest(registration_id, &error, &request_info);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  EXPECT_TRUE(request_info);
  EXPECT_EQ(request_info->request_index(), 1);
  EXPECT_FALSE(request_info->download_guid().empty());
  EXPECT_EQ(
      GetRequestStats(sw_id),
      (ResponseStateStats{0 /* pending_requests */, 1 /* active_requests */,
                          1 /* completed_requests */}));

  // Mark as complete.
  AnnotateRequestInfoWithFakeDownloadManagerData(request_info.get());
  MarkRequestAsComplete(registration_id, request_info.get(), &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  ASSERT_EQ(
      GetRequestStats(sw_id),
      (ResponseStateStats{0 /* pending_requests */, 0 /* active_requests */,
                          2 /* completed_requests */}));

  // We are out of pending requests.
  PopNextRequest(registration_id, &error, &request_info);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  EXPECT_FALSE(request_info);
  EXPECT_EQ(
      GetRequestStats(sw_id),
      (ResponseStateStats{0 /* pending_requests */, 0 /* active_requests */,
                          2 /* completed_requests */}));
}

TEST_F(BackgroundFetchDataManagerTest, DownloadedUpdated) {
  int64_t sw_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, sw_id);

  BackgroundFetchRegistrationId registration_id(
      sw_id, origin(), kExampleDeveloperId, kExampleUniqueId);
  auto requests = CreateValidRequests(origin(), 3u /* num_requests */);

  BackgroundFetchOptions options;
  blink::mojom::BackgroundFetchError error;
  {
    EXPECT_CALL(*this, OnRegistrationCreated(registration_id, _, _, _, _, _));

    CreateRegistration(registration_id, requests, options, SkBitmap(), &error);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  }

  auto registration =
      GetRegistration(sw_id, origin(), kExampleDeveloperId, &error);
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  EXPECT_EQ(registration.downloaded, 0u);

  scoped_refptr<BackgroundFetchRequestInfo> request_info;
  PopNextRequest(registration_id, &error, &request_info);
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  AnnotateRequestInfoWithFakeDownloadManagerData(request_info.get(),
                                                 true /* succeeded */);
  MarkRequestAsComplete(registration_id, request_info.get(), &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

  registration = GetRegistration(sw_id, origin(), kExampleDeveloperId, &error);
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  EXPECT_EQ(registration.downloaded, kResponseFileSize);

  PopNextRequest(registration_id, &error, &request_info);
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  AnnotateRequestInfoWithFakeDownloadManagerData(request_info.get(),
                                                 true /* succeeded */);
  MarkRequestAsComplete(registration_id, request_info.get(), &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

  registration = GetRegistration(sw_id, origin(), kExampleDeveloperId, &error);
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  EXPECT_EQ(registration.downloaded, 2 * kResponseFileSize);

  PopNextRequest(registration_id, &error, &request_info);
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  AnnotateRequestInfoWithFakeDownloadManagerData(request_info.get(),
                                                 false /* succeeded */);
  MarkRequestAsComplete(registration_id, request_info.get(), &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

  registration = GetRegistration(sw_id, origin(), kExampleDeveloperId, &error);
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  // |registration.downloaded| is unchanged.
  EXPECT_EQ(registration.downloaded, 2 * kResponseFileSize);
}

TEST_F(BackgroundFetchDataManagerTest, ExceedingQuotaAbandonsFetch) {
  int64_t sw_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, sw_id);

  BackgroundFetchRegistrationId registration_id(
      sw_id, origin(), kExampleDeveloperId, kExampleUniqueId);
  auto requests = CreateValidRequests(origin(), 3u /* num_requests */);

  BackgroundFetchOptions options;
  blink::mojom::BackgroundFetchError error;
  {
    EXPECT_CALL(*this, OnRegistrationCreated(registration_id, _, _, _, _, _));

    CreateRegistration(registration_id, requests, options, SkBitmap(), &error);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  }

  scoped_refptr<BackgroundFetchRequestInfo> request_info;
  PopNextRequest(registration_id, &error, &request_info);
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  AnnotateRequestInfoWithFakeDownloadManagerData(
      request_info.get(), true /* succeeded */, true /* over_quota */);
  {
    EXPECT_CALL(*this, OnQuotaExceeded(registration_id));
    MarkRequestAsComplete(registration_id, request_info.get(), &error);
    EXPECT_EQ(error, blink::mojom::BackgroundFetchError::QUOTA_EXCEEDED);
  }
}

TEST_F(BackgroundFetchDataManagerTest, WriteToCache) {
  int64_t sw_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, sw_id);

  BackgroundFetchRegistrationId registration_id(
      sw_id, origin(), kExampleDeveloperId, kExampleUniqueId);
  auto requests = CreateValidRequests(origin(), 2u /* num_requests */);

  BackgroundFetchOptions options;
  blink::mojom::BackgroundFetchError error;
  {
    EXPECT_CALL(*this, OnRegistrationCreated(registration_id, _, _, _, _, _));

    CreateRegistration(registration_id, requests, options, SkBitmap(), &error);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  }

  scoped_refptr<BackgroundFetchRequestInfo> request_info;
  PopNextRequest(registration_id, &error, &request_info);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  ASSERT_TRUE(request_info);

  AnnotateRequestInfoWithFakeDownloadManagerData(request_info.get(),
                                                 true /* success */);
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
                                                 true /* success */);
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
      sw_id, origin(), kExampleDeveloperId, kExampleUniqueId);

  ServiceWorkerFetchRequest request;
  request.url = GURL(origin().GetURL().spec());

  BackgroundFetchOptions options;
  blink::mojom::BackgroundFetchError error;
  blink::mojom::BackgroundFetchFailureReason failure_reason;
  {
    EXPECT_CALL(*this, OnRegistrationCreated(registration_id, _, _, _, _, _));

    CreateRegistration(registration_id, {request}, options, SkBitmap(), &error);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  }

  scoped_refptr<BackgroundFetchRequestInfo> request_info;
  PopNextRequest(registration_id, &error, &request_info);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  ASSERT_TRUE(request_info);

  AnnotateRequestInfoWithFakeDownloadManagerData(request_info.get(),
                                                 true /* success */);
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

  std::vector<ServiceWorkerFetchRequest> requests =
      CreateValidRequests(origin(), 2u /* num_requests */);
  BackgroundFetchOptions options;
  blink::mojom::BackgroundFetchError error;
  BackgroundFetchRegistrationId registration_id(
      sw_id, origin(), kExampleDeveloperId, kExampleUniqueId);

  {
    EXPECT_CALL(*this, OnRegistrationCreated(registration_id, _, _, _, _, _));

    CreateRegistration(registration_id, requests, options, SkBitmap(), &error);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  }

  EXPECT_EQ(
      GetRequestStats(sw_id),
      (ResponseStateStats{2 /* pending_requests */, 0 /* active_requests */,
                          0 /* completed_requests */}));

  // Nothing is downloaded yet.
  std::vector<BackgroundFetchSettledFetch> settled_fetches;
  MatchRequests(registration_id, base::nullopt /* request_to_match */,
                nullptr /* cache_query_params */, true /* match_all */, &error,
                &settled_fetches);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  EXPECT_EQ(settled_fetches.size(), requests.size());

  for (size_t i = 0; i < requests.size(); i++) {
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
      (ResponseStateStats{0 /* pending_requests */, 0 /* active_requests */,
                          requests.size() /* completed_requests */}));

  MatchRequests(registration_id, base::nullopt /* request_to_match */,
                nullptr /* cache_query_params */, true /* match_all */, &error,
                &settled_fetches);

  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  // We are marking the responses as failed in Download Manager.
  EXPECT_EQ(settled_fetches.size(), requests.size());
}

TEST_F(BackgroundFetchDataManagerTest, MatchRequestsFromCache) {
  int64_t sw_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, sw_id);

  BackgroundFetchRegistrationId registration_id(
      sw_id, origin(), kExampleDeveloperId, kExampleUniqueId);
  auto requests = CreateValidRequests(origin(), 2u /* num_requests */);

  BackgroundFetchOptions options;
  blink::mojom::BackgroundFetchError error;
  {
    EXPECT_CALL(*this, OnRegistrationCreated(registration_id, _, _, _, _, _));

    CreateRegistration(registration_id, requests, options, SkBitmap(), &error);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  }

  std::vector<BackgroundFetchSettledFetch> settled_fetches;
  // Nothing is downloaded yet.
  MatchRequests(registration_id, base::nullopt /* request_to_match */,
                nullptr /* cache_query_params */, true /* match_all */, &error,
                &settled_fetches);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  EXPECT_EQ(settled_fetches.size(), requests.size());

  scoped_refptr<BackgroundFetchRequestInfo> request_info;
  PopNextRequest(registration_id, &error, &request_info);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  ASSERT_TRUE(request_info);
  AnnotateRequestInfoWithFakeDownloadManagerData(request_info.get(),
                                                 true /* success */);
  MarkRequestAsComplete(registration_id, request_info.get(), &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

  MatchRequests(registration_id, base::nullopt /* request_to_match */,
                nullptr /* cache_query_params */, false /* match_all */, &error,
                &settled_fetches);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  EXPECT_EQ(settled_fetches.size(), 1u);

  PopNextRequest(registration_id, &error, &request_info);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  ASSERT_TRUE(request_info);
  AnnotateRequestInfoWithFakeDownloadManagerData(request_info.get(),
                                                 true /* success */);
  MarkRequestAsComplete(registration_id, request_info.get(), &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

  MatchRequests(registration_id, base::nullopt /* request_to_match */,
                nullptr /* cache_query_params */, true /* match_all */, &error,
                &settled_fetches);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  ASSERT_EQ(settled_fetches.size(), requests.size());

  // Sanity check that the responses are written to / read from the cache.
  EXPECT_TRUE(MatchCache(requests[0]));
  EXPECT_TRUE(MatchCache(requests[1]));
  EXPECT_EQ(settled_fetches[0].response->cache_storage_cache_name,
            kExampleUniqueId);
  EXPECT_EQ(settled_fetches[1].response->cache_storage_cache_name,
            kExampleUniqueId);

  RestartDataManagerFromPersistentStorage();

  MatchRequests(registration_id, base::nullopt /* request_to_match */,
                nullptr /* cache_query_params */, true /* match_all */, &error,
                &settled_fetches);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  EXPECT_EQ(settled_fetches.size(), requests.size());
}

TEST_F(BackgroundFetchDataManagerTest, MatchRequestsForASpecificRequest) {
  int64_t sw_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, sw_id);

  auto requests = CreateValidRequests(origin(), 2u /* num_requests */);
  BackgroundFetchOptions options;
  blink::mojom::BackgroundFetchError error;
  BackgroundFetchRegistrationId registration_id(
      sw_id, origin(), kExampleDeveloperId, kExampleUniqueId);

  {
    EXPECT_CALL(*this, OnRegistrationCreated(registration_id, _, _, _, _, _));

    CreateRegistration(registration_id, requests, options, SkBitmap(), &error);
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
      (ResponseStateStats{0 /* pending_requests */, 0 /* active_requests */,
                          requests.size() /* completed_requests */}));

  std::vector<BackgroundFetchSettledFetch> settled_fetches;
  MatchRequests(registration_id, requests[0] /* request_to_match */,
                nullptr /* cache_query_params */, false /* match_all */, &error,
                &settled_fetches);
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  // We are marking the responses as failed in Download Manager.
  EXPECT_EQ(settled_fetches.size(), 1u);

  // Try matching a non existing request.
  ServiceWorkerFetchRequest non_existing_request;
  non_existing_request.url = GURL("https://example.com/missing-file.txt");
  MatchRequests(registration_id, non_existing_request /* request_to_match */,
                nullptr /* cache_query_params */, false /* match_all */, &error,
                &settled_fetches);
  EXPECT_TRUE(settled_fetches.empty());
}

TEST_F(BackgroundFetchDataManagerTest, MatchRequestsForAnIncompleteRequest) {
  int64_t sw_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, sw_id);

  auto requests = CreateValidRequests(origin(), 3u /* num_requests */);
  BackgroundFetchOptions options;
  blink::mojom::BackgroundFetchError error;
  BackgroundFetchRegistrationId registration_id(
      sw_id, origin(), kExampleDeveloperId, kExampleUniqueId);

  {
    EXPECT_CALL(*this, OnRegistrationCreated(registration_id, _, _, _, _, _));

    CreateRegistration(registration_id, requests, options, SkBitmap(), &error);
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
      (ResponseStateStats{1 /* pending_requests */, 0 /* active_requests */,
                          requests.size() - 1 /* completed_requests */}));

  std::vector<BackgroundFetchSettledFetch> settled_fetches;
  MatchRequests(registration_id, requests[2] /* request_to_match */,
                nullptr /* cache_query_params */, false /* match_all */, &error,
                &settled_fetches);
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  ASSERT_EQ(settled_fetches.size(), 1u);
  EXPECT_TRUE(settled_fetches[0].response.is_null());
}

TEST_F(BackgroundFetchDataManagerTest, IgnoreMethodAndMatchAll) {
  int64_t sw_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, sw_id);

  std::vector<ServiceWorkerFetchRequest> requests = {
      CreateValidRequestWithMethod(origin(), "GET"),
      CreateValidRequestWithMethod(origin(), "POST")};

  BackgroundFetchOptions options;
  blink::mojom::BackgroundFetchError error;
  BackgroundFetchRegistrationId registration_id(
      sw_id, origin(), kExampleDeveloperId, kExampleUniqueId);

  {
    EXPECT_CALL(*this, OnRegistrationCreated(registration_id, _, _, _, _, _));

    CreateRegistration(registration_id, requests, options, SkBitmap(), &error);
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
      (ResponseStateStats{0 /* pending_requests */, 0 /* active_requests */,
                          requests.size() /* completed_requests */}));

  std::vector<BackgroundFetchSettledFetch> settled_fetches;
  blink::mojom::QueryParamsPtr cache_query_params =
      blink::mojom::QueryParams::New();
  cache_query_params->ignore_method = true;
  MatchRequests(registration_id, requests[0] /* request_to_match */,
                std::move(cache_query_params), true /* match_all */, &error,
                &settled_fetches);

  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  // If the ASSERT below fails, the Cache Storage API implementation has likely
  // changed to distinguish keys by request data other than just the URL.
  // Thank you! Please can you update the 1u below to 2u, or file a bug against
  // component Background Fetch to do so.
  ASSERT_EQ(settled_fetches.size(), 1u);
}

TEST_F(BackgroundFetchDataManagerTest, Cleanup) {
  // Tests that the BackgroundFetchDataManager cleans up registrations
  // marked for deletion.
  int64_t sw_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, sw_id);

  BackgroundFetchRegistrationId registration_id(
      sw_id, origin(), kExampleDeveloperId, kExampleUniqueId);

  std::vector<ServiceWorkerFetchRequest> requests =
      CreateValidRequests(origin(), 2u);
  BackgroundFetchOptions options;
  blink::mojom::BackgroundFetchError error;
  blink::mojom::BackgroundFetchFailureReason failure_reason;

  EXPECT_EQ(0u,
            GetRegistrationUserDataByKeyPrefix(sw_id, kUserDataPrefix).size());
  // Create a registration.
  {
    EXPECT_CALL(*this, OnRegistrationCreated(registration_id, _, _, _, _, _));

    CreateRegistration(registration_id, requests, options, SkBitmap(), &error);
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

  EXPECT_EQ(4u,  // Metadata proto + UI options + remaining pending fetches.
            GetRegistrationUserDataByKeyPrefix(sw_id, kUserDataPrefix).size());

  // Cleanup should delete the registration.
  background_fetch_data_manager_->Cleanup();
  thread_bundle_.RunUntilIdle();
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

  std::vector<ServiceWorkerFetchRequest> requests =
      CreateValidRequests(origin(), 2u);
  BackgroundFetchOptions options;
  options.title = kInitialTitle;
  options.download_total = 42u;
  blink::mojom::BackgroundFetchError error;
  // Register a Background Fetch.
  BackgroundFetchRegistrationId registration_id(
      sw_id, origin(), kExampleDeveloperId, kExampleUniqueId);

  {
    EXPECT_CALL(*this, OnRegistrationCreated(registration_id, _, _, _, _, _));

    CreateRegistration(registration_id, requests, options, CreateTestIcon(),
                       &error);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  }

  {
    std::vector<BackgroundFetchInitializationData> data =
        GetInitializationData();
    ASSERT_EQ(data.size(), 1u);
    const BackgroundFetchInitializationData& init = data[0];

    EXPECT_EQ(init.registration_id, registration_id);
    EXPECT_EQ(init.registration.unique_id, kExampleUniqueId);
    EXPECT_EQ(init.registration.developer_id, kExampleDeveloperId);
    EXPECT_EQ(init.options.title, kInitialTitle);
    EXPECT_EQ(init.options.download_total, 42u);
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
    EXPECT_EQ(ServiceWorkerUtils::SerializeFetchRequestToString(
                  request_info->fetch_request()),
              ServiceWorkerUtils::SerializeFetchRequestToString(
                  init_request_info->fetch_request()));
  }

  // Create another registration.
  BackgroundFetchRegistrationId registration_id2(
      sw_id, origin(), kAlternativeDeveloperId, kAlternativeUniqueId);
  {
    EXPECT_CALL(*this, OnRegistrationCreated(registration_id2, _, _, _, _, _));

    CreateRegistration(registration_id2, requests, options, SkBitmap(), &error);
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

  std::vector<ServiceWorkerFetchRequest> requests =
      CreateValidRequests(origin());
  BackgroundFetchOptions options;

  std::vector<blink::mojom::BackgroundFetchError> errors(5);

  // We expect a single successful registration to be created.
  EXPECT_CALL(*this, OnRegistrationCreated(_, _, _, _, _, _));

  const int num_parallel_creates = 5;

  base::RunLoop run_loop;
  base::RepeatingClosure quit_once_all_finished_closure =
      base::BarrierClosure(num_parallel_creates, run_loop.QuitClosure());

  for (int i = 0; i < num_parallel_creates; i++) {
    // New |unique_id| per iteration, since each is a distinct registration.
    BackgroundFetchRegistrationId registration_id(
        service_worker_registration_id, origin(), kExampleDeveloperId,
        base::GenerateGUID());

    background_fetch_data_manager_->CreateRegistration(
        registration_id, requests, options, SkBitmap(),
        /* start_paused = */ false,
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

  auto requests = CreateValidRequests(origin(), 3u /* num_requests */);
  BackgroundFetchOptions options;
  blink::mojom::BackgroundFetchError error;
  BackgroundFetchRegistrationId registration_id(
      sw_id, origin(), kExampleDeveloperId, kExampleUniqueId);

  {
    base::HistogramTester histogram_tester;
    CreateRegistration(registration_id, requests, options, SkBitmap(), &error);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
    histogram_tester.ExpectBucketCount(
        "BackgroundFetch.Storage.CreateMetadataTask", 0 /* kNone */, 1);
  }

  BackgroundFetchRegistrationId registration_id2(
      sw_id, url::Origin::Create(GURL("https://examplebad.com")),
      kAlternativeDeveloperId, kAlternativeUniqueId);

  {
    base::HistogramTester histogram_tester;
    // This should fail because the Service Worker doesn't exist.
    CreateRegistration(registration_id2, requests, options, SkBitmap(), &error);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::STORAGE_ERROR);
    histogram_tester.ExpectBucketCount(
        "BackgroundFetch.Storage.CreateMetadataTask",
        1 /* kServiceWorkerStorageError */, 1);
  }

  scoped_refptr<BackgroundFetchRequestInfo> request_info;
  PopNextRequest(registration_id, &error, &request_info);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  ASSERT_TRUE(request_info);
  AnnotateRequestInfoWithFakeDownloadManagerData(request_info.get(),
                                                 true /* success */);
  MarkRequestAsComplete(registration_id, request_info.get(), &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

  std::vector<BackgroundFetchSettledFetch> settled_fetches;

  {
    MatchRequests(registration_id, base::nullopt /* request_to_match */,
                  nullptr /* cache_query_params */, false /* match_all */,
                  &error, &settled_fetches);

    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  }

  // Delete all entries to get a CachStorageError.
  for (const auto& request : requests) {
    DeleteFromCache(request);
    ASSERT_FALSE(MatchCache(request));
  }

  {
    base::HistogramTester histogram_tester;
    MatchRequests(registration_id, base::nullopt /* request_to_match */,
                  nullptr /* cache_query_params */, true /* match_all */,
                  &error, &settled_fetches);

    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::STORAGE_ERROR);
    histogram_tester.ExpectBucketCount(
        "BackgroundFetch.Storage.MatchRequestsTask", 2 /* kCacheStorageError */,
        1);
  }
}

}  // namespace content
