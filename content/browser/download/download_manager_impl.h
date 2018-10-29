// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_MANAGER_IMPL_H_
#define CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_MANAGER_IMPL_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/synchronization/lock.h"
#include "build/build_config.h"
#include "components/download/public/common/download_item_impl_delegate.h"
#include "components/download/public/common/download_url_parameters.h"
#include "components/download/public/common/in_progress_download_manager.h"
#include "components/download/public/common/url_download_handler.h"
#include "content/browser/loader/navigation_url_loader.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/download_manager_delegate.h"
#include "content/public/browser/ssl_status.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/mojom/url_loader.mojom.h"

namespace download {
class DownloadFileFactory;
class DownloadItemFactory;
class DownloadItemImpl;
class DownloadRequestHandleInterface;
}

namespace content {
class ResourceContext;

class CONTENT_EXPORT DownloadManagerImpl
    : public DownloadManager,
      public download::UrlDownloadHandler::Delegate,
      public download::InProgressDownloadManager::Delegate,
      private download::DownloadItemImplDelegate {
 public:
  using DownloadItemImplCreated =
      base::Callback<void(download::DownloadItemImpl*)>;

  // Caller guarantees that |net_log| will remain valid
  // for the lifetime of DownloadManagerImpl (until Shutdown() is called).
  explicit DownloadManagerImpl(BrowserContext* browser_context);
  ~DownloadManagerImpl() override;

  // Implementation functions (not part of the DownloadManager interface).

  // Creates a download item for the SavePackage system.
  // Must be called on the UI thread.  Note that the DownloadManager
  // retains ownership.
  virtual void CreateSavePackageDownloadItem(
      const base::FilePath& main_file_path,
      const GURL& page_url,
      const std::string& mime_type,
      int render_process_id,
      int render_frame_id,
      std::unique_ptr<download::DownloadRequestHandleInterface> request_handle,
      const DownloadItemImplCreated& item_created);

  // DownloadManager functions.
  void SetDelegate(DownloadManagerDelegate* delegate) override;
  DownloadManagerDelegate* GetDelegate() const override;
  void Shutdown() override;
  void GetAllDownloads(DownloadVector* result) override;
  void StartDownload(std::unique_ptr<download::DownloadCreateInfo> info,
                     std::unique_ptr<download::InputStream> stream,
                     scoped_refptr<download::DownloadURLLoaderFactoryGetter>
                         url_loader_factory_getter,
                     const download::DownloadUrlParameters::OnStartedCallback&
                         on_started) override;

  int RemoveDownloadsByURLAndTime(
      const base::Callback<bool(const GURL&)>& url_filter,
      base::Time remove_begin,
      base::Time remove_end) override;
  void DownloadUrl(
      std::unique_ptr<download::DownloadUrlParameters> parameters) override;
  void DownloadUrl(std::unique_ptr<download::DownloadUrlParameters> params,
                   std::unique_ptr<storage::BlobDataHandle> blob_data_handle,
                   scoped_refptr<network::SharedURLLoaderFactory>
                       blob_url_loader_factory) override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  download::DownloadItem* CreateDownloadItem(
      const std::string& guid,
      uint32_t id,
      const base::FilePath& current_path,
      const base::FilePath& target_path,
      const std::vector<GURL>& url_chain,
      const GURL& referrer_url,
      const GURL& site_url,
      const GURL& tab_url,
      const GURL& tab_refererr_url,
      const std::string& mime_type,
      const std::string& original_mime_type,
      base::Time start_time,
      base::Time end_time,
      const std::string& etag,
      const std::string& last_modified,
      int64_t received_bytes,
      int64_t total_bytes,
      const std::string& hash,
      download::DownloadItem::DownloadState state,
      download::DownloadDangerType danger_type,
      download::DownloadInterruptReason interrupt_reason,
      bool opened,
      base::Time last_access_time,
      bool transient,
      const std::vector<download::DownloadItem::ReceivedSlice>& received_slices)
      override;
  void PostInitialization(DownloadInitializationDependency dependency) override;
  bool IsManagerInitialized() const override;
  int InProgressCount() const override;
  int NonMaliciousInProgressCount() const override;
  BrowserContext* GetBrowserContext() const override;
  void CheckForHistoryFilesRemoval() override;
  void OnHistoryQueryComplete(
      base::OnceClosure load_history_downloads_cb) override;
  download::DownloadItem* GetDownload(uint32_t id) override;
  download::DownloadItem* GetDownloadByGuid(const std::string& guid) override;

  // UrlDownloadHandler::Delegate implementation.
  void OnUrlDownloadStarted(
      std::unique_ptr<download::DownloadCreateInfo> download_create_info,
      std::unique_ptr<download::InputStream> stream,
      scoped_refptr<download::DownloadURLLoaderFactoryGetter>
          url_loader_factory_getter,
      const download::DownloadUrlParameters::OnStartedCallback& callback)
      override;
  void OnUrlDownloadStopped(download::UrlDownloadHandler* downloader) override;
  void OnUrlDownloadHandlerCreated(
      download::UrlDownloadHandler::UniqueUrlDownloadHandlerPtr downloader)
      override;

  // For testing; specifically, accessed from TestFileErrorInjector.
  void SetDownloadItemFactoryForTesting(
      std::unique_ptr<download::DownloadItemFactory> item_factory);
  void SetDownloadFileFactoryForTesting(
      std::unique_ptr<download::DownloadFileFactory> file_factory);
  virtual download::DownloadFileFactory* GetDownloadFileFactoryForTesting();

  // Helper function to initiate a download request. This function initiates
  // the download using functionality provided by the
  // ResourceDispatcherHostImpl::BeginURLRequest function. The function returns
  // the result of the downoad operation. Please see the
  // DownloadInterruptReason enum for information on possible return values.
  static download::DownloadInterruptReason BeginDownloadRequest(
      std::unique_ptr<net::URLRequest> url_request,
      ResourceContext* resource_context,
      download::DownloadUrlParameters* params);

  // Continue a navigation that ends up to be a download after it reaches the
  // OnResponseStarted() step. It has to be called on the UI thread.
  void InterceptNavigation(
      std::unique_ptr<network::ResourceRequest> resource_request,
      std::vector<GURL> url_chain,
      scoped_refptr<network::ResourceResponse> response,
      network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
      net::CertStatus cert_status,
      int frame_tree_node_id);

 private:
  using DownloadSet = std::set<download::DownloadItem*>;
  using DownloadGuidMap =
      std::unordered_map<std::string, download::DownloadItemImpl*>;
  using DownloadItemImplVector = std::vector<download::DownloadItemImpl*>;

  // For testing.
  friend class DownloadManagerTest;
  friend class DownloadTest;

  void CreateSavePackageDownloadItemWithId(
      const base::FilePath& main_file_path,
      const GURL& page_url,
      const std::string& mime_type,
      int render_process_id,
      int render_frame_id,
      std::unique_ptr<download::DownloadRequestHandleInterface> request_handle,
      const DownloadItemImplCreated& on_started,
      uint32_t id);

  // InProgressDownloadManager::Delegate implementations.
  bool InterceptDownload(const download::DownloadCreateInfo& info) override;
  base::FilePath GetDefaultDownloadDirectory() override;
  void StartDownloadItem(
      std::unique_ptr<download::DownloadCreateInfo> info,
      const download::DownloadUrlParameters::OnStartedCallback& on_started,
      download::InProgressDownloadManager::StartDownloadItemCallback callback)
      override;
  net::URLRequestContextGetter* GetURLRequestContextGetter(
      const download::DownloadCreateInfo& info) override;

  // Called when InProgressDownloadManager is initialzed.
  void OnInProgressDownloadManagerInitialized();

  // Creates a new download item and call |callback|.
  void CreateNewDownloadItemToStart(
      std::unique_ptr<download::DownloadCreateInfo> info,
      const download::DownloadUrlParameters::OnStartedCallback& on_started,
      download::InProgressDownloadManager::StartDownloadItemCallback callback,
      uint32_t id);

  using GetNextIdCallback = base::OnceCallback<void(uint32_t)>;
  // Called to get an ID for a new download. |callback| may be called
  // synchronously.
  void GetNextId(GetNextIdCallback callback);

  // Sets the |next_download_id_| if the |next_id| is larger. Runs all the
  // |id_callbacks_| if both the ID from both history db and in-progress db
  // are retrieved.
  void SetNextId(uint32_t next_id);

  // Called when the next ID from history db is retrieved.
  void OnHistoryNextIdRetrived(uint32_t next_id);

  // Create a new active item based on the info.  Separate from
  // StartDownload() for testing.
  download::DownloadItemImpl* CreateActiveItem(
      uint32_t id,
      const download::DownloadCreateInfo& info);

  // Called with the result of DownloadManagerDelegate::CheckForFileExistence.
  // Updates the state of the file and then notifies this update to the file's
  // observer.
  void OnFileExistenceChecked(uint32_t download_id, bool result);

  // Overridden from DownloadItemImplDelegate
  void DetermineDownloadTarget(download::DownloadItemImpl* item,
                               const DownloadTargetCallback& callback) override;
  bool ShouldCompleteDownload(download::DownloadItemImpl* item,
                              const base::Closure& complete_callback) override;
  bool ShouldOpenFileBasedOnExtension(const base::FilePath& path) override;
  bool ShouldOpenDownload(download::DownloadItemImpl* item,
                          const ShouldOpenDownloadCallback& callback) override;
  void CheckForFileRemoval(download::DownloadItemImpl* download_item) override;
  std::string GetApplicationClientIdForFileScanning() const override;
  void ResumeInterruptedDownload(
      std::unique_ptr<download::DownloadUrlParameters> params,
      const GURL& site_url) override;
  void OpenDownload(download::DownloadItemImpl* download) override;
  bool IsMostRecentDownloadItemAtFilePath(
      download::DownloadItemImpl* download) override;
  void ShowDownloadInShell(download::DownloadItemImpl* download) override;
  void DownloadRemoved(download::DownloadItemImpl* download) override;
  void DownloadInterrupted(download::DownloadItemImpl* download) override;
  base::Optional<download::DownloadEntry> GetInProgressEntry(
      download::DownloadItemImpl* download) override;
  bool IsOffTheRecord() const override;
  void ReportBytesWasted(download::DownloadItemImpl* download) override;

  // Drops a download before it is created.
  void DropDownload();

  // Helper method to start or resume a download.
  void BeginDownloadInternal(
      std::unique_ptr<download::DownloadUrlParameters> params,
      std::unique_ptr<storage::BlobDataHandle> blob_data_handle,
      scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory,
      bool is_new_download,
      const GURL& site_url);

  void InterceptNavigationOnChecksComplete(
      ResourceRequestInfo::WebContentsGetter web_contents_getter,
      std::unique_ptr<network::ResourceRequest> resource_request,
      std::vector<GURL> url_chain,
      scoped_refptr<network::ResourceResponse> response,
      net::CertStatus cert_status,
      network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
      bool is_download_allowed);
  void BeginResourceDownloadOnChecksComplete(
      std::unique_ptr<download::DownloadUrlParameters> params,
      scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory,
      bool is_new_download,
      const GURL& site_url,
      bool is_download_allowed);

  // Whether |next_download_id_| is initialized.
  bool IsNextIdInitialized() const;

#if defined(OS_ANDROID)
  // Check whether a download should be cleared from history. On Android,
  // cancelled and non-resumable interrupted download will be cleaned up to
  // save memory.
  bool ShouldClearDownloadFromDB(download::DownloadItem::DownloadState state,
                                 download::DownloadInterruptReason reason);
#endif  // defined(OS_ANDROID)

  // Factory for creation of downloads items.
  std::unique_ptr<download::DownloadItemFactory> item_factory_;

  // |downloads_| is the owning set for all downloads known to the
  // DownloadManager.  This includes downloads started by the user in
  // this session, downloads initialized from the history system, and
  // "save page as" downloads.
  // TODO(asanka): Remove this container in favor of downloads_by_guid_ as a
  // part of http://crbug.com/593020.
  std::unordered_map<uint32_t, std::unique_ptr<download::DownloadItemImpl>>
      downloads_;

  // Same as the above, but maps from GUID to download item. Note that the
  // container is case sensitive. Hence the key needs to be normalized to
  // upper-case when inserting new elements here. Fortunately for us,
  // DownloadItemImpl already normalizes the string GUID.
  DownloadGuidMap downloads_by_guid_;

  // True if the download manager has been initialized and requires a shutdown.
  bool shutdown_needed_;

  // True if the download manager has been initialized and loaded all the data.
  bool initialized_;

  // Whether the history db and/or in progress cache are initialized.
  bool history_db_initialized_;
  bool in_progress_cache_initialized_;

  // Observers that want to be notified of changes to the set of downloads.
  base::ObserverList<Observer>::Unchecked observers_;

  // Stores information about in-progress download items.
  std::unique_ptr<download::DownloadItem::Observer>
      in_progress_download_observer_;

  // The current active browser context.
  BrowserContext* browser_context_;

  // Allows an embedder to control behavior. Guaranteed to outlive this object.
  DownloadManagerDelegate* delegate_;

  // TODO(qinmin): remove this once network service is enabled by default.
  std::vector<download::UrlDownloadHandler::UniqueUrlDownloadHandlerPtr>
      url_download_handlers_;

  std::unique_ptr<download::InProgressDownloadManager> in_progress_manager_;

  // Callback to run to load all history downloads.
  base::OnceClosure load_history_downloads_cb_;

  // The next download id to issue to new downloads. The |next_download_id_| can
  // only be used when both history and in-progress db have provided their
  // values.
  uint32_t next_download_id_;

  // Whether next download ID from history DB is being retrieved.
  bool is_history_download_id_retrieved_;

  // The download GUIDs that are cleared up on startup.
  std::set<std::string> cleared_download_guids_on_startup_;
  int cancelled_download_cleared_from_history_;
  int interrupted_download_cleared_from_history_;

  // Callbacks to run once download ID is determined.
  using IdCallbackVector = std::vector<std::unique_ptr<GetNextIdCallback>>;
  IdCallbackVector id_callbacks_;

  base::WeakPtrFactory<DownloadManagerImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DownloadManagerImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_MANAGER_IMPL_H_
