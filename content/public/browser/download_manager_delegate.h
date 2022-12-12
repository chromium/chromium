// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DOWNLOAD_MANAGER_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_DOWNLOAD_MANAGER_DELEGATE_H_

#include <stdint.h>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/download_item_rename_handler.h"
#include "components/download/public/common/download_url_parameters.h"
#include "components/download/public/common/quarantine_connection.h"
#include "content/common/content_export.h"
#include "content/public/browser/save_page_type.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/origin.h"

class GURL;

namespace content {

class BrowserContext;
class WebContents;

// Called by SavePackage when it creates a download::DownloadItem.
using SavePackageDownloadCreatedCallback =
    base::OnceCallback<void(download::DownloadItem*)>;

// Will be called asynchronously with the results of the ChooseSavePath
// operation.  If the delegate wants notification of the download item created
// in response to this operation, the SavePackageDownloadCreatedCallback will be
// non-null.
using SavePackagePathPickedCallback =
    base::OnceCallback<void(const base::FilePath&,
                            SavePageType,
                            SavePackageDownloadCreatedCallback)>;

// Called with the results of DetermineDownloadTarget().
//
// |target_path| should be set to a non-empty path which is taken to be the
//     final target path for the download. Any file already at this path will be
//     overwritten.
//
// |disposition| and |danger_type| are attributes associated with the download
//     item and can be accessed via the download::DownloadItem accessors.
//
// |intermediate_path| specifies the path to the intermediate file. The download
//     will be written to this path until all the bytes have been written. Upon
//     completion, the file will be renamed to |target_path|.
//     |intermediate_path| could be the same as |target_path|. Both paths must
//     be in the same directory.
//
// |display_name| specifies the suggested file name in case the file name cannot
//     be determined from target_path. Could be empty.
//
// |interrupt_reason| should be set to DOWNLOAD_INTERRUPT_REASON_NONE in
//     order to proceed with the download. DOWNLOAD_INTERRUPT_REASON_USER_CANCEL
//     results in the download being marked cancelled. Any other value results
//     in the download being marked as interrupted. The other fields are only
//     considered valid if |interrupt_reason| is NONE.
using DownloadTargetCallback = base::OnceCallback<void(
    const base::FilePath& target_path,
    download::DownloadItem::TargetDisposition disposition,
    download::DownloadDangerType danger_type,
    download::DownloadItem::InsecureDownloadStatus insecure_download_status,
    const base::FilePath& intermediate_path,
    const base::FilePath& display_name,
    const std::string& mime_type,
    download::DownloadInterruptReason interrupt_reason)>;

// Called when a download delayed by the delegate has completed.
using DownloadOpenDelayedCallback = base::OnceCallback<void(bool)>;

// On failure, |next_id| is equal to kInvalidId.
using DownloadIdCallback = base::OnceCallback<void(uint32_t /* next_id */)>;

// Called on whether a download is allowed to continue.
using CheckDownloadAllowedCallback = base::OnceCallback<void(bool /*allow*/)>;

// Called by CheckSavePackageAllowed when the content of a save package is known
// to be allowed or not.
using SavePackageAllowedCallback = base::OnceCallback<void(bool /*allow*/)>;

// Browser's download manager: manages all downloads and destination view.
class CONTENT_EXPORT DownloadManagerDelegate {
 public:
  // Lets the delegate know that the download manager is shutting down.
  virtual void Shutdown() {}

  // Runs |callback| with a new download id when possible, perhaps
  // synchronously. If this call fails, |callback| will be called with
  // kInvalidId.
  virtual void GetNextId(DownloadIdCallback callback);

  // Called to notify the delegate that a new download |item| requires a
  // download target to be determined. The delegate should return |true| if it
  // will determine the target information and will invoke |callback|. The
  // callback may be invoked directly (synchronously). If this function returns
  // |false|, the download manager will continue the download using a default
  // target path.
  //
  // The state of the |item| shouldn't be modified during the process of
  // filename determination save for external data (GetExternalData() /
  // SetExternalData()).
  //
  // If the download should be canceled, |callback| should be invoked with an
  // empty |target_path| argument.
  virtual bool DetermineDownloadTarget(download::DownloadItem* item,
                                       DownloadTargetCallback* callback);

  // Tests if a file type should be opened automatically. This consider both
  // user and policy settings, and should be called when it doesn't matter
  // what set the auto-open, just if it is set.
  virtual bool ShouldAutomaticallyOpenFile(const GURL& url,
                                           const base::FilePath& path);

  // Tests if a file type should be opened automatically by policy. This
  // should only be used if it matters if the file will auto-open by policy.
  // Generally used to determine if we need to show UI indicating an active
  // policy.
  virtual bool ShouldAutomaticallyOpenFileByPolicy(const GURL& url,
                                                   const base::FilePath& path);

