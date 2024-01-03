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
  // TODO(crbug.com/1495354): Return the identity used to upload the file.
  return nil;
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
  SetState(State::kInProgress);
  // TODO(crbug.com/1495354): Start upload.
}

void DriveUploadTask::Cancel() {
  // TODO(crbug.com/1495354): Cancel upload.
  SetState(State::kCancelled);
}

float DriveUploadTask::GetProgress() const {
  // TODO(crbug.com/1495354): Return actual progress of upload.
  return 0;
}

NSURL* DriveUploadTask::GetResponseLink() const {
  // TODO(crbug.com/1495354): If upload has succeeded, return link to open
  // uploaded file.
  return nil;
}

NSError* DriveUploadTask::GetError() const {
  // TODO(crbug.com/1495354): If upload has failed, return error object.
  return nil;
}

#pragma mark - Private

void DriveUploadTask::SetState(State state) {
  state_ = state;
  OnUploadUpdated();
}
