// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FILE_UPLOAD_PANEL_COORDINATOR_FILE_UPLOAD_PANEL_PICKER_RESULT_LOADER_H_
#define IOS_CHROME_BROWSER_FILE_UPLOAD_PANEL_COORDINATOR_FILE_UPLOAD_PANEL_PICKER_RESULT_LOADER_H_

#import <Foundation/Foundation.h>
#import <PhotosUI/PhotosUI.h>

#import "base/functional/callback.h"
#import "base/memory/weak_ptr.h"
#import "base/sequence_checker.h"
#import "base/task/sequenced_task_runner.h"
#import "ios/web/public/web_state_id.h"

@class FileUploadPanelMediaItem;

// A class that loads file representations for `PHPickerResult` objects.
class FileUploadPanelPickerResultLoader {
 public:
  // The callback will be called with the loaded items, or `nil` on failure.
  using LoadResultCallback =
      base::OnceCallback<void(NSArray<FileUploadPanelMediaItem*>*)>;

  FileUploadPanelPickerResultLoader(NSArray<PHPickerResult*>* results,
                                    web::WebStateID web_state_id);
  FileUploadPanelPickerResultLoader(const FileUploadPanelPickerResultLoader&) =
      delete;
  FileUploadPanelPickerResultLoader& operator=(
      const FileUploadPanelPickerResultLoader&) = delete;
  ~FileUploadPanelPickerResultLoader();

  // Loads file representations for the results from the photo library provided
  // in the constructor. Upon completion, `callback` is called with the loaded
  // items, or with `nil` if an error occurs.
  void Load(LoadResultCallback callback);

 private:
  // Helper for `Load`. Loads results recursively starting from `index`.
  void LoadNextResult(NSUInteger index, LoadResultCallback callback);

  // Helper to handle the result of loading a single item and proceed to the
  // next.
  void OnResultLoaded(NSUInteger index,
                      LoadResultCallback callback,
                      FileUploadPanelMediaItem* loaded_item);

  // Loads the file representation for a single `result` from the photo library.
  void LoadPickerResult(
      PHPickerResult* result,
      base::OnceCallback<void(FileUploadPanelMediaItem*)> callback);

  // Handles the result of loading a file from the photo library.
  void HandleMovedFileRepresentation(
      UTType* file_type,
      base::OnceCallback<void(FileUploadPanelMediaItem*)> callback,
      std::optional<base::FilePath> file_path);

  SEQUENCE_CHECKER(sequence_checker_);
  NSArray<PHPickerResult*>* results_;
  NSMutableArray<FileUploadPanelMediaItem*>* loaded_items_;
  web::WebStateID web_state_id_;
  base::WeakPtrFactory<FileUploadPanelPickerResultLoader> weak_ptr_factory_{
      this};
};

#endif  // IOS_CHROME_BROWSER_FILE_UPLOAD_PANEL_COORDINATOR_FILE_UPLOAD_PANEL_PICKER_RESULT_LOADER_H_
