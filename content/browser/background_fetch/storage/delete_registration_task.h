// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_STORAGE_DELETE_REGISTRATION_TASK_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_STORAGE_DELETE_REGISTRATION_TASK_H_

#include <string>
#include <vector>

#include "content/browser/background_fetch/storage/database_task.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/cache_storage/cache_storage.mojom.h"

namespace content {
namespace background_fetch {

// Deletes Background Fetch registration entries from the database.
class DeleteRegistrationTask : public background_fetch::DatabaseTask {
 public:
  DeleteRegistrationTask(DatabaseTaskHost* host,
                         int64_t service_worker_registration_id,
                         const blink::StorageKey& storage_key,
                         const std::string& unique_id,
                         HandleBackgroundFetchErrorCallback callback);

  DeleteRegistrationTask(const DeleteRegistrationTask&) = delete;
  DeleteRegistrationTask& operator=(const DeleteRegistrationTask&) = delete;

  ~DeleteRegistrationTask() override;

  void Start() override;

 private:
  void DidGetRegistration(base::OnceClosure done_closure,
                          const std::vector<std::string>& data,
                          blink::ServiceWorkerStatusCode status);

  void DidDeleteRegistration(base::OnceClosure done_closure,
                             blink::ServiceWorkerStatusCode status);

  void DidDeleteCache(base::OnceClosure done_closure,
                      int64_t trace_id,
                      blink::mojom::CacheStorageError error);

  void FinishWithError(blink::mojom::BackgroundFetchError error) override;

  int64_t service_worker_registration_id_;
  blink::StorageKey storage_key_;
  std::string unique_id_;
  HandleBackgroundFetchErrorCallback callback_;

  base::WeakPtrFactory<DeleteRegistrationTask> weak_factory_{
      this};  // Keep as last.
};

}  // namespace background_fetch
}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_STORAGE_DELETE_REGISTRATION_TASK_H_
