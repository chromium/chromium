// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_STORAGE_GET_REGISTRATION_TASK_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_STORAGE_GET_REGISTRATION_TASK_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "content/browser/background_fetch/background_fetch.pb.h"
#include "content/browser/background_fetch/background_fetch_registration_id.h"
#include "content/browser/background_fetch/storage/database_task.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content {
namespace background_fetch {

// Gets an active Background Fetch metadata entry from the database.
class GetRegistrationTask : public DatabaseTask {
 public:
  using GetRegistrationCallback = base::OnceCallback<void(
      blink::mojom::BackgroundFetchError,
      BackgroundFetchRegistrationId,
      blink::mojom::BackgroundFetchRegistrationDataPtr)>;

  GetRegistrationTask(DatabaseTaskHost* host,
                      int64_t service_worker_registration_id,
                      const blink::StorageKey& storage_key,
                      const std::string& developer_id,
                      GetRegistrationCallback callback);

  GetRegistrationTask(const GetRegistrationTask&) = delete;
  GetRegistrationTask& operator=(const GetRegistrationTask&) = delete;

  ~GetRegistrationTask() override;

  // DatabaseTask implementation:
  void Start() override;

 private:
  void DidGetMetadata(
      blink::mojom::BackgroundFetchError error,
      std::unique_ptr<proto::BackgroundFetchMetadata> metadata_proto);

  void FinishWithError(blink::mojom::BackgroundFetchError error) override;

  int64_t service_worker_registration_id_;
  blink::StorageKey storage_key_;
  std::string developer_id_;

  GetRegistrationCallback callback_;

  std::unique_ptr<proto::BackgroundFetchMetadata> metadata_proto_;

  base::WeakPtrFactory<GetRegistrationTask> weak_factory_{
      this};  // Keep as last.
};

}  // namespace background_fetch
}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_STORAGE_GET_REGISTRATION_TASK_H_
