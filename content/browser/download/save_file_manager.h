// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Objects that handle file operations for saving files, on the file thread.
//
// The SaveFileManager owns a set of SaveFile objects, each of which connects
// with a SaveItem object which belongs to one SavePackage and runs on the file
// thread for saving data in order to avoid disk activity on either network IO
// thread or the UI thread. It coordinates the notifications from the network
// and UI.
//
// The SaveFileManager itself is a singleton object created and owned by the
// BrowserMainLoop.
//
// The data sent to SaveFileManager have 2 sources:
// - SimpleURLLoaders which are used to download sub-resources and
//   save-only-HTML pages
// - render processese, for HTML pages which are serialized from the DOM in
//   their original encoding. The data is received on the UI thread and
//   dispatched directly to the SaveFileManager on the file thread.
//
// A typical saving job operation involves multiple threads and sequences:
//
// Updating an in progress save file:
//      |----> data from    ---->|  |
//      |      render process    |  |
//      |      SimpleURLLoaders  |  |
// ui_thread                     |  |
//                   download_task_runner (writes to disk)
//                               |----> stats ---->|
//                                              ui_thread (feedback for user)
//
//
// Cancel operations perform the inverse order when triggered by a user action:
// ui_thread (user click)
//    |----> cancel command ---->|
//    |           |      download_task_runner (close file)
//    |           |---------------------> cancel command ---->|
//    |                                               io_thread (stops net IO
// ui_thread (user close contents)                               for saving)
//    |----> cancel command ---->|
//                            Render process(stop serializing DOM and sending
//                                           data)
//
//
// The SaveFileManager tracks saving requests, mapping from a save item id to
// the SavePackage for the contents where the saving job was initiated. In the
// event of a contents closure during saving, the SavePackage will notify the
// SaveFileManage to cancel all SaveFile jobs.

#ifndef CONTENT_BROWSER_DOWNLOAD_SAVE_FILE_MANAGER_H_
#define CONTENT_BROWSER_DOWNLOAD_SAVE_FILE_MANAGER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/browser/download/save_types.h"
#include "content/common/content_export.h"

class GURL;

namespace base {
class FilePath;
}

