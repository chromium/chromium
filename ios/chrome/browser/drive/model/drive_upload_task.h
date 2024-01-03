// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DRIVE_MODEL_DRIVE_UPLOAD_TASK_H_
#define IOS_CHROME_BROWSER_DRIVE_MODEL_DRIVE_UPLOAD_TASK_H_

#import "ios/chrome/browser/drive/model/upload_task.h"

#import "base/files/file_path.h"

// Upload task which uses a `DriveFileUploader` to retrieve a destination folder
// in a user's Drive, creates it if necessary, and uploads a file to it.
class DriveUploadTask final : public UploadTask {
 public:
  DriveUploadTask();
  ~DriveUploadTask() final;

  // Sets source `path`, `suggested_name` and `mime_type` of file to upload.
  void SetFileToUpload(const base::FilePath& path,
                       const base::FilePath& suggested_name,
                       const std::string& mime_type);

  // UploadTask overrides.
  State GetState() const final;
  void Start() final;
  void Cancel() final;
  id<SystemIdentity> GetIdentity() const final;
  float GetProgress() const final;
  NSURL* GetResponseLink() const final;
  NSError* GetError() const final;

 private:
  // Sets `state_` and calls `UploadTaskUpdated()`.
  void SetState(State state);

  // File path of file to upload.
  base::FilePath file_path_;
  // Suggested file name for uploaded file.
  base::FilePath suggested_file_name_;
  // MIME type of uploaded file.
  std::string file_mime_type_;
  // Current state of upload.
  State state_ = State::kNotStarted;
};

#endif  // IOS_CHROME_BROWSER_DRIVE_MODEL_DRIVE_UPLOAD_TASK_H_
