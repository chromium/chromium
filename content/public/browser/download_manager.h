// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The DownloadManager object manages the process of downloading, including
// updates to the history system and providing the information for displaying
// the downloads view in the Destinations tab. There is one DownloadManager per
// active browser context in Chrome.
//
// Download observers:
// Objects that are interested in notifications about new downloads, or progress
// updates for a given download must implement one of the download observer
// interfaces:
//   DownloadManager::Observer:
//     - allows observers, primarily views, to be notified when changes to the
//       set of all downloads (such as new downloads, or deletes) occur
// Use AddObserver() / RemoveObserver() on the appropriate download object to
// receive state updates.
//
// Download state persistence:
// The DownloadManager uses the history service for storing persistent
// information about the state of all downloads. The history system maintains a
// separate table for this called 'downloads'. At the point that the
// DownloadManager is constructed, we query the history service for the state of
// all persisted downloads.

#ifndef CONTENT_PUBLIC_BROWSER_DOWNLOAD_MANAGER_H_
#define CONTENT_PUBLIC_BROWSER_DOWNLOAD_MANAGER_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/download/public/common/download_interrupt_reasons.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/download_stream.mojom-forward.h"
#include "components/download/public/common/download_url_parameters.h"
#include "components/download/public/common/input_stream.h"
#include "components/download/public/common/simple_download_manager.h"
#include "content/common/content_export.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/origin.h"

class GURL;

