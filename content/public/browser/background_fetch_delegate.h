// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BACKGROUND_FETCH_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_BACKGROUND_FETCH_DELEGATE_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_contents.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "third_party/blink/public/mojom/background_fetch/background_fetch.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"

class GURL;

namespace gfx {
class Size;
}  // namespace gfx

namespace net {
class HttpRequestHeaders;
struct NetworkTrafficAnnotationTag;
}  // namespace net

namespace content {
struct BackgroundFetchResponse;
struct BackgroundFetchResult;
struct BackgroundFetchDescription;

enum class BackgroundFetchPermission {
  // Background Fetch is allowed.
  ALLOWED,

  // Background Fetch should be started in a paused state, to let the user
  // decide whether to continue.
  ASK,

  // Background Fetch is blocked.
  BLOCKED,
};

// Interface for launching background fetches. Implementing classes would
// generally interface with the DownloadService or DownloadManager.
// Must only be used on the UI thread and generally expected to be called by the
// BackgroundFetchDelegateProxy.
class CONTENT_EXPORT BackgroundFetchDelegate {
 public:
  using GetIconDisplaySizeCallback = base::OnceCallback<void(const gfx::Size&)>;
  using GetUploadDataCallback =
      base::OnceCallback<void(blink::mojom::SerializedBlobPtr)>;

  // Client interface that a BackgroundFetchDelegate would use to signal the
  // progress of a background fetch.
  class Client {
   public:
    virtual ~Client() {}

    // Called when the entire download job has been cancelled by the delegate,
    // e.g. because the user clicked cancel on a notification.
    virtual void OnJobCancelled(
        const std::string& job_unique_id,
        const std::string& download_guid,
        blink::mojom::BackgroundFetchFailureReason reason_to_abort) = 0;

    // Called after the download has started with the initial response
    // (including headers and URL chain). Always called on the UI thread.
    virtual void OnDownloadStarted(
        const std::string& job_unique_id,
        const std::string& download_guid,
        std::unique_ptr<content::BackgroundFetchResponse> response) = 0;

    // Called during the download to indicate the current progress. Always
    // called on the UI thread.
    virtual void OnDownloadUpdated(const std::string& job_unique_id,
                                   const std::string& download_guid,
                                   uint64_t bytes_uploaded,
                                   uint64_t bytes_downloaded) = 0;

    // Called after the download has completed giving the result including the
    // path to the downloaded file and its size. Always called on the UI thread.
    virtual void OnDownloadComplete(
        const std::string& job_unique_id,
        const std::string& download_guid,
        std::unique_ptr<BackgroundFetchResult> result) = 0;

    // Called when the UI of a background fetch job is activated.
    virtual void OnUIActivated(const std::string& job_unique_id) = 0;

    // Called after the UI has been updated.
    virtual void OnUIUpdated(const std::string& job_unique_id) = 0;

    // Called by the Download Client when it needs the upload data for
    // the given |download_guid|.
    virtual void GetUploadData(const std::string& job_unique_id,
                               const std::string& download_guid,
                               GetUploadDataCallback callback) = 0;
  };

  BackgroundFetchDelegate();

  virtual ~BackgroundFetchDelegate();

  // Gets size of the icon to display with the Background Fetch UI.
  virtual void GetIconDisplaySize(GetIconDisplaySizeCallback callback) = 0;

  // Creates a new download grouping identified by |job_unique_id|. Further
  // downloads started by DownloadUrl will also use this |job_unique_id| so that
  // a notification can be updated with the current status. If the download was
  // already started in a previous browser session, then |current_guids| should
  // contain the GUIDs of in progress downloads, while completed downloads are
  // recorded in |completed_parts|. Updates are communicated to |client|.
  virtual void CreateDownloadJob(
      base::WeakPtr<Client> client,
      std::unique_ptr<BackgroundFetchDescription> fetch_description) = 0;

  // Creates a new download identified by |download_guid| in the download job
  // identified by |job_unique_id|.
  virtual void DownloadUrl(
      const std::string& job_unique_id,
      const std::string& download_guid,
      const std::string& method,
      const GURL& url,
      ::network::mojom::CredentialsMode credentials_mode,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      const net::HttpRequestHeaders& headers,
      bool has_request_body) = 0;

  // Aborts any downloads associated with |job_unique_id|.
  virtual void Abort(const std::string& job_unique_id) = 0;

  // Called after the fetch has completed so that the delegate can clean up.
  virtual void MarkJobComplete(const std::string& job_unique_id) = 0;

  // Updates the UI shown for the fetch job associated with |job_unique_id| to
  // display a new |title| or |icon|.
  virtual void UpdateUI(const std::string& job_unique_id,
                        const std::optional<std::string>& title,
                        const std::optional<SkBitmap>& icon) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BACKGROUND_FETCH_DELEGATE_H_
