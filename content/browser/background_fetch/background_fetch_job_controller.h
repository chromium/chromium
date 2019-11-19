// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_JOB_CONTROLLER_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_JOB_CONTROLLER_H_

#include <stdint.h>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "content/browser/background_fetch/background_fetch_delegate_proxy.h"
#include "content/browser/background_fetch/background_fetch_registration_id.h"
#include "content/browser/background_fetch/background_fetch_request_info.h"
#include "content/browser/background_fetch/background_fetch_scheduler.h"
#include "content/common/background_fetch/background_fetch_types.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace content {

class BackgroundFetchDataManager;

// The JobController will be responsible for coordinating communication with the
// DownloadManager. It will get requests from the RequestManager and dispatch
// them to the DownloadService. It lives entirely on the service worker core
// thread.
//
// Lifetime: It is created lazily only once a Background Fetch registration
// starts downloading, and it is destroyed once no more communication with the
// DownloadService or Offline Items Collection is necessary (i.e. once the
// registration has been aborted, or once it has completed/failed and the
// waitUntil promise has been resolved so UpdateUI can no longer be called).
class CONTENT_EXPORT BackgroundFetchJobController
    : public BackgroundFetchDelegateProxy::Controller {
 public:
  using ErrorCallback =
      base::OnceCallback<void(blink::mojom::BackgroundFetchError)>;
  using FinishedCallback =
      base::OnceCallback<void(const BackgroundFetchRegistrationId&,
                              blink::mojom::BackgroundFetchFailureReason,
                              ErrorCallback)>;
  using ProgressCallback = base::RepeatingCallback<void(
      const std::string& unique_id,
      const blink::mojom::BackgroundFetchRegistrationData&)>;
  using RequestStartedCallback =
      base::OnceCallback<void(const BackgroundFetchRegistrationId&,
                              const BackgroundFetchRequestInfo*)>;
  using RequestFinishedCallback =
      base::OnceCallback<void(const BackgroundFetchRegistrationId&,
                              scoped_refptr<BackgroundFetchRequestInfo>)>;

  BackgroundFetchJobController(
      BackgroundFetchDataManager* data_manager,
      BackgroundFetchDelegateProxy* delegate_proxy,
      const BackgroundFetchRegistrationId& registration_id,
      blink::mojom::BackgroundFetchOptionsPtr options,
      const SkBitmap& icon,
      uint64_t bytes_downloaded,
      uint64_t bytes_uploaded,
      uint64_t upload_total,
      ProgressCallback progress_callback,
      FinishedCallback finished_callback);
  ~BackgroundFetchJobController() override;

  // Initializes the job controller with the status of the active and completed
  // downloads, as well as the title to use.
  // Only called when this has been loaded from the database.
  void InitializeRequestStatus(
      int completed_downloads,
      int total_downloads,
      std::vector<scoped_refptr<BackgroundFetchRequestInfo>>
          active_fetch_requests,
      bool start_paused);

  // Gets the number of bytes downloaded/uploaded for jobs that are currently
  // running.
  uint64_t GetInProgressDownloadedBytes();
  uint64_t GetInProgressUploadedBytes();

  // Returns a blink::mojom::BackgroundFetchRegistrationDataPtr object
  // created with member fields.
  blink::mojom::BackgroundFetchRegistrationDataPtr NewRegistrationData() const;

  const BackgroundFetchRegistrationId& registration_id() const {
    return registration_id_;
  }

  // BackgroundFetchDelegateProxy::Controller implementation:
  void DidStartRequest(
      const std::string& guid,
      std::unique_ptr<BackgroundFetchResponse> response) override;
  void DidUpdateRequest(const std::string& guid,
                        uint64_t bytes_uploaded,
                        uint64_t bytes_downloaded) override;
  void DidCompleteRequest(
      const std::string& guid,
      std::unique_ptr<BackgroundFetchResult> result) override;
  void AbortFromDelegate(
      blink::mojom::BackgroundFetchFailureReason failure_reason) override;
  void GetUploadData(
      const std::string& guid,
      BackgroundFetchDelegate::GetUploadDataCallback callback) override;

  // Aborts the fetch. |callback| will run with the result of marking the
  // registration for deletion.
  void Abort(blink::mojom::BackgroundFetchFailureReason failure_reason,
             ErrorCallback callback);

  // Request processing.
  void PopNextRequest(RequestStartedCallback request_started_callback,
                      RequestFinishedCallback request_finished_callback);
  void DidPopNextRequest(
      RequestStartedCallback request_started_callback,
      RequestFinishedCallback request_finished_callback,
      blink::mojom::BackgroundFetchError error,
      scoped_refptr<BackgroundFetchRequestInfo> request_info);
  void StartRequest(scoped_refptr<BackgroundFetchRequestInfo> request,
                    RequestFinishedCallback request_finished_callback);
  void MarkRequestAsComplete(scoped_refptr<BackgroundFetchRequestInfo> request);

  // Whether there are more requests to process as part of this job.
  bool HasMoreRequests();

  int pending_downloads() const { return pending_downloads_; }

 private:
  struct InProgressRequestBytes {
    uint64_t uploaded = 0u;
    uint64_t downloaded = 0u;
  };

  // Called after the request is completely processed, and the next one can be
  // started.
  void DidMarkRequestAsComplete(blink::mojom::BackgroundFetchError error);

  // Notifies the scheduler that the download is complete, and hands the result
  // over.
  void NotifyDownloadComplete(
      scoped_refptr<BackgroundFetchRequestInfo> request);

  // Called when the job completes or has been aborted. |callback| will run
  // with the result of marking the registration for deletion.
  void Finish(blink::mojom::BackgroundFetchFailureReason reason_to_abort,
              ErrorCallback callback);

  void DidGetUploadData(BackgroundFetchDelegate::GetUploadDataCallback callback,
                        blink::mojom::BackgroundFetchError error,
                        blink::mojom::SerializedBlobPtr blob);

  // Manager for interacting with the DB. It is owned by the
  // BackgroundFetchContext.
  BackgroundFetchDataManager* data_manager_;

  // Proxy for interacting with the BackgroundFetchDelegate across thread
  // boundaries. It is owned by the BackgroundFetchContext.
  BackgroundFetchDelegateProxy* delegate_proxy_;

  // A map from the download GUID to the active request.
  std::map<std::string, scoped_refptr<BackgroundFetchRequestInfo>>
      active_request_map_;

  // A map from the download GUID to the in-progress bytes.
  std::map<std::string, InProgressRequestBytes> active_bytes_map_;

  // The registration ID of the fetch this controller represents.
  BackgroundFetchRegistrationId registration_id_;

  // Options for the represented background fetch registration.
  blink::mojom::BackgroundFetchOptionsPtr options_;

  // Icon for the represented background fetch registration.
  SkBitmap icon_;

  // Finished callback to invoke when the active request has finished mapped by
  // its download GUID.
  std::map<std::string, RequestFinishedCallback>
      active_request_finished_callbacks_;

  // Cache of downloaded byte count stored by the DataManager, to enable
  // delivering progress events without having to read from the database.
  uint64_t complete_requests_downloaded_bytes_cache_;

  // Overall number of bytes that have been uploaded.
  uint64_t complete_requests_uploaded_bytes_cache_;

  // Total number of bytes to upload.
  uint64_t upload_total_;

  // Callback run each time download progress updates.
  ProgressCallback progress_callback_;

  // Number of requests that comprise the whole job.
  int total_downloads_ = 0;

  // Number of the requests that have been completed so far.
  int completed_downloads_ = 0;

  // The number of requests that are currently being processed.
  int pending_downloads_ = 0;

  // The reason background fetch was aborted.
  blink::mojom::BackgroundFetchFailureReason failure_reason_ =
      blink::mojom::BackgroundFetchFailureReason::NONE;

  // Custom callback that runs after the controller is finished.
  FinishedCallback finished_callback_;

  base::WeakPtrFactory<BackgroundFetchJobController> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BackgroundFetchJobController);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_JOB_CONTROLLER_H_
