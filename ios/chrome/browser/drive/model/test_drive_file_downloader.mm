// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive/model/test_drive_file_downloader.h"

#import "base/task/single_thread_task_runner.h"

namespace {

// Time constant used to post delayed tasks and simulate Drive file downloads.
constexpr base::TimeDelta kTestDriveFileDownloaderTimeConstant =
    base::Milliseconds(100);

}  // namespace

TestDriveFileDownloader::TestDriveFileDownloader(id<SystemIdentity> identity)
    : identity_(identity) {}

TestDriveFileDownloader::~TestDriveFileDownloader() = default;

void TestDriveFileDownloader::SetDownloadFileCompletionQuitClosure(
    base::RepeatingClosure quit_closure) {
  download_file_quit_closure_ = std::move(quit_closure);
}

#pragma mark - DriveFileDownloader

id<SystemIdentity> TestDriveFileDownloader::GetIdentity() const {
  return identity_;
}

bool TestDriveFileDownloader::IsExecutingDownload(
    DriveFileDownloadID download_id) const {
  return callbacks_weak_ptr_factory_.HasWeakPtrs();
}

void TestDriveFileDownloader::CancelDownload(DriveFileDownloadID download_id) {
  callbacks_weak_ptr_factory_.InvalidateWeakPtrs();
}

DriveFileDownloadID TestDriveFileDownloader::DownloadFile(
    const DriveItem& item_to_download,
    NSURL* file_url,
    DriveFileDownloadProgressCallback progress_callback,
    DriveFileDownloadCompletionCallback completion_callback) {
  // TODO(crbug.com/344812969): Report progress.
  // Then report result.
  const auto completion_quit_closure = base::BindRepeating(
      &TestDriveFileDownloader::RunDownloadFileCompletionQuitClosure,
      callbacks_weak_ptr_factory_.GetWeakPtr());
  DriveFileDownloadID download_id = [[NSUUID UUID] UUIDString];
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&TestDriveFileDownloader::ReportDownloadFileResult,
                     callbacks_weak_ptr_factory_.GetWeakPtr(),
                     std::move(completion_callback), download_id, true, nil)
          .Then(completion_quit_closure),
      kTestDriveFileDownloaderTimeConstant);
  return download_id;
}

#pragma mark - Private

void TestDriveFileDownloader::ReportDownloadFileResult(
    DriveFileDownloadCompletionCallback completion_callback,
    DriveFileDownloadID download_id,
    BOOL successful,
    NSError* error) {
  std::move(completion_callback).Run(download_id, successful, error);
}

void TestDriveFileDownloader::RunDownloadFileCompletionQuitClosure() {
  download_file_quit_closure_.Run();
}
