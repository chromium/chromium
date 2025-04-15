// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_MANAGER_IMPL_H_
#define CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_MANAGER_IMPL_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/sequenced_task_runner_helpers.h"
#include "build/build_config.h"
#include "components/download/public/common/download_item_impl_delegate.h"
#include "components/download/public/common/download_job.h"
#include "components/download/public/common/download_url_parameters.h"
#include "components/download/public/common/in_progress_download_manager.h"
#include "components/download/public/common/url_download_handler.h"
#include "content/browser/loader/navigation_url_loader.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/download_manager_delegate.h"
#include "content/public/browser/ssl_status.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "url/origin.h"

namespace download {
class DownloadFileFactory;
class DownloadItemFactory;
class DownloadItemImpl;
}

// These values are persisted to logs (Download.InitiatedByWindowOpener).
// Entries should not be renumbered and numeric values should never be reused.
// Openee is the tab over which the download is initiated.
// Opener is the tab that opened the openee tab and initiated a download on it.
enum class InitiatedByWindowOpenerType {
  kSameOrigin = 0,
  // Openee and opener are cross origin.
  kCrossOrigin = 1,
  // Openee and opener are cross origin but same site (i.e. same eTLD+1).
  kSameSite = 2,
  // Either the openee or the opener is not HTTP or HTTPS, e.g. about:blank.
  kNonHTTPOrHTTPS = 3,
  kMaxValue = kNonHTTPOrHTTPS
};

