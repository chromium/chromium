// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_SAVE_PACKAGE_H_
#define CONTENT_BROWSER_DOWNLOAD_SAVE_PACKAGE_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/download/public/common/download_item.h"
#include "content/browser/download/save_types.h"
#include "content/common/content_export.h"
#include "content/public/browser/download_manager_delegate.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "content/public/browser/save_page_type.h"
#include "content/public/common/referrer.h"
#include "net/base/isolation_info.h"
#include "net/base/net_errors.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-forward.h"
#include "url/gurl.h"

class GURL;

namespace download {
class DownloadItemImpl;
}

namespace content {
class DownloadManagerImpl;
class PageImpl;
class FrameTreeNode;
class RenderFrameHostImpl;
class SaveFileManager;
class SaveItem;
class SavePackage;

// SavePackage manages the process of saving a page as only-HTML, complete-HTML
// or MHTML and provides status information about the job.
// - only-html: the web page is saved to a single HTML file excluding
// sub-resources and sub-frames
// - complete-html: the web page's main frame HTML is saved to the user selected
// file and a directory for the auxiliary files such as all sub-frame html
// files, image files, css files and js files is created
// - MHTML: the main frame and all auxiliary files are stored a single text
//   file using the MHTML format.
//
// Each page saving job may include one or multiple files which need to be
// saved. Each file is represented by a SaveItem, and all SaveItems are owned
// by the SavePackage. SaveItems are created when a user initiates a page
// saving job, and exist for the duration of one contents's life time.
class CONTENT_EXPORT SavePackage final
    : public base::RefCountedThreadSafe<SavePackage> {
 public:
  enum WaitState {
    // State when created but not initialized.
    INITIALIZE = 0,
    // State when after initializing, but not yet saving.
    START_PROCESS,
    // Waiting on a list of savable resources from the backend.
    RESOURCES_LIST,
    // Waiting for data sent from net IO or from file system.
    NET_FILES,
    // Waiting for html DOM data sent from render process.
    HTML_DATA,
    // Saving page finished successfully.
    SUCCESSFUL,
    // Failed to save page.
    FAILED
  };

  static const base::FilePath::CharType kDefaultHtmlExtension[];

  // Constructor for user initiated page saving. This constructor results in a
  // SavePackage that will generate and sanitize a suggested name for the user
  // in the "Save As" dialog box.
  explicit SavePackage(PageImpl& page);

  SavePackage(const SavePackage&) = delete;
  SavePackage& operator=(const SavePackage&) = delete;

  // Initialize the SavePackage. Returns true if it initializes properly.  Need
  // to make sure that this method must be called in the UI thread because using
  // g_browser_process on a non-UI thread can cause crashes during shutdown.
  // |cb| will be called when the download::DownloadItem is created, before data
  // is written to disk.
  bool Init(SavePackageDownloadCreatedCallback cb);

  // Cancel all in progress request, might be called by user or internal error.
  void Cancel(bool user_action, bool cancel_download_item = true);

  void Finish();

  // Notifications sent from the download sequence to the UI thread.
  void StartSave(const SaveFileCreateInfo* info);
  bool UpdateSaveProgress(SaveItemId save_item_id,
                          int64_t size,
                          bool write_success);
  // Called for updating end state.
  void SaveFinished(SaveItemId save_item_id, int64_t size, bool is_success);
  void SaveCanceled(const SaveItem* save_item);

  // Calculate the percentage of whole save page job.
  // Rough percent complete, -1 means we don't know (since we didn't receive a
  // total size).
  int PercentComplete();

  bool canceled() const { return user_canceled_ || disk_error_occurred_; }
  bool finished() const { return finished_; }
  SavePageType save_type() const { return save_type_; }

  SavePackageId id() const { return unique_id_; }

  void GetSaveInfo();

  // Response from |sender| frame to GetSavableResourceLinks request.
  void SavableResourceLinksResponse(
      RenderFrameHostImpl* sender,
      const std::vector<GURL>& resources_list,
      blink::mojom::ReferrerPtr referrer,
      const std::vector<blink::mojom::SavableSubframePtr>& subframes);

  // Response to GetSavableResourceLinks that indicates an error when processing
  // the frame associated with |sender|.
  void SavableResourceLinksError(RenderFrameHostImpl* sender);

 private:
  friend class base::RefCountedThreadSafe<SavePackage>;

  // Friends for testing. Needed for accessing the test-only constructor below.
  friend class SavePackageTest;
  friend class WebContentsImpl;
  FRIEND_TEST_ALL_PREFIXES(SavePackageTest, TestSuggestedSaveNames);
  FRIEND_TEST_ALL_PREFIXES(SavePackageTest, TestLongSafePureFilename);
  FRIEND_TEST_ALL_PREFIXES(SavePackageFencedFrameTest,
                           DontRequestSavableResourcesFromFencedFrames);
  FRIEND_TEST_ALL_PREFIXES(SavePackageBrowserTest, ImplicitCancel);
  FRIEND_TEST_ALL_PREFIXES(SavePackageBrowserTest, ExplicitCancel);
  FRIEND_TEST_ALL_PREFIXES(SavePackageBrowserTest, Reload);
  FRIEND_TEST_ALL_PREFIXES(SavePackageBrowserTest, DownloadItemDestroyed);

  // Map from SaveItem::id() (aka save_item_id) into a SaveItem.
  using SaveItemIdMap = std::
      unordered_map<SaveItemId, std::unique_ptr<SaveItem>, SaveItemId::Hasher>;

  using FileNameSet = std::set<base::FilePath::StringType,
                               bool (*)(base::FilePath::StringPieceType,
                                        base::FilePath::StringPieceType)>;

  using FileNameCountMap =
      std::unordered_map<base::FilePath::StringType, uint32_t>;

  // Used only for testing. Bypasses the file and directory name generation /
  // sanitization by providing well known paths better suited for tests.
  SavePackage(PageImpl& page,
              SavePageType save_type,
              const base::FilePath& file_full_path,
              const base::FilePath& directory_full_path);

  ~SavePackage();

  void InitWithDownloadItem(
      SavePackageDownloadCreatedCallback download_created_callback,
      download::DownloadItemImpl* item);

  // Callback for WebContents::GenerateMHTML().
  void OnMHTMLGenerated(int64_t size);

  // Notes from Init() above applies here as well.
  void InternalInit();

  void Stop(bool cancel_download_item);
  void CheckFinish();

  // Callback used to check if renaming is allowed once paths to saved filed
  // have been obtained from `file_manager`.
  void CheckRenameAllowedForPaths(
      base::flat_map<base::FilePath, base::FilePath> tmp_paths_to_final_paths);

  // Called by CheckRenameAllowedForPaths after checking if the final renaming
  // step should happen or not.
  void RenameIfAllowed(bool allowed);

  // Clears the associated page.
  void ClearPage();

  // Initiate a saving job of a specific URL. We send the request to
  // SaveFileManager, which will dispatch it to different approach according to
  // the save source. |process_all_remaining_items| indicates whether we need to
  // save all remaining items.
  void SaveNextFile(bool process_all_remainder_items);

  // Continue processing the save page job after one SaveItem has been finished.
  void DoSavingProcess();

  // Update the download history of this item upon completion.
  void FinalizeDownloadEntry();

  // Return max length of a path for a specific base directory.
  // This is needed on POSIX, which restrict the length of file names in
  // addition to the restriction on the length of path names.
  // |base_dir| is assumed to be a directory name with no trailing slash.
  static uint32_t GetMaxPathLengthForDirectory(const base::FilePath& base_dir);

  // Truncates a filename to fit length constraints.
  //
  // |directory|    : Directory containing target file.
  // |extension|    : Extension.
  // |max_path_len| : Maximum size allowed for |len(directory + base_name +
  //                  extension|.
  // |base_name|    : Variable portion. The length of this component will be
  //                  adjusted to fit the length constraints described at
  //                  |max_path_len| above.
  //
  // Returns true if |base_name| could be successfully adjusted to fit the
  // aforementioned constraints, or false otherwise.
  // TODO(asanka): This function is wrong. |base_name| cannot be truncated
  //   without knowing its encoding and truncation has to be performed on
  //   character boundaries. Also the implementation doesn't look up the actual
  //   path constraints and instead uses hard coded constants. crbug.com/618737
  static bool TruncateBaseNameToFitPathConstraints(
      const base::FilePath& directory,
      const base::FilePath::StringType& extension,
      uint32_t max_path_len,
      base::FilePath::StringType* base_name);

  // Create a file name based on the response from the server.
  bool GenerateFileName(const std::string& disposition,
                        const GURL& url,
                        bool need_html_ext,
                        base::FilePath::StringType* generated_name);

  // Main routine that initiates asking all frames for their savable resources.
  //
  // Responses are received asynchronously by OnSavableResourceLinks... methods
  // and pending responses are counted/tracked by
  // CompleteSavableResourceLinksResponse.
  //
  // SavableResourceLinksResponse creates SaveItems for each savable resource
  // and each subframe - these SaveItems get enqueued into |waiting_item_queue_|
  // with the help of CreatePendingSaveItem, EnqueueSavableResource,
  // EnqueueFrame.
  void GetSavableResourceLinks();
  void GetSavableResourceLinksForRenderFrameHost(RenderFrameHostImpl* rfh);

  // Helper for finding or creating a SaveItem with the given parameters.
  SaveItem* CreatePendingSaveItem(
      FrameTreeNodeId container_frame_tree_node_id,
      FrameTreeNodeId save_item_frame_tree_node_id,
      const GURL& url,
      const Referrer& referrer,
      SaveFileCreateInfo::SaveFileSource save_source);

  // Helper for finding a SaveItem with the given url, or falling back to
  // creating a SaveItem with the given parameters.
  void CreatePendingSaveItemDeduplicatingByUrl(
      FrameTreeNodeId container_frame_tree_node_id,
      FrameTreeNodeId save_item_frame_tree_node_id,
      const GURL& url,
      const Referrer& referrer,
      SaveFileCreateInfo::SaveFileSource save_source);

  // Helper to enqueue a savable resource reported by GetSavableResourceLinks.
  void EnqueueSavableResource(FrameTreeNodeId container_frame_tree_node_id,
                              const GURL& url,
                              const Referrer& referrer);
  // Helper to enqueue a subframe reported by GetSavableResourceLinks.
  void EnqueueFrame(FrameTreeNodeId container_frame_tree_node_id,
                    FrameTreeNodeId frame_tree_node_id,
                    const GURL& frame_original_url);

  // Helper tracking how many |number_of_frames_pending_response_| we have
  // left and kicking off the next phase after we got all the
  // SavableResourceLinks reply messages we were waiting for.
  void CompleteSavableResourceLinksResponse();

  // For each frame in the current page, ask the renderer process associated
  // with that frame to serialize that frame into html.
  void GetSerializedHtmlWithLocalLinks();

  // Ask renderer process to serialize |target_tree_node| into html data
  // with resource links replaced with a link to a locally saved copy.
  void GetSerializedHtmlWithLocalLinksForFrame(FrameTreeNode* target_tree_node);

  // Called when receiving a response to GetSerializedHtmlWithLocalLinks() from
  // the renderer, including in |data| the amount of content serialized so far.
  void OnDidReceiveSerializedHtmlData(base::WeakPtr<RenderFrameHostImpl> sender,
                                      const std::string& data);

  // Called right after the last  response to GetSerializedHtmlWithLocalLinks()
  // has been received from the renderer, so that the SaveFileManager can also
  // be notified that the entire process is over.
  void OnDidFinishedSerializingHtmlData(
      base::WeakPtr<RenderFrameHostImpl> sender);

  // Helper function to lookup the right SaveItem for a given RenderFrameHost
  // from the |frame_tree_node_id_to_save_item_| map. Used to avoid duplication
  // and meant to be used from the DidReceiveData() and Done() callbacks used
  // along with the call to the remote GetSerializedHtmlWithLocalLinks() method.
  const SaveItem* LookupSaveItemForSender(
      base::WeakPtr<RenderFrameHostImpl> sender);

  // Look up SaveItem by save item id from in progress map.
  SaveItem* LookupInProgressSaveItem(SaveItemId save_item_id);

  // Remove SaveItem from in progress map and put it to saved map.
  void PutInProgressItemToSavedMap(SaveItem* save_item);

  // Retrieves the URL to be saved from the main frame.
  static GURL GetUrlToBeSaved(RenderFrameHost* main_frame);

  static base::FilePath CreateDirectoryOnFileThread(
      const std::u16string& title,
      const GURL& page_url,
      bool can_save_as_complete,
      const std::string& mime_type,
      const base::FilePath& website_save_dir,
      const base::FilePath& download_save_dir);
  void ContinueGetSaveInfo(bool can_save_as_complete,
                           const base::FilePath& suggested_path);
  void OnPathPicked(SavePackagePathPickedParams params,
                    SavePackageDownloadCreatedCallback cb);

  // The number of in process SaveItems.
  int in_process_count() const {
    return static_cast<int>(in_progress_items_.size());
  }

  // The number of all SaveItems which have completed, including success items
  // and failed items.
  int completed_count() const {
    return static_cast<int>(saved_success_items_.size() +
                            saved_failed_items_.size());
  }

  // The current speed in files per second. This is used to update the
  // download::DownloadItem associated to this SavePackage. The files per second
  // is presented by the download::DownloadItem to the UI as bytes per second,
  // which is not correct but matches the way the total and received number of
  // files is presented as the total and received bytes.
  int64_t CurrentSpeed() const;

  // The current page, may be null if the primary page has been navigated away
  // or destroyed.
  base::WeakPtr<PageImpl> page_;

  // A queue for items we are about to start saving.
  base::circular_deque<std::unique_ptr<SaveItem>> waiting_item_queue_;

  // Map of all saving job in in-progress state.
  SaveItemIdMap in_progress_items_;

  // Map of all saving job which are failed.
  SaveItemIdMap saved_failed_items_;

  // Used to de-dupe urls that are being gathered into |waiting_item_queue_|
  // and also to find SaveItems to associate with a containing frame.
  // Note that |url_to_save_item_| does NOT own SaveItems - they
  // remain owned by waiting_item_queue_, in_progress_items_, etc.
  std::map<GURL, raw_ptr<SaveItem, CtnExperimental>> url_to_save_item_;

  // Map used to route responses from a given a subframe (i.e.
  // GetSerializedHtmlWithLocalLinksResponse) to the right SaveItem.
  // Note that |frame_tree_node_id_to_save_item_| does NOT own SaveItems - they
  // remain owned by waiting_item_queue_, in_progress_items_, etc.
  std::unordered_map<FrameTreeNodeId,
                     raw_ptr<SaveItem, CtnExperimental>,
                     FrameTreeNodeId::Hasher>
      frame_tree_node_id_to_save_item_;

  // Used to limit which local paths get exposed to which frames
  // (i.e. to prevent information disclosure to oop frames).
  // Note that |frame_tree_node_id_to_contained_save_items_| does NOT own
  // SaveItems - they remain owned by waiting_item_queue_, in_progress_items_,
  // etc.
  std::unordered_map<FrameTreeNodeId,
                     std::vector<raw_ptr<SaveItem, VectorExperimental>>,
                     FrameTreeNodeId::Hasher>
      frame_tree_node_id_to_contained_save_items_;

  // Number of frames that we still need to get a response from.
  int number_of_frames_pending_response_ = 0;

  // Map of all saving job which are successfully saved.
  SaveItemIdMap saved_success_items_;

  // Non-owning pointer for handling file writing on the download sequence.
  // This dangling raw_ptr occurred in:
  // content_browsertests: SavePackageBrowserTest.Reload
  // https://ci.chromium.org/ui/p/chromium/builders/try/linux-rel/1378285/test-results?q=ExactID%3Aninja%3A%2F%2Fcontent%2Ftest%3Acontent_browsertests%2FSavePackageBrowserTest.Reload+VHash%3Ad83661216aa0a42d
  raw_ptr<SaveFileManager, FlakyDanglingUntriaged> file_manager_ = nullptr;

  // DownloadManager owns the download::DownloadItem and handles history and UI.
  // These dangling raw_ptrs occurred in:
  // content_browsertests: SavePackageBrowserTest.Reload
  // chttps://ci.chromium.org/ui/p/chromium/builders/try/linux-rel/1430369/test-results?q=ExactID%3Aninja%3A%2F%2Fcontent%2Ftest%3Acontent_browsertests%2FSavePackageBrowserTest.Reload+VHash%3Ad83661216aa0a42d
  raw_ptr<DownloadManagerImpl, FlakyDanglingUntriaged> download_manager_ =
      nullptr;
  raw_ptr<download::DownloadItemImpl, FlakyDanglingUntriaged> download_ =
      nullptr;

  // The URL of the page the user wants to save.
  const GURL page_url_;
  base::FilePath saved_main_file_path_;
  base::FilePath saved_main_directory_path_;

  // Isolation info for network state partitioning.
  const net::IsolationInfo page_isolation_info_;
  bool page_is_outermost_main_frame_;

  // The title of the page the user wants to save.
  const std::u16string title_;

  // Used to calculate package download speed (in files per second).
  const base::TimeTicks start_tick_;

  // Indicates whether the actual saving job is finishing or not.
  bool finished_ = false;

  // Indicates whether user canceled the saving job.
  bool user_canceled_ = false;

  // Indicates whether user get disk error.
  bool disk_error_occurred_ = false;

  // Variables to record errors that happened so we can record them via
  // UMA statistics.
  bool wrote_to_completed_file_ = false;
  bool wrote_to_failed_file_ = false;

  // Type about saving page as only-html or complete-html.
  SavePageType save_type_ = SAVE_PAGE_TYPE_UNKNOWN;

#if BUILDFLAG(IS_MAC)
  // A list of tags specified by the user to be set on the file upon the
  // completion of it being written to disk.
  std::vector<std::string> file_tags_;
#endif

  // Number of all need to be saved resources.
  size_t all_save_items_count_ = 0;

  // This set is used to eliminate duplicated file names in saving directory.
  FileNameSet file_name_set_;

  // This map is used to track serial number for specified filename.
  FileNameCountMap file_name_count_map_;

  // Indicates current waiting state when SavePackage try to get something
  // from outside.
  WaitState wait_state_ = INITIALIZE;

  // Unique ID for this SavePackage.
  const SavePackageId unique_id_;

  // UKM IDs for reporting.
  ukm::SourceId ukm_source_id_;
  uint64_t ukm_download_id_;

  base::WeakPtrFactory<SavePackage> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_SAVE_PACKAGE_H_