namespace content {
class BrowserContext;
class SaveFile;
class SavePackage;
class StoragePartition;
struct Referrer;

class CONTENT_EXPORT SaveFileManager
    : public base::RefCountedThreadSafe<SaveFileManager> {
 public:
   // Returns the singleton instance of the SaveFileManager.
  static SaveFileManager* Get();

  SaveFileManager();

  // Lifetime management.
  void Shutdown();

  // Saves the specified URL |url|. |save_package| must not be deleted before
  // the call to RemoveSaveFile. Should be called on the UI thread,
  void SaveURL(SaveItemId save_item_id,
               const GURL& url,
               const Referrer& referrer,
               int render_process_host_id,
               int render_view_routing_id,
               int render_frame_routing_id,
               SaveFileCreateInfo::SaveFileSource save_source,
               const base::FilePath& file_full_path,
               BrowserContext* context,
               StoragePartition* storage_partition,
               SavePackage* save_package);

  // Notifications sent from the IO thread and run on the file thread:
  void StartSave(std::unique_ptr<SaveFileCreateInfo> info);
  void UpdateSaveProgress(SaveItemId save_item_id, const std::string& data);
  void SaveFinished(SaveItemId save_item_id,
                    SavePackageId save_package_id,
                    bool is_success);

  // Notifications sent from the UI thread and run on the file thread.
  // Cancel a SaveFile instance which has specified save item id.
  void CancelSave(SaveItemId save_item_id);

  // Called on the UI thread to remove a save package from SaveFileManager's
  // tracking map.
  void RemoveSaveFile(SaveItemId save_item_id, SavePackage* package);

  // Helper function for deleting specified file.
  void DeleteDirectoryOrFile(const base::FilePath& full_path, bool is_dir);

  // Runs on file thread to save a file by copying from file system when
  // original url is using file scheme.
  void SaveLocalFile(const GURL& original_file_url,
                     SaveItemId save_item_id,
                     SavePackageId save_package_id);

  // Renames all the successfully saved files.
  void RenameAllFiles(const FinalNamesMap& final_names,
                      const base::FilePath& resource_dir,
                      int render_process_id,
                      int render_frame_routing_id,
                      SavePackageId save_package_id);

  // When the user cancels the saving, we need to remove all remaining saved
  // files of this page saving job from save_file_map_.
  void RemoveSavedFileFromFileMap(const std::vector<SaveItemId>& save_item_ids);

 private:
  friend class base::RefCountedThreadSafe<SaveFileManager>;

  class SimpleURLLoaderHelper;

  ~SaveFileManager();

  // A cleanup helper that runs on the file thread.
  void OnShutdown();

  // Called only on UI thread to get the SavePackage for a contents's browser
  // context.
  static SavePackage* GetSavePackageFromRenderIds(int render_process_id,
                                                  int render_frame_routing_id);

  // Look up the SavePackage according to save item id.
  SavePackage* LookupPackage(SaveItemId save_item_id);

  // Called only on the file thread.
  // Look up one in-progress saving item according to save item id.
  SaveFile* LookupSaveFile(SaveItemId save_item_id);

  // Help function for sending notification of canceling specific request.
  void SendCancelRequest(SaveItemId save_item_id);

  // Notifications sent from the file thread and run on the UI thread.

  // Lookup the SaveManager for this WebContents' saving browser context and
  // inform it the saving job has been started.
  void OnStartSave(const SaveFileCreateInfo& info);
  // Update the SavePackage with the current state of a started saving job.
  // If the SavePackage for this saving job is gone, cancel the request.
  void OnUpdateSaveProgress(SaveItemId save_item_id,
                            int64_t bytes_so_far,
                            bool write_success);
  // Update the SavePackage with the finish state, and remove the request
  // tracking entries.
  void OnSaveFinished(SaveItemId save_item_id,
                      int64_t bytes_so_far,
                      bool is_success);
  // Notifies SavePackage that the whole page saving job is finished.
  void OnFinishSavePageJob(int render_process_id,
                           int render_frame_routing_id,
                           SavePackageId save_package_id);

  // Notifications sent from the UI thread and run on the file thread.

  // Deletes a specified file on the file thread.
  void OnDeleteDirectoryOrFile(const base::FilePath& full_path, bool is_dir);

  // Notifications sent from the UI thread and run on the IO thread

  // Initiates a request for URL to be saved.
  void OnSaveURL(const GURL& url,
                 const Referrer& referrer,
                 SaveItemId save_item_id,
                 SavePackageId save_package_id,
                 int render_process_host_id,
                 int render_view_routing_id,
                 int render_frame_routing_id,
                 StoragePartition* storage_partition);

  // Called on the UI thread to remove the SimpleURLLoader in
  // |url_loader_helpers_| associated with |save_item_id|. This stops the load
  // if it is not complete.
  void ClearURLLoader(SaveItemId save_item_id);

  // A map from save_item_id into SaveFiles.
  std::unordered_map<SaveItemId, std::unique_ptr<SaveFile>, SaveItemId::Hasher>
      save_file_map_;

  // Tracks which SavePackage to send data to, called only on UI thread.
  // SavePackageMap maps save item ids to their SavePackage.
  std::unordered_map<SaveItemId, SavePackage*, SaveItemId::Hasher> packages_;

  // The helper object doing the actual download. Should be accessed on the UI
  // thread.
  std::unordered_map<SaveItemId,
                     std::unique_ptr<SimpleURLLoaderHelper>,
                     SaveItemId::Hasher>
      url_loader_helpers_;

  DISALLOW_COPY_AND_ASSIGN(SaveFileManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_SAVE_FILE_MANAGER_H_
