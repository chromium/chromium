// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_STORAGE_CREATE_METADATA_TASK_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_STORAGE_CREATE_METADATA_TASK_H_

#include <memory>
#include <string>
#include <vector>

#include "content/browser/background_fetch/background_fetch.pb.h"
#include "content/browser/background_fetch/storage/database_task.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/mojom/background_fetch/background_fetch.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace content {
namespace background_fetch {

// Checks if the registration can be created, then writes the Background
// Fetch metadata in the SW database with corresponding entries in the cache.
class CreateMetadataTask : public DatabaseTask {
 public:
  using CreateMetadataCallback = base::OnceCallback<void(
      blink::mojom::BackgroundFetchError,
      blink::mojom::BackgroundFetchRegistrationDataPtr)>;

  CreateMetadataTask(DatabaseTaskHost* host,
                     const BackgroundFetchRegistrationId& registration_id,
                     std::vector<blink::mojom::FetchAPIRequestPtr> requests,
                     blink::mojom::BackgroundFetchOptionsPtr options,
                     const SkBitmap& icon,
                     bool start_paused,
                     const net::IsolationInfo& isolation_info,
                     CreateMetadataCallback callback);

  CreateMetadataTask(const CreateMetadataTask&) = delete;
  CreateMetadataTask& operator=(const CreateMetadataTask&) = delete;

  ~CreateMetadataTask() override;

  void Start() override;

 private:
  void DidGetCanCreateRegistration(blink::mojom::BackgroundFetchError error,
                                   bool can_create);
  void DidGetIsQuotaAvailable(bool is_available);

  void GetRegistrationUniqueId();

  void DidGetUniqueId(const std::vector<std::string>& data,
                      blink::ServiceWorkerStatusCode status);

  void DidSerializeIcon(std::string serialized_icon);

  void StoreMetadata();

  void DidStoreMetadata(
      blink::ServiceWorkerStatusCode status);

  void InitializeMetadataProto();

  void DidOpenCache(int64_t trace_id, blink::mojom::CacheStorageError error);

  void DidStoreRequests(blink::mojom::CacheStorageVerboseErrorPtr error);

  void FinishWithError(blink::mojom::BackgroundFetchError error) override;

  BackgroundFetchRegistrationId registration_id_;
  std::vector<blink::mojom::FetchAPIRequestPtr> requests_;
  blink::mojom::BackgroundFetchOptionsPtr options_;
  SkBitmap icon_;
  bool start_paused_;
  net::IsolationInfo isolation_info_;
  CreateMetadataCallback callback_;

  std::unique_ptr<proto::BackgroundFetchMetadata> metadata_proto_;

  std::string serialized_icon_;

  base::WeakPtrFactory<CreateMetadataTask> weak_factory_{
      this};  // Keep as last.
};

}  // namespace background_fetch
}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_STORAGE_CREATE_METADATA_TASK_H_
