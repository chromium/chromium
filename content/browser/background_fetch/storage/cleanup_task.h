// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_STORAGE_CLEANUP_TASK_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_STORAGE_CLEANUP_TASK_H_

#include <string>
#include <utility>
#include <vector>

#include "content/browser/background_fetch/storage/database_task.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"

namespace content {
namespace background_fetch {

// Deletes inactive registrations marked for deletion.
class CleanupTask : public background_fetch::DatabaseTask {
 public:
  explicit CleanupTask(DatabaseTaskHost* host);

  ~CleanupTask() override;

  void Start() override;

 private:
  void DidGetRegistrations(
      const std::vector<std::pair<int64_t, std::string>>& registration_data,
      blink::ServiceWorkerStatusCode status);

  void DidGetActiveUniqueIds(
      const std::vector<std::pair<int64_t, std::string>>& registration_data,
      const std::vector<std::pair<int64_t, std::string>>& active_unique_id_data,
      blink::ServiceWorkerStatusCode status);

  void FinishWithError(blink::mojom::BackgroundFetchError error) override;

  std::string HistogramName() const override;

  base::WeakPtrFactory<CleanupTask> weak_factory_{this};  // Keep as last.

  DISALLOW_COPY_AND_ASSIGN(CleanupTask);
};

}  // namespace background_fetch
}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_STORAGE_CLEANUP_TASK_H_