  // Allows the delegate to delay completion of the download.  This function
  // will either return true (in which case the download may complete)
  // or will call the callback passed when the download is ready for
  // completion.  This routine may be called multiple times; once the callback
  // has been called or the function has returned true for a particular
  // download it should continue to return true for that download.
  virtual bool ShouldCompleteDownload(download::DownloadItem* item,
                                      base::OnceClosure complete_callback);

  // Allows the delegate to override opening the download. If this function
  // returns false, the delegate needs to call |callback| when it's done
  // with the item, and is responsible for opening it. When it returns true,
  // the callback will not be used. This function is called after the final
  // rename, but before the download state is set to COMPLETED.
  virtual bool ShouldOpenDownload(download::DownloadItem* item,
                                  DownloadOpenDelayedCallback callback);

  // Checks and hands off the downloading to be handled by another system based
  // on mime type. Returns true if the download was intercepted.
  virtual bool InterceptDownloadIfApplicable(
      const GURL& url,
      const std::string& user_agent,
      const std::string& content_disposition,
      const std::string& mime_type,
      const std::string& request_origin,
      int64_t content_length,
      bool is_transient,
      WebContents* web_contents);

  // Retrieve the directories to save html pages and downloads to.
  virtual void GetSaveDir(BrowserContext* browser_context,
                          base::FilePath* website_save_dir,
                          base::FilePath* download_save_dir) {}

  // Asks the user for the path to save a page. The delegate calls the callback
  // to give the answer.
  virtual void ChooseSavePath(
      WebContents* web_contents,
      const base::FilePath& suggested_path,
      const base::FilePath::StringType& default_extension,
      bool can_save_as_complete,
      SavePackagePathPickedCallback callback) {}

  // Sanitize a filename that's going to be used for saving a subresource of a
  // SavePackage.
  //
  // If the delegate does nothing, the default filename already populated in
  // |filename| will be used. Otherwise, the delegate can update |filename| to
  // the desired filename.
  //
  // |filename| contains a basename with an extension, but without a path. This
  // should be the case on return as well. I.e. |filename| cannot specify a
  // relative path.
  // |source_url| contains the URL from which the download originates and is
  // needed to determine the file's danger level.
  virtual void SanitizeSavePackageResourceName(base::FilePath* filename,
                                               const GURL& source_url) {}

  // Sanitize a download parameters
  //
  // If the delegate does nothing, the default parameters already populated in
  // |params| will be used. Otherwise, the delegate can update |params| to
  // the desired parameters.
  virtual void SanitizeDownloadParameters(
      download::DownloadUrlParameters* params) {}

  // Opens the file associated with this download.
  virtual void OpenDownload(download::DownloadItem* download) {}

  // Shows the download via the OS shell.
  virtual void ShowDownloadInShell(download::DownloadItem* download) {}

  // Return a GUID string used for identifying the application to the system AV
  // function for scanning downloaded files. If no GUID is provided or if the
  // provided GUID is invalid, then the appropriate quarantining will be
  // performed manually without passing the download to the system AV function.
  //
  // This GUID is only used on Windows.
  virtual std::string ApplicationClientIdForFileScanning();

  // Checks whether download is allowed to continue. |check_download_allowed_cb|
  // is called with the decision on completion.
  virtual void CheckDownloadAllowed(
      const WebContents::Getter& web_contents_getter,
      const GURL& url,
      const std::string& request_method,
      absl::optional<url::Origin> request_initiator,
      bool from_download_cross_origin_redirect,
      bool content_initiated,
      CheckDownloadAllowedCallback check_download_allowed_cb);

  // Gets a callback which can connect the download manager to a Quarantine
  // Service instance if available.
  virtual download::QuarantineConnectionCallback
  GetQuarantineConnectionCallback();

  // Gets a handler to perform the rename for a download item.  If no special
  // rename handling is required, don't override this, as the default
  // implementation returns null, which indicates that the default rename
  // handling should be performed.
  virtual std::unique_ptr<download::DownloadItemRenameHandler>
  GetRenameHandlerForDownload(download::DownloadItem* download_item);

  // Gets a |DownloadItem| from the GUID, or null if no such GUID is available.
  virtual download::DownloadItem* GetDownloadByGuid(const std::string& guid);

  // Allows the delegate to delay completion of a SavePackage's final renaming
  // step so it can be disallowed.
  virtual void CheckSavePackageAllowed(
      download::DownloadItem* download_item,
      base::flat_map<base::FilePath, base::FilePath> save_package_files,
      SavePackageAllowedCallback callback);

 protected:
  virtual ~DownloadManagerDelegate();
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DOWNLOAD_MANAGER_DELEGATE_H_
