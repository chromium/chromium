// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_STORAGE_GET_REQUEST_BODY_TASK_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_STORAGE_GET_REQUEST_BODY_TASK_H_

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "content/browser/background_fetch/background_fetch_request_info.h"
#include "content/browser/background_fetch/storage/database_task.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/data_pipe_getter.mojom-forward.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"

namespace content {
namespace background_fetch {

class GetRequestBodyTask : public DatabaseTask {
 public:
  using GetRequestBodyCallback = base::OnceCallback<void(
      blink::mojom::BackgroundFetchError,
      mojo::PendingRemote<network::mojom::DataPipeGetter>)>;

  GetRequestBodyTask(
      DatabaseTaskHost* host,
      const BackgroundFetchRegistrationId& registration_id,
      const scoped_refptr<BackgroundFetchRequestInfo>& request_info,
      GetRequestBodyCallback callback);

  GetRequestBodyTask(const GetRequestBodyTask&) = delete;
  GetRequestBodyTask& operator=(const GetRequestBodyTask&) = delete;

  ~GetRequestBodyTask() override;

  // DatabaseTask implementation:
  void Start() override;

 private:
  void DidOpenCache(int64_t trace_id, blink::mojom::CacheStorageError error);
  void DidMatchRequest(int64_t trace_id,
                       blink::mojom::CacheStorageCache::KeysResult result);

  void FinishWithError(blink::mojom::BackgroundFetchError error) override;

  BackgroundFetchRegistrationId registration_id_;
  scoped_refptr<BackgroundFetchRequestInfo> request_info_;
  GetRequestBodyCallback callback_;

  mojo::PendingRemote<network::mojom::DataPipeGetter> data_pipe_getter_;

  base::WeakPtrFactory<GetRequestBodyTask> weak_factory_{
      this};  // Keep as last.
};

}  // namespace background_fetch
}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_STORAGE_GET_REQUEST_BODY_TASK_H_
