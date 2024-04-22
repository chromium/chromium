// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_STORAGE_GET_INITIALIZATION_DATA_TASK_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_STORAGE_GET_INITIALIZATION_DATA_TASK_H_

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "content/browser/background_fetch/background_fetch_registration_id.h"
#include "content/browser/background_fetch/storage/database_task.h"
#include "content/browser/service_worker/service_worker_info.h"
#include "content/common/background_fetch/background_fetch_types.h"
#include "content/common/content_export.h"
#include "net/base/isolation_info.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/mojom/background_fetch/background_fetch.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace content {

class BackgroundFetchRequestInfo;

namespace background_fetch {

// All the information needed to create a JobController and resume the fetch
// after start-up.
struct CONTENT_EXPORT BackgroundFetchInitializationData {
  BackgroundFetchInitializationData();

  BackgroundFetchInitializationData(const BackgroundFetchInitializationData&) =
      delete;
  BackgroundFetchInitializationData& operator=(
      const BackgroundFetchInitializationData&) = delete;

  BackgroundFetchInitializationData(BackgroundFetchInitializationData&&);

  ~BackgroundFetchInitializationData();

  BackgroundFetchRegistrationId registration_id;
  blink::mojom::BackgroundFetchOptionsPtr options =
      blink::mojom::BackgroundFetchOptions::New();
  SkBitmap icon;
  blink::mojom::BackgroundFetchRegistrationDataPtr registration_data =
      blink::mojom::BackgroundFetchRegistrationData::New();
  size_t num_requests;
  size_t num_completed_requests;
  std::vector<scoped_refptr<BackgroundFetchRequestInfo>> active_fetch_requests;
  std::string ui_title;

  // The error, if any, when getting the registration data.
  blink::mojom::BackgroundFetchError error =
      blink::mojom::BackgroundFetchError::NONE;

  std::optional<net::IsolationInfo> isolation_info;
};

using GetInitializationDataCallback =
    base::OnceCallback<void(blink::mojom::BackgroundFetchError,
                            std::vector<BackgroundFetchInitializationData>)>;

// Gets all the data needed to resume fetches. The task starts by getting
// all the <ServiceWorker Registration ID, Background Fetch Unique ID>
// pairs available.
//    * TODO(crbug.com/41394781): Consider persisting which SWIDs contain BGF
//    info.
// Then for every Background Fetch Unique ID the required information is
// queried from the ServiceWorker Database to fill an instance of
// BackgroundFetchInitializationData.
//
// Note: All of this must run in one DatabaseTask, to ensure the
// BackgroundFetchContext is properly initialized with JobControllers before
// running any additional DatabaseTasks and reaching an incorrect state.
class GetInitializationDataTask : public DatabaseTask {
 public:
  using InitializationDataMap =
      std::map<std::string, BackgroundFetchInitializationData>;

  GetInitializationDataTask(DatabaseTaskHost* host,
                            GetInitializationDataCallback callback);

  GetInitializationDataTask(const GetInitializationDataTask&) = delete;
  GetInitializationDataTask& operator=(const GetInitializationDataTask&) =
      delete;

  ~GetInitializationDataTask() override;

  // DatabaseTask implementation:
  void Start() override;

 private:
  void DidGetRegistrations(
      const std::vector<std::pair<int64_t, std::string>>& user_data,
      blink::ServiceWorkerStatusCode status);

  void FinishWithError(blink::mojom::BackgroundFetchError error) override;

  GetInitializationDataCallback callback_;

  // Map from the unique_id to the initialization data.
  InitializationDataMap initialization_data_map_;

  base::WeakPtrFactory<GetInitializationDataTask> weak_factory_{
      this};  // Keep as last.
};

}  // namespace background_fetch

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_STORAGE_GET_INITIALIZATION_DATA_TASK_H_