namespace content {
class CONTENT_EXPORT DownloadManagerImpl
    : public DownloadManager,
      public download::InProgressDownloadManager::Delegate,
      private download::DownloadItemImplDelegate {
 public:
  using DownloadItemImplCreated =
      base::OnceCallback<void(download::DownloadItemImpl*)>;

  // Caller guarantees that |net_log| will remain valid
  // for the lifetime of DownloadManagerImpl (until Shutdown() is called).
  explicit DownloadManagerImpl(BrowserContext* browser_context);

  DownloadManagerImpl(const DownloadManagerImpl&) = delete;
  DownloadManagerImpl& operator=(const DownloadManagerImpl&) = delete;

  ~DownloadManagerImpl() override;

  // Implementation functions (not part of the DownloadManager interface).

  // Creates a download item for the SavePackage system.
  // Must be called on the UI thread.  Note that the DownloadManager
  // retains ownership.
  void CreateSavePackageDownloadItem(
      const base::FilePath& main_file_path,
      const GURL& page_url,
      const std::string& mime_type,
      int render_process_id,
      int render_frame_id,
      download::DownloadJob::CancelRequestCallback cancel_request_callback,
      DownloadItemImplCreated item_created);

  // DownloadManager functions.
  void SetDelegate(DownloadManagerDelegate* delegate) override;
  DownloadManagerDelegate* GetDelegate() override;
  void Shutdown() override;
  void GetAllDownloads(
      download::SimpleDownloadManager::DownloadVector* result) override;
  void GetUninitializedActiveDownloadsIfAny(
      download::SimpleDownloadManager::DownloadVector* result) override;
  int RemoveDownloadsByURLAndTime(
      const base::RepeatingCallback<bool(const GURL&)>& url_filter,
      base::Time remove_begin,
      base::Time remove_end) override;
  bool CanDownload(download::DownloadUrlParameters* parameters) override;
  void DownloadUrl(
      std::unique_ptr<download::DownloadUrlParameters> parameters) override;
  void DownloadUrl(std::unique_ptr<download::DownloadUrlParameters> params,
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
      const StoragePartitionConfig& storage_partition_config,
      const GURL& tab_url,
      const GURL& tab_refererr_url,
      const std::optional<url::Origin>& request_initiator,
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
  bool IsManagerInitialized() override;
  int InProgressCount() override;
  int BlockingShutdownCount() override;
  BrowserContext* GetBrowserContext() override;
  void CheckForHistoryFilesRemoval() override;
  void OnHistoryQueryComplete(
      base::OnceClosure load_history_downloads_cb) override;
  download::DownloadItem* GetDownload(uint32_t id) override;
  download::DownloadItem* GetDownloadByGuid(const std::string& guid) override;
  void GetNextId(GetNextIdCallback callback) override;
  std::string StoragePartitionConfigToSerializedEmbedderDownloadData(
      const StoragePartitionConfig& storage_partition_config) override;
  StoragePartitionConfig SerializedEmbedderDownloadDataToStoragePartitionConfig(
      const std::string& serialized_embedder_download_data) override;
  StoragePartitionConfig GetStoragePartitionConfigForSiteUrl(
      const GURL& site_url) override;

  void StartDownload(
      std::unique_ptr<download::DownloadCreateInfo> info,
      std::unique_ptr<download::InputStream> stream,
      download::DownloadUrlParameters::OnStartedCallback on_started);

  // For testing; specifically, accessed from TestFileErrorInjector.
  void SetDownloadItemFactoryForTesting(
      std::unique_ptr<download::DownloadItemFactory> item_factory);
  void SetDownloadFileFactoryForTesting(
      std::unique_ptr<download::DownloadFileFactory> file_factory);
  virtual download::DownloadFileFactory* GetDownloadFileFactoryForTesting();

  // Continue a navigation that ends up to be a download after it reaches the
  // OnResponseStarted() step. It has to be called on the UI thread.
  void InterceptNavigation(
      std::unique_ptr<network::ResourceRequest> resource_request,
      std::vector<GURL> url_chain,
      network::mojom::URLResponseHeadPtr response_head,
      mojo::ScopedDataPipeConsumerHandle response_body,
      network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
      net::CertStatus cert_status,
      FrameTreeNodeId frame_tree_node_id,
      bool from_download_cross_origin_redirect);

  // DownloadItemImplDelegate overrides.
  download::QuarantineConnectionCallback GetQuarantineConnectionCallback()
      override;
  std::string GetApplicationClientIdForFileScanning() const override;
  std::unique_ptr<download::DownloadItemRenameHandler>
  GetRenameHandlerForDownload(
      download::DownloadItemImpl* download_item) override;

 private:
  using DownloadSet = std::set<download::DownloadItem*>;
  using DownloadGuidMap =
      std::unordered_map<std::string,
                         raw_ptr<download::DownloadItemImpl, CtnExperimental>>;
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
      download::DownloadJob::CancelRequestCallback cancel_request_callback,
      DownloadItemImplCreated on_started,
      uint32_t id);

  // InProgressDownloadManager::Delegate implementations.
  void OnDownloadsInitialized() override;
  bool InterceptDownload(const download::DownloadCreateInfo& info) override;
  base::FilePath GetDefaultDownloadDirectory() override;
  void StartDownloadItem(
      std::unique_ptr<download::DownloadCreateInfo> info,
      download::DownloadUrlParameters::OnStartedCallback on_started,
      download::InProgressDownloadManager::StartDownloadItemCallback callback)
      override;

  // Creates a new download item and call |callback|.
  void CreateNewDownloadItemToStart(
      std::unique_ptr<download::DownloadCreateInfo> info,
      download::DownloadUrlParameters::OnStartedCallback on_started,
      download::InProgressDownloadManager::StartDownloadItemCallback callback,
      uint32_t id,
      const base::FilePath& duplicate_download_file_path,
      bool duplicate_file_exists);

  // Sets the |next_download_id_| if the |next_id| is larger. Runs all the
  // |id_callbacks_| if both the ID from both history db and in-progress db
  // are retrieved.
  void SetNextId(uint32_t next_id);

  // Called when the next ID from history db is retrieved.
  void OnHistoryNextIdRetrieved(uint32_t next_id);

  // Create a new active item based on the info.  Separate from
  // StartDownload() for testing.
  download::DownloadItemImpl* CreateActiveItem(
      uint32_t id,
      const download::DownloadCreateInfo& info);

  // Called with the result of CheckForFileExistence. Updates the state of the
  // file and then notifies this update to the file's observer.
  void OnFileExistenceChecked(const std::string& guid, bool result);

  // Overridden from DownloadItemImplDelegate
  void DetermineDownloadTarget(
      download::DownloadItemImpl* item,
      download::DownloadTargetCallback callback) override;
  bool ShouldCompleteDownload(download::DownloadItemImpl* item,
                              base::OnceClosure complete_callback) override;
  bool ShouldAutomaticallyOpenFile(const GURL& url,
                                   const base::FilePath& path) override;
  bool ShouldAutomaticallyOpenFileByPolicy(const GURL& url,
                                           const base::FilePath& path) override;
  bool ShouldOpenDownload(download::DownloadItemImpl* item,
                          ShouldOpenDownloadCallback callback) override;
  void CheckForFileRemoval(download::DownloadItemImpl* download_item) override;
  void ResumeInterruptedDownload(
      std::unique_ptr<download::DownloadUrlParameters> params,
      const std::string& serialized_embedder_download_data) override;
  void OpenDownload(download::DownloadItemImpl* download) override;
  void ShowDownloadInShell(download::DownloadItemImpl* download) override;
  void DownloadRemoved(download::DownloadItemImpl* download) override;
  void DownloadInterrupted(download::DownloadItemImpl* download) override;
  bool IsOffTheRecord() const override;
  void ReportBytesWasted(download::DownloadItemImpl* download) override;
  void BindWakeLockProvider(
      mojo::PendingReceiver<device::mojom::WakeLockProvider> receiver) override;

  // Drops a download before it is created.
  void DropDownload();

  // Helper method to start or resume a download.
  void BeginDownloadInternal(
      std::unique_ptr<download::DownloadUrlParameters> params,
      scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory,
      bool is_new_download,
      const std::string& serialized_embedder_download_data);

  void InterceptNavigationOnChecksComplete(
      FrameTreeNodeId frame_tree_node_id,
      std::unique_ptr<network::ResourceRequest> resource_request,
      std::vector<GURL> url_chain,
      net::CertStatus cert_status,
      network::mojom::URLResponseHeadPtr response_head,
      mojo::ScopedDataPipeConsumerHandle response_body,
      network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
      bool is_download_allowed);
  void BeginResourceDownloadOnChecksComplete(
      std::unique_ptr<download::DownloadUrlParameters> params,
      scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory,
      bool is_new_download,
      const StoragePartitionConfig& storage_partition_config,
      bool is_download_allowed);

  // Whether |next_download_id_| is initialized.
  bool IsNextIdInitialized() const;

  // Called when a new download is created.
  void OnDownloadCreated(std::unique_ptr<download::DownloadItemImpl> download);

  // Retrieves a download from |in_progress_downloads_|.
  std::unique_ptr<download::DownloadItemImpl> RetrieveInProgressDownload(
      uint32_t id);

  // Import downloads from |in_progress_downloads_| into |downloads_|, resolve
  // missing download IDs.
  void ImportInProgressDownloads(uint32_t next_id);

  // Called when this object is considered initialized.
  void OnDownloadManagerInitialized();

  // Check whether a download should be cleared from history. Cancelled and
  // non-resumable interrupted download will be cleaned up to save memory.
  bool ShouldClearDownloadFromDB(const GURL& url,
                                 download::DownloadItem::DownloadState state,
                                 download::DownloadInterruptReason reason,
                                 const base::Time& start_time);

  // Called when Id for a new download item is retrieved.
  void OnNewDownloadIdRetrieved(
      std::unique_ptr<download::DownloadCreateInfo> info,
      download::DownloadUrlParameters::OnStartedCallback on_started,
      download::InProgressDownloadManager::StartDownloadItemCallback callback,
      uint32_t id);

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

  // Whether the history db and/or in progress cache are initialized.
  bool history_db_initialized_;
  bool in_progress_cache_initialized_;

  // Observers that want to be notified of changes to the set of downloads.
  base::ObserverList<Observer>::Unchecked observers_;

  // Stores information about in-progress download items.
  std::unique_ptr<download::DownloadItem::Observer>
      in_progress_download_observer_;

  // The current active browser context.
  raw_ptr<BrowserContext> browser_context_;

  // Allows an embedder to control behavior. Guaranteed to outlive this object.
  raw_ptr<DownloadManagerDelegate> delegate_;

  std::unique_ptr<download::InProgressDownloadManager> in_progress_manager_;

  // Callback to run to load all history downloads.
  base::OnceClosure load_history_downloads_cb_;

  // The next download id to issue to new downloads. The |next_download_id_| can
  // only be used when both history and in-progress db have provided their
  // values.
  uint32_t next_download_id_;

  // Whether next download ID from history DB is being retrieved.
  bool is_history_download_id_retrieved_;

  // Whether new download should be persisted to the in progress download
  // database.
  bool should_persist_new_download_;

  // The download GUIDs that are cleared up on startup.
  std::set<std::string> cleared_download_guids_on_startup_;

  // In progress downloads returned by |in_progress_manager_| that are not yet
  // added to |downloads_|. If a download was started without launching full
  // browser process, its ID will be invalid. DownloadManager will assign new
  // ID to it when importing all downloads.
  std::vector<std::unique_ptr<download::DownloadItemImpl>>
      in_progress_downloads_;

  // Callbacks to run once download ID is determined.
  using IdCallbackVector = std::vector<std::unique_ptr<GetNextIdCallback>>;
  IdCallbackVector id_callbacks_;

  // SequencedTaskRunner to check for file existence. A sequence is used so
  // that a large download history doesn't cause a large number of concurrent
  // disk operations.
  const scoped_refptr<base::SequencedTaskRunner> disk_access_task_runner_;

  // DownloadItem for which a query is queued in the |disk_access_task_runner_|.
  std::set<std::string> pending_disk_access_query_;

  base::WeakPtrFactory<DownloadManagerImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_MANAGER_IMPL_H_
