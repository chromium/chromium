// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DRIVE_MODEL_TEST_DRIVE_FILE_UPLOADER_H_
#define IOS_CHROME_BROWSER_DRIVE_MODEL_TEST_DRIVE_FILE_UPLOADER_H_

#import "base/functional/callback_helpers.h"
#import "base/memory/weak_ptr.h"
#import "ios/chrome/browser/drive/model/drive_file_uploader.h"
#import "ios/chrome/browser/drive/model/test_constants.h"

@protocol SystemIdentity;

// Test implementation for `DriveFileUploader`.
class TestDriveFileUploader final : public DriveFileUploader {
 public:
  explicit TestDriveFileUploader(id<SystemIdentity> identity);
  ~TestDriveFileUploader() final;

  // Returns values reported by callbacks of `DriveFileUploader` methods.
  // Unless overridden e.g. using `SetFolderSearchResult()`, a default value
  // will be returned.
  DriveFolderResult GetFolderSearchResult() const;
  DriveFolderResult GetFolderCreationResult() const;
  std::vector<DriveFileUploadProgress> GetFileUploadProgressElements() const;
  DriveFileUploadResult GetFileUploadResult() const;
  DriveStorageQuotaResult GetStorageQuotaResult() const;

  // Sets folder search result to be reported by `SearchSaveToDriveFolder()`.
  void SetFolderSearchResult(const DriveFolderResult& result);
  // Sets folder creation result to be reported by `CreateSaveToDriveFolder()`.
  void SetFolderCreationResult(const DriveFolderResult& result);
  // Sets file upload progress elements to be reported by `UploadFile()`.
  void SetFileUploadProgressElements(
      std::vector<DriveFileUploadProgress> progress_elements);
  // Sets file upload progress result to be reported by `UploadFile()`.
  void SetFileUploadResult(const DriveFileUploadResult& result);
  // Sets storage quota result to be reported by `FetchStorageQuota()`.
  void SetStorageQuotaResult(const DriveStorageQuotaResult& result);

  // Set quit closures.
  void SetSearchFolderQuitClosure(base::RepeatingClosure quit_closure);
  void SetCreateFolderQuitClosure(base::RepeatingClosure quit_closure);
  void SetUploadFileProgressQuitClosure(base::RepeatingClosure quit_closure);
  void SetUploadFileCompletionQuitClosure(base::RepeatingClosure quit_closure);
  void SetFetchStorageQuotaQuitClosure(base::RepeatingClosure quit_closure);

  // Returns `folder_name` passed to `SearchSaveToDriveFolder()`.
  NSString* GetSearchedFolderName() const;
  // Returns `folder_name` passed to `CreateSaveToDriveFolder()`.
  NSString* GetCreatedFolderName() const;
  // Returns `file_url` passed to `UploadFile()`.
  NSURL* GetUploadedFileUrl() const;
  // Returns `file_name` passed to `UploadFile()`.
  NSString* GetUploadedFileName() const;
  // Returns `file_mime_type` passed to `UploadFile()`.
  NSString* GetUploadedFileMimeType() const;
  // Returns `folder_identifier` passed to `UploadFile()`.
  NSString* GetUploadedFileFolderIdentifier() const;

  // `DriveFileUploader` overrides.
  id<SystemIdentity> GetIdentity() const final;
  bool IsExecutingQuery() const final;
  void CancelCurrentQuery() final;
  void SearchSaveToDriveFolder(
      NSString* folder_name,
      DriveFolderCompletionCallback completion_callback) final;
  void CreateSaveToDriveFolder(
      NSString* folder_name,
      DriveFolderCompletionCallback completion_callback) final;
  void UploadFile(NSURL* file_url,
                  NSString* file_name,
                  NSString* file_mime_type,
                  NSString* folder_identifier,
                  DriveFileUploadProgressCallback progress_callback,
                  DriveFileUploadCompletionCallback completion_callback) final;
  void FetchStorageQuota(
      DriveStorageQuotaCompletionCallback completion_callback) final;

 private:
  // Calls `completion_callback` with `folder_search_result` and calls
  // `quit_closure_`.
  void ReportFolderSearchResult(
      DriveFolderCompletionCallback completion_callback,
      DriveFolderResult folder_search_result);
  // Calls `completion_callback` with `folder_creation_result` and calls
  // `quit_closure_`.
  void ReportFolderCreationResult(
      DriveFolderCompletionCallback completion_callback,
      DriveFolderResult folder_creation_result);
  // Calls `progress_callback` with `file_upload_progress` and calls
  // `quit_closure_`.
  void ReportFileUploadProgress(
      DriveFileUploadProgressCallback progress_callback,
      DriveFileUploadProgress file_upload_progress);
  // Calls `completion_callback` with `file_upload_result` and calls
  // `quit_closure_`.
  void ReportFileUploadResult(
      DriveFileUploadCompletionCallback completion_callback,
      DriveFileUploadResult file_upload_result);
  // Calls `completion_callback` with `storage_quota_result` and calls
  // `quit_closure_`.
  void ReportStorageQuotaResult(
      DriveStorageQuotaCompletionCallback completion_callback,
      DriveStorageQuotaResult storage_quota_result);

  // Run quit closures.
  void RunSearchFolderQuitClosure();
  void RunCreateFolderQuitClosure();
  void RunUploadFileProgressQuitClosure();
  void RunUploadFileCompletionQuitClosure();
  void RunFetchStorageQuotaQuitClosure();

  id<SystemIdentity> identity_;

  // Values passed to `DriveFileUploader` query methods.
  NSString* searched_folder_name_;
  NSString* created_folder_name_;
  NSURL* uploaded_file_url_;
  NSString* uploaded_file_name_;
  NSString* uploaded_file_mime_type_;
  NSString* uploaded_file_folder_identifier_;

  // Results/progress to be reported by callbacks of `DriveFileUploader`
  // methods. If one of these values is not set, a default value will be
  // reported instead.
  std::optional<DriveFolderResult> folder_search_result_;
  std::optional<DriveFolderResult> folder_creation_result_;
  std::vector<DriveFileUploadProgress> file_upload_progress_elements_;
  std::optional<DriveFileUploadResult> file_upload_result_;
  std::optional<DriveStorageQuotaResult> storage_quota_result_;

  // Quit closures.
  base::RepeatingClosure search_folder_quit_closure_ = base::DoNothing();
  base::RepeatingClosure create_folder_quit_closure_ = base::DoNothing();
  base::RepeatingClosure upload_file_progress_quit_closure_ = base::DoNothing();
  base::RepeatingClosure upload_file_completion_quit_closure_ =
      base::DoNothing();
  base::RepeatingClosure fetch_storage_quota_quit_closure_ = base::DoNothing();

  // Last value reported by `ReportFileUploadResult()`, if any.
  std::optional<DriveFileUploadResult> last_reported_file_upload_result_;

  // Behavior e.g. whether to return an error or not.
  TestDriveFileUploaderBehavior behavior_ =
      TestDriveFileUploaderBehavior::kSucceed;

  // Weak pointer factory, for callbacks. Can be used to cancel any pending
  // tasks by invalidating all weak pointers.
  base::WeakPtrFactory<TestDriveFileUploader> callbacks_weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_DRIVE_MODEL_TEST_DRIVE_FILE_UPLOADER_H_
