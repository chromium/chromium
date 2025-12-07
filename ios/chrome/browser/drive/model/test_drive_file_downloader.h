// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DRIVE_MODEL_TEST_DRIVE_FILE_DOWNLOADER_H_
#define IOS_CHROME_BROWSER_DRIVE_MODEL_TEST_DRIVE_FILE_DOWNLOADER_H_

#import "base/functional/callback_helpers.h"
#import "ios/chrome/browser/drive/model/drive_file_downloader.h"

// Test implementation for `DriveFileDownloader`.
class TestDriveFileDownloader final : public DriveFileDownloader {
 public:
  explicit TestDriveFileDownloader(id<SystemIdentity> identity);
  ~TestDriveFileDownloader() final;

  // Set quit closures.
  void SetDownloadFileCompletionQuitClosure(
      base::RepeatingClosure quit_closure);

  // DriveFileDownloader implementation
  id<SystemIdentity> GetIdentity() const final;
  bool IsExecutingDownload(DriveFileDownloadID download_id) const final;
  void CancelDownload(DriveFileDownloadID download_id) final;
  DriveFileDownloadID DownloadFile(
      const DriveItem& item_to_download,
      NSURL* file_url,
      DriveFileDownloadProgressCallback progress_callback,
      DriveFileDownloadCompletionCallback completion_callback) final;

 private:
  // Calls `completion_callback` with `download_id`, `successful` and `error`.
  void ReportDownloadFileResult(
      DriveFileDownloadCompletionCallback completion_callback,
      DriveFileDownloadID download_id,
      BOOL successful,
      NSError* error);

  // Run quit closures.
  void RunDownloadFileCompletionQuitClosure();

  // Identity returned by `GetIdentity()`.
  id<SystemIdentity> identity_;

  // Quit closures.
  base::RepeatingClosure download_file_quit_closure_ = base::DoNothing();

  // Weak pointer factory, for callbacks. Can be used to cancel any pending
  // tasks by invalidating all weak pointers.
  base::WeakPtrFactory<TestDriveFileDownloader> callbacks_weak_ptr_factory_{
      this};
};

#endif  // IOS_CHROME_BROWSER_DRIVE_MODEL_TEST_DRIVE_FILE_DOWNLOADER_H_
