// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_STORAGE_GET_DEVELOPER_IDS_TASK_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_STORAGE_GET_DEVELOPER_IDS_TASK_H_

#include <string>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "content/browser/background_fetch/storage/database_task.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/background_fetch/background_fetch.mojom.h"

namespace content {

class ServiceWorkerRegistration;

namespace background_fetch {

// Gets the developer ids for all active registrations - registrations that have
// not completed.
class GetDeveloperIdsTask : public DatabaseTask {
 public:
  GetDeveloperIdsTask(
      DatabaseTaskHost* host,
      int64_t service_worker_registration_id,
      const blink::StorageKey& storage_key,
      blink::mojom::BackgroundFetchService::GetDeveloperIdsCallback callback);

  GetDeveloperIdsTask(const GetDeveloperIdsTask&) = delete;
  GetDeveloperIdsTask& operator=(const GetDeveloperIdsTask&) = delete;

  ~GetDeveloperIdsTask() override;

  // DatabaseTask implementation:
  void Start() override;

 private:
  void DidGetServiceWorkerRegistration(
      blink::ServiceWorkerStatusCode status,
      scoped_refptr<ServiceWorkerRegistration> registration);
  void DidGetUniqueIds(
      blink::ServiceWorkerStatusCode status,
      const base::flat_map<std::string, std::string>& data_map);

  void FinishWithError(blink::mojom::BackgroundFetchError error) override;

  int64_t service_worker_registration_id_;
  blink::StorageKey storage_key_;

  blink::mojom::BackgroundFetchService::GetDeveloperIdsCallback callback_;

  std::vector<std::string> developer_ids_;

  base::WeakPtrFactory<GetDeveloperIdsTask> weak_factory_{
      this};  // Keep as last.
};

}  // namespace background_fetch
}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_STORAGE_GET_DEVELOPER_IDS_TASK_H_
