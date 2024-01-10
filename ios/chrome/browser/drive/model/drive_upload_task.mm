// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive/model/drive_upload_task.h"

#import "base/apple/foundation_util.h"
#import "base/files/file_path.h"
#import "base/functional/bind.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/drive/model/drive_file_uploader.h"

DriveUploadTask::DriveUploadTask(std::unique_ptr<DriveFileUploader> uploader)
    : uploader_{std::move(uploader)} {}

DriveUploadTask::~DriveUploadTask() = default;

#pragma mark - Public

id<SystemIdentity> DriveUploadTask::GetIdentity() const {
  return uploader_->GetIdentity();
}

void DriveUploadTask::SetFileToUpload(const base::FilePath& path,
                                      const base::FilePath& suggested_name,
                                      const std::string& mime_type) {
  file_path_ = path;
  suggested_file_name_ = suggested_name;
  file_mime_type_ = mime_type;
}

void DriveUploadTask::SetDestinationFolderName(const std::string& folder_name) {
  folder_name_ = folder_name;
}

#pragma mark - UploadTask

UploadTask::State DriveUploadTask::GetState() const {
  return state_;
}

void DriveUploadTask::Start() {
  if (state_ == State::kInProgress || state_ == State::kCancelled ||
      state_ == State::kComplete) {
    // If upload is in progress, cancelled or completed, do nothing.
    return;
  }
  upload_progress_.reset();
  upload_result_.reset();
  SetState(State::kInProgress);
  SearchFolderThenCreateFolderOrDirectlyUploadFile();
}

void DriveUploadTask::Cancel() {
  if (uploader_->IsExecutingQuery()) {
    uploader_->CancelCurrentQuery();
  }
  upload_progress_.reset();
  upload_result_.reset();
  SetState(State::kCancelled);
}

float DriveUploadTask::GetProgress() const {
  if (!upload_progress_ ||
      upload_progress_->total_bytes_expected_to_upload == 0) {
    return 0;
  }
  return static_cast<float>(upload_progress_->total_bytes_uploaded) /
         upload_progress_->total_bytes_expected_to_upload;
}

NSURL* DriveUploadTask::GetResponseLink() const {
  if (!upload_result_) {
    return nil;
  }
  return [NSURL URLWithString:upload_result_->file_link];
}

NSError* DriveUploadTask::GetError() const {
  if (!upload_result_) {
    return nil;
  }
  return upload_result_->error;
}

#pragma mark - Private

void DriveUploadTask::SearchFolderThenCreateFolderOrDirectlyUploadFile() {
  // Search a destination Drive folder using
  // `SearchSaveToDriveFolder(folder_name, ...)`;
  uploader_->SearchSaveToDriveFolder(
      base::SysUTF8ToNSString(folder_name_),
      base::BindOnce(&DriveUploadTask::CreateFolderOrDirectlyUploadFile,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DriveUploadTask::CreateFolderOrDirectlyUploadFile(
    DriveFolderResult folder_search_result) {
  // If folder search failed, update state and result with the error object.
  if (folder_search_result.error) {
    upload_result_ =
        DriveFileUploadResult({.error = folder_search_result.error});
    SetState(State::kFailed);
    return;
  }
  // If the first step returned an existing folder, upload file directly.
  if (folder_search_result.folder_identifier) {
    UploadFile(folder_search_result);
    return;
  }
  // Otherwise, create a destination Drive folder using
  // `CreateSaveToDriveFolder(folder_name, ...)`;
  uploader_->CreateSaveToDriveFolder(
      base::SysUTF8ToNSString(folder_name_),
      base::BindOnce(&DriveUploadTask::UploadFile,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DriveUploadTask::UploadFile(DriveFolderResult folder_result) {
  // If `folder_result` contains an error, then a destination folder did not
  // exist and could not be created, update state and result with the error
  // object.
  if (folder_result.error) {
    upload_result_ = DriveFileUploadResult({.error = folder_result.error});
    SetState(State::kFailed);
    return;
  }
  // If a destination folder was created/found then upload the file at
  // `file_url` using `UploadFile(file_url, ...)`.
  uploader_->UploadFile(
      base::apple::FilePathToNSURL(file_path_),
      base::apple::FilePathToNSString(suggested_file_name_),
      base::SysUTF8ToNSString(file_mime_type_), folder_result.folder_identifier,
      base::BindRepeating(&DriveUploadTask::OnDriveFileUploadProgress,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&DriveUploadTask::OnDriveFileUploadResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DriveUploadTask::OnDriveFileUploadProgress(
    DriveFileUploadProgress progress) {
  upload_progress_ = progress;
  OnUploadUpdated();
}

void DriveUploadTask::OnDriveFileUploadResult(DriveFileUploadResult result) {
  upload_result_ = result;
  SetState(result.error == nil ? State::kComplete : State::kFailed);
}

void DriveUploadTask::SetState(State state) {
  state_ = state;
  OnUploadUpdated();
}
