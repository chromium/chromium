// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DRIVE_MODEL_DRIVE_UPLOAD_TASK_H_
#define IOS_CHROME_BROWSER_DRIVE_MODEL_DRIVE_UPLOAD_TASK_H_

#import "base/files/file_path.h"
#import "ios/chrome/browser/drive/model/drive_file_uploader.h"
#import "ios/chrome/browser/drive/model/upload_task.h"

class DriveFileUploader;

// Upload task which uses a `DriveFileUploader` to retrieve a destination folder
// in a user's Drive, creates it if necessary, and uploads a file to it.
class DriveUploadTask final : public UploadTask {
 public:
  explicit DriveUploadTask(std::unique_ptr<DriveFileUploader> uploader);
  ~DriveUploadTask() final;

  // Sets source `path`, `suggested_name` and `mime_type` of file to upload.
  void SetFileToUpload(const base::FilePath& path,
                       const base::FilePath& suggested_name,
                       const std::string& mime_type);
  // Sets name of folder in which to add uploaded files.
  void SetDestinationFolderName(const std::string& folder_name);

  // UploadTask overrides.
  State GetState() const final;
  void Start() final;
  void Cancel() final;
  id<SystemIdentity> GetIdentity() const final;
  float GetProgress() const final;
  NSURL* GetResponseLink() const final;
  NSError* GetError() const final;

 private:
  // Performs the first step of this upload task i.e. search a destination Drive
  // folder using `uploader_->SearchSaveToDriveFolder(folder_name, ...)`.
  // The result will be reported to `CreateFolderOrDirectlyUploadFile()`;
  void SearchFolderThenCreateFolderOrDirectlyUploadFile();

  // Performs the second step of this upload task i.e.
  // if the first step returned an existing folder, directly upload the file to
  // this existing folder using `UploadFile()`. Otherwise, create a destination
  // folder using `uploader_->CreateSaveToDriveFolder(folder_name, ...)` and
  // report the result to `UploadFile()`;
  void CreateFolderOrDirectlyUploadFile(DriveFolderResult folder_search_result);

  // Performs the third step of this upload task i.e. uploads the
  // file at `file_url` to the folder contained in `folder_result` using
  // `uploader_->UploadFile(file_url, ...)`.
  void UploadFile(DriveFolderResult folder_result);

  // Called when the uploader is reporting progress of upload.
  void OnDriveFileUploadProgress(DriveFileUploadProgress progress);
  // Called when the uploader is reporting result of upload.
  void OnDriveFileUploadResult(DriveFileUploadResult result);

  // Sets `state_` and calls `UploadTaskUpdated()`.
  void SetState(State state);

  // Current state of upload.
  State state_ = State::kNotStarted;

  // File path of file to upload.
  base::FilePath file_path_;
  // Suggested file name for uploaded file.
  base::FilePath suggested_file_name_;
  // MIME type of uploaded file.
  std::string file_mime_type_;
  // Name of folder in which to add uploaded files.
  std::string folder_name_;

  // File uploader.
  std::unique_ptr<DriveFileUploader> uploader_;
  // Latest progress reported to `OnDriveFileUploadProgress()`, if any.
  std::optional<DriveFileUploadProgress> upload_progress_;
  // Result of this upload task if it is done. If one the steps failed, then
  // this will contain an error. Otherwise it will contain a link to the
  // successfully uploaded file.
  std::optional<DriveFileUploadResult> upload_result_;

  base::WeakPtrFactory<DriveUploadTask> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_DRIVE_MODEL_DRIVE_UPLOAD_TASK_H_
