// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_STORAGE_MARK_REGISTRATION_FOR_DELETION_TASK_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_STORAGE_MARK_REGISTRATION_FOR_DELETION_TASK_H_

#include <string>
#include <vector>

#include "content/browser/background_fetch/storage/database_task.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"

namespace content {
namespace background_fetch {

// Marks Background Fetch registrations for deletion from the database. This is
// used when some parts of the registration may still be in use and cannot be
// completely removed.
class MarkRegistrationForDeletionTask : public background_fetch::DatabaseTask {
 public:
  using MarkRegistrationForDeletionCallback =
      base::OnceCallback<void(blink::mojom::BackgroundFetchError,
                              blink::mojom::BackgroundFetchFailureReason)>;

  MarkRegistrationForDeletionTask(
      DatabaseTaskHost* host,
      const BackgroundFetchRegistrationId& registration_id,
      bool check_for_failure,
      MarkRegistrationForDeletionCallback callback);

  MarkRegistrationForDeletionTask(const MarkRegistrationForDeletionTask&) =
      delete;
  MarkRegistrationForDeletionTask& operator=(
      const MarkRegistrationForDeletionTask&) = delete;

  ~MarkRegistrationForDeletionTask() override;

  void Start() override;

 private:
  void DidGetActiveUniqueId(const std::vector<std::string>& data,
                            blink::ServiceWorkerStatusCode status);

  void DidDeactivate(blink::ServiceWorkerStatusCode status);

  void DidGetCompletedRequests(const std::vector<std::string>& data,
                               blink::ServiceWorkerStatusCode status);

  void FinishWithError(blink::mojom::BackgroundFetchError error) override;

  BackgroundFetchRegistrationId registration_id_;
  bool check_for_failure_;
  MarkRegistrationForDeletionCallback callback_;

  blink::mojom::BackgroundFetchFailureReason failure_reason_ =
      blink::mojom::BackgroundFetchFailureReason::NONE;

  base::WeakPtrFactory<MarkRegistrationForDeletionTask> weak_factory_{
      this};  // Keep as last.
};

}  // namespace background_fetch
}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_STORAGE_MARK_REGISTRATION_FOR_DELETION_TASK_H_