namespace content {

class BrowserContext;
class DownloadManagerDelegate;
class StoragePartitionConfig;

// Browser's download manager: manages all downloads and destination view.
class CONTENT_EXPORT DownloadManager : public base::SupportsUserData::Data,
                                       public download::SimpleDownloadManager {
 public:
  ~DownloadManager() override {}

  // Returns the task runner that's used for all download-related blocking
  // tasks, such as file IO.
  static scoped_refptr<base::SequencedTaskRunner> GetTaskRunner();

  // Sets/Gets the delegate for this DownloadManager. The delegate has to live
  // past its Shutdown method being called (by the DownloadManager).
  virtual void SetDelegate(DownloadManagerDelegate* delegate) = 0;
  virtual DownloadManagerDelegate* GetDelegate() = 0;

  // Shutdown the download manager. Content calls this when BrowserContext is
  // being destructed. If the embedder needs this to be called earlier, it can
  // call it. In that case, the delegate's Shutdown() method will only be called
  // once.
  virtual void Shutdown() = 0;

  // Interface to implement for observers that wish to be informed of changes
  // to the DownloadManager's collection of downloads.
  class CONTENT_EXPORT Observer {
   public:
    // A download::DownloadItem was created. This item may be visible before the
    // filename is determined; in this case the return value of
    // GetTargetFileName() will be null.  This method may be called an arbitrary
    // number of times, e.g. when loading history on startup.  As a result,
    // consumers should avoid doing large amounts of work in
    // OnDownloadCreated().  TODO(<whoever>): When we've fully specified the
    // possible states of the download::DownloadItem in download_item.h, we
    // should remove the caveat above.
    virtual void OnDownloadCreated(DownloadManager* manager,
                                   download::DownloadItem* item) {}

    // Called when the download manager intercepted a download navigation but
    // didn't create the download item. Possible reasons:
    // 1. |delegate| is null.
    // 2. |delegate| doesn't allow the download.
    virtual void OnDownloadDropped(DownloadManager* manager) {}

    // Called when the download manager has finished loading the data.
    virtual void OnManagerInitialized() {}

    // Called when the DownloadManager is being destroyed to prevent Observers
    // from calling back to a stale pointer.
    virtual void ManagerGoingDown(DownloadManager* manager) {}

   protected:
    virtual ~Observer() {}
  };

  // Remove downloads whose URLs match the |url_filter| and are within
  // the given time constraints - after remove_begin (inclusive) and before
  // remove_end (exclusive). You may pass in null Time values to do an unbounded
  // delete in either direction.
  virtual int RemoveDownloadsByURLAndTime(
      const base::RepeatingCallback<bool(const GURL&)>& url_filter,
      base::Time remove_begin,
      base::Time remove_end) = 0;

  using SimpleDownloadManager::DownloadUrl;
  // For downloads of blob URLs, the caller can pass a URLLoaderFactory to
  // use to load the Blob URL. If none is specified and the blob URL cannot be
  // mapped to a blob by the time the download request starts, then the download
  // will fail.
  virtual void DownloadUrl(
      std::unique_ptr<download::DownloadUrlParameters> parameters,
      scoped_refptr<network::SharedURLLoaderFactory>
          blob_url_loader_factory) = 0;

  // Allow objects to observe the download creation process.
  virtual void AddObserver(Observer* observer) = 0;

  // Remove a download observer from ourself.
  virtual void RemoveObserver(Observer* observer) = 0;

  // Called by the embedder, after creating the download manager, to let it know
  // about downloads from previous runs of the browser.
  virtual download::DownloadItem* CreateDownloadItem(
      const std::string& guid,
      uint32_t id,
      const base::FilePath& current_path,
      const base::FilePath& target_path,
      const std::vector<GURL>& url_chain,
      const GURL& referrer_url,
      const StoragePartitionConfig& storage_partition_config,
      const GURL& tab_url,
      const GURL& tab_referrer_url,
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
      const std::vector<download::DownloadItem::ReceivedSlice>&
          received_slices) = 0;

  // Enum to describe which dependency was initialized in PostInitialization.
  enum DownloadInitializationDependency {
    DOWNLOAD_INITIALIZATION_DEPENDENCY_NONE,
    DOWNLOAD_INITIALIZATION_DEPENDENCY_HISTORY_DB,
    DOWNLOAD_INITIALIZATION_DEPENDENCY_IN_PROGRESS_CACHE,
  };

  // Called when download manager has loaded all the data, once when the history
  // db is initialized and once when the in-progress cache is initialized.
  virtual void PostInitialization(
      DownloadInitializationDependency dependency) = 0;

  // Returns if the manager has been initialized and loaded all the data.
  virtual bool IsManagerInitialized() = 0;

  // The number of in progress (including paused) downloads.
  // Performance note: this loops over all items. If profiling finds that this
  // is too slow, use an AllDownloadItemNotifier to count in-progress items.
  virtual int InProgressCount() = 0;

  // The number of in progress (including paused) downloads that should block
  // shutdown. This excludes downloads that are marked as malicious.
  // Performance note: this loops over all items. If profiling finds that this
  // is too slow, use an AllDownloadItemNotifier to count in-progress items.
  virtual int BlockingShutdownCount() = 0;

  virtual BrowserContext* GetBrowserContext() = 0;

  // Called when download history query completes. Call
  // |load_history_downloads_cb| to load all the history downloads.
  virtual void OnHistoryQueryComplete(
      base::OnceClosure load_history_downloads_cb) = 0;

  // Get the download item for |id| if present, no matter what type of download
  // it is or state it's in.
  // DEPRECATED: Don't add new callers for GetDownload(uint32_t). Instead keep
  // track of the GUID and use GetDownloadByGuid(), or observe the
  // download::DownloadItem if you need to keep track of a specific download.
  // (http://crbug.com/593020)
  virtual download::DownloadItem* GetDownload(uint32_t id) = 0;

  using GetNextIdCallback = base::OnceCallback<void(uint32_t)>;
  // Called to get an ID for a new download. |callback| may be called
  // synchronously.
  virtual void GetNextId(GetNextIdCallback callback) = 0;

  // Called to convert between a StoragePartitionConfig and a serialized
  // proto::EmbedderDownloadData. The serialized proto::EmbedderDownloadData is
  // written to the downloads database.
  virtual std::string StoragePartitionConfigToSerializedEmbedderDownloadData(
      const StoragePartitionConfig& storage_partition_config) = 0;
  virtual StoragePartitionConfig
  SerializedEmbedderDownloadDataToStoragePartitionConfig(
      const std::string& serialized_embedder_download_data) = 0;

  // Called to get the proper StoragePartitionConfig that corresponds to the
  // given site URL. This method is used in DownloadHistory to convert download
  // history entries containing just site URLs to DownloadItem objects that no
  // longer use site URL. The download history database is not able to migrate
  // away from site URL because it is shared by all platforms, therefore it
  // cannot reference StoragePartitionConfig since it is a content class.
  // See https://crbug.com/1258193 for more details.
  virtual StoragePartitionConfig GetStoragePartitionConfigForSiteUrl(
      const GURL& site_url) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DOWNLOAD_MANAGER_H_
