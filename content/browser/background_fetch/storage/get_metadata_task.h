// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_STORAGE_GET_METADATA_TASK_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_STORAGE_GET_METADATA_TASK_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "content/browser/background_fetch/background_fetch.pb.h"
#include "content/browser/background_fetch/storage/database_task.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"

namespace blink {
class StorageKey;
}  // namespace blink

namespace content {
namespace background_fetch {

// Gets an active Background Fetch metadata entry from the database.
class GetMetadataTask : public DatabaseTask {
 public:
  using GetMetadataCallback =
      base::OnceCallback<void(blink::mojom::BackgroundFetchError,
                              std::unique_ptr<proto::BackgroundFetchMetadata>)>;

  GetMetadataTask(DatabaseTaskHost* host,
                  int64_t service_worker_registration_id,
                  const blink::StorageKey& storage_key,
                  const std::string& developer_id,
                  GetMetadataCallback callback);

  GetMetadataTask(const GetMetadataTask&) = delete;
  GetMetadataTask& operator=(const GetMetadataTask&) = delete;

  ~GetMetadataTask() override;

  // DatabaseTask implementation:
  void Start() override;

 private:
  void DidGetUniqueId(const std::vector<std::string>& data,
                      blink::ServiceWorkerStatusCode status);

  void DidGetMetadata(const std::vector<std::string>& data,
                      blink::ServiceWorkerStatusCode status);

  void ProcessMetadata(const std::string& metadata);

  void FinishWithError(blink::mojom::BackgroundFetchError error) override;

  int64_t service_worker_registration_id_;
  blink::StorageKey storage_key_;
  std::string developer_id_;

  GetMetadataCallback callback_;

  std::unique_ptr<proto::BackgroundFetchMetadata> metadata_proto_;

  base::WeakPtrFactory<GetMetadataTask> weak_factory_{this};  // Keep as last.
};

}  // namespace background_fetch
}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_STORAGE_GET_METADATA_TASK_H_
