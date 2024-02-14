// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DRIVE_MODEL_DRIVE_FILE_UPLOADER_H_
#define IOS_CHROME_BROWSER_DRIVE_MODEL_DRIVE_FILE_UPLOADER_H_

#import <Foundation/Foundation.h>

#import "base/functional/callback.h"

@protocol SystemIdentity;

// Result returned asynchronously by the completion callback of a query to
// search/create a folder.
struct DriveFolderResult {
  // Identifier of the found/created folder, if search/creation succeeded. If
  // this is a search result but there is no folder matching the search
  // criteria, both `error` and `folder_identifier` will be nil.
  NSString* folder_identifier = nil;
  // Error object, if folder search/creation failed. Empty search results are
  // not treated as errors.
  NSError* error = nil;
};

using DriveFolderCompletionCallback =
    base::OnceCallback<void(const DriveFolderResult&)>;

// Progress returned asynchronously by the progress callback of a query to
// upload a file.
struct DriveFileUploadProgress {
  // Number of bytes uploaded so far.
  uint64_t total_bytes_uploaded;
  // Number of bytes expected to be uploaded.
  uint64_t total_bytes_expected_to_upload;
};

using DriveFileUploadProgressCallback =
    base::RepeatingCallback<void(const DriveFileUploadProgress&)>;

// Result returned asynchronously by the completion callback of a query to
// upload a file.
struct DriveFileUploadResult {
  // A link for opening the file, if file upload succeeded.
  NSString* file_link = nil;
  // Error object, if file upload failed.
  NSError* error = nil;
};

using DriveFileUploadCompletionCallback =
    base::OnceCallback<void(const DriveFileUploadResult&)>;

// Result reported by the completion block of a query to fetch the user's Drive
// storage quota.
struct DriveStorageQuotaResult {
  // The usage limit, if applicable. Set to -1 if the user has unlimited
  // storage.
  int64_t limit;
  // The usage by all files in Drive.
  int64_t usage_in_drive;
  // The usage by trashed files in Drive.
  int64_t usage_in_drive_trash;
  // The total usage across all services.
  int64_t usage;
  // Error object, if fetching storage quota failed.
  NSError* error = nil;
};

using DriveStorageQuotaCompletionCallback =
    base::OnceCallback<void(const DriveStorageQuotaResult&)>;

// This interface is used to perform queries in a user's Drive account.
class DriveFileUploader {
 public:
  DriveFileUploader();
  virtual ~DriveFileUploader();

  // Returns the identity used to perform queries.
  virtual id<SystemIdentity> GetIdentity() const = 0;
  // Returns whether a query is currently being executed by this uploader.
  virtual bool IsExecutingQuery() const = 0;
  // Cancels the query currently being executed by this uploader.
  virtual void CancelCurrentQuery() = 0;

  // Returns the "Save to Drive" folder. The "Save to Drive" folder
  // meets multiple criteria:
  // - 1 - Its name must be `folder_name`.
  // - 2 - It must have been created from this app.
  // - 3 - It must not be in the trash.
  // If multiple folders match these criteria, the most recent one according to
  // creation date is returned.
  // The result, including possible error details, is returned asynchronously
  // through `completion_callback`.
  virtual void SearchSaveToDriveFolder(
      NSString* folder_name,
      DriveFolderCompletionCallback completion_callback) = 0;

  // Creates the destination Drive folder. The name of the created folder is
  // `folder_name` and a custom property is added in the folder's metadata to
  // find the folder later.
  // The result, including possible error details, is returned asynchronously
  // through `completion_callback`.
  virtual void CreateSaveToDriveFolder(
      NSString* folder_name,
      DriveFolderCompletionCallback completion_callback) = 0;

  // Uploads the file stored locally at `file_url`, with MIME type
  // `file_mime_type`, in Drive folder with identifier `folder_identifier`.
  // Progress of upload is reported through `progress_callback` and final
  // result, including possible error details, is returned asynchronously
  // through `completion_callback`.
  virtual void UploadFile(
      NSURL* file_url,
      NSString* file_name,
      NSString* file_mime_type,
      NSString* folder_identifier,
      DriveFileUploadProgressCallback progress_callback,
      DriveFileUploadCompletionCallback completion_callback) = 0;

  // Fetches the Drive storage quota, to check if there is enough storage for
  // uploads.
  virtual void FetchStorageQuota(
      DriveStorageQuotaCompletionCallback completion_callback) = 0;
};

#endif  // IOS_CHROME_BROWSER_DRIVE_MODEL_DRIVE_FILE_UPLOADER_H_
