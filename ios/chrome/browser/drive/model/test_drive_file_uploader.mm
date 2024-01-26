// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive/model/test_drive_file_uploader.h"

#import "base/command_line.h"
#import "base/task/single_thread_task_runner.h"

TestDriveFileUploader::TestDriveFileUploader(id<SystemIdentity> identity)
    : identity_(identity) {
  // Parse command line arguments to override behavior.
  const std::string command_line_behavior =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          kTestDriveFileUploaderCommandLineSwitch);
  if (command_line_behavior ==
      kTestDriveFileUploaderCommandLineSwitchFailAndThenSucceed) {
    behavior_ = TestDriveFileUploaderBehavior::kFailAndThenSucceed;
  } else if (command_line_behavior ==
             kTestDriveFileUploaderCommandLineSwitchSucceed) {
    behavior_ = TestDriveFileUploaderBehavior::kSucceed;
  }
}

TestDriveFileUploader::~TestDriveFileUploader() = default;

#pragma mark - Public

void TestDriveFileUploader::SetFolderSearchResult(
    const DriveFolderResult& result) {
  folder_search_result_ = result;
}

void TestDriveFileUploader::SetFolderCreationResult(
    const DriveFolderResult& result) {
  folder_creation_result_ = result;
}

void TestDriveFileUploader::SetFileUploadProgressElements(
    std::vector<DriveFileUploadProgress> progress_elements) {
  file_upload_progress_elements_ = std::move(progress_elements);
}

void TestDriveFileUploader::SetFileUploadResult(
    const DriveFileUploadResult& result) {
  file_upload_result_ = result;
}

void TestDriveFileUploader::SetQuitClosure(
    base::RepeatingClosure quit_closure) {
  quit_closure_ = quit_closure;
}

NSString* TestDriveFileUploader::GetSearchedFolderName() const {
  return searched_folder_name_;
}

NSString* TestDriveFileUploader::GetCreatedFolderName() const {
  return created_folder_name_;
}

NSURL* TestDriveFileUploader::GetUploadedFileUrl() const {
  return uploaded_file_url_;
}

NSString* TestDriveFileUploader::GetUploadedFileName() const {
  return uploaded_file_name_;
}

NSString* TestDriveFileUploader::GetUploadedFileMimeType() const {
  return uploaded_file_mime_type_;
}

NSString* TestDriveFileUploader::GetUploadedFileFolderIdentifier() const {
  return uploaded_file_folder_identifier_;
}

#pragma mark - DriveFileUploader

id<SystemIdentity> TestDriveFileUploader::GetIdentity() const {
  return identity_;
}

bool TestDriveFileUploader::IsExecutingQuery() const {
  return callbacks_weak_ptr_factory_.HasWeakPtrs();
}

void TestDriveFileUploader::CancelCurrentQuery() {
  callbacks_weak_ptr_factory_.InvalidateWeakPtrs();
}

void TestDriveFileUploader::SearchSaveToDriveFolder(
    NSString* folder_name,
    DriveFolderCompletionCallback completion_callback) {
  searched_folder_name_ = folder_name;
  auto quit_closure =
      base::BindRepeating(&TestDriveFileUploader::RunQuitClosure,
                          callbacks_weak_ptr_factory_.GetWeakPtr());
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&TestDriveFileUploader::ReportFolderSearchResult,
                     callbacks_weak_ptr_factory_.GetWeakPtr(),
                     std::move(completion_callback), GetFolderSearchResult())
          .Then(quit_closure),
      base::Milliseconds(100));
  folder_search_result_.reset();
}

void TestDriveFileUploader::CreateSaveToDriveFolder(
    NSString* folder_name,
    DriveFolderCompletionCallback completion_callback) {
  created_folder_name_ = folder_name;
  auto quit_closure =
      base::BindRepeating(&TestDriveFileUploader::RunQuitClosure,
                          callbacks_weak_ptr_factory_.GetWeakPtr());
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&TestDriveFileUploader::ReportFolderCreationResult,
                     callbacks_weak_ptr_factory_.GetWeakPtr(),
                     std::move(completion_callback), GetFolderCreationResult())
          .Then(quit_closure),
      base::Milliseconds(100));
  folder_creation_result_.reset();
}

void TestDriveFileUploader::UploadFile(
    NSURL* file_url,
    NSString* file_name,
    NSString* file_mime_type,
    NSString* folder_identifier,
    DriveFileUploadProgressCallback progress_callback,
    DriveFileUploadCompletionCallback completion_callback) {
  auto quit_closure =
      base::BindRepeating(&TestDriveFileUploader::RunQuitClosure,
                          callbacks_weak_ptr_factory_.GetWeakPtr());
  uploaded_file_url_ = file_url;
  uploaded_file_name_ = file_name;
  uploaded_file_mime_type_ = file_mime_type;
  uploaded_file_folder_identifier_ = folder_identifier;
  // First report progress.
  base::TimeDelta delay;
  std::vector<DriveFileUploadProgress> progress_elements =
      GetFileUploadProgressElements();
  for (const DriveFileUploadProgress& progress : progress_elements) {
    delay += base::Milliseconds(100);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&TestDriveFileUploader::ReportFileUploadProgress,
                       callbacks_weak_ptr_factory_.GetWeakPtr(),
                       progress_callback, progress)
            .Then(quit_closure),
        delay);
  }
  // Then report result.
  delay += base::Milliseconds(100);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&TestDriveFileUploader::ReportFileUploadResult,
                     callbacks_weak_ptr_factory_.GetWeakPtr(),
                     std::move(completion_callback), GetFileUploadResult())
          .Then(quit_closure),
      delay);
}

#pragma mark - Private

void TestDriveFileUploader::ReportFolderSearchResult(
    DriveFolderCompletionCallback completion_callback,
    DriveFolderResult folder_search_result) {
  std::move(completion_callback).Run(folder_search_result);
}

void TestDriveFileUploader::ReportFolderCreationResult(
    DriveFolderCompletionCallback completion_callback,
    DriveFolderResult folder_creation_result) {
  std::move(completion_callback).Run(folder_creation_result);
}

void TestDriveFileUploader::ReportFileUploadProgress(
    DriveFileUploadProgressCallback progress_callback,
    DriveFileUploadProgress file_upload_progress) {
  std::move(progress_callback).Run(file_upload_progress);
}

void TestDriveFileUploader::ReportFileUploadResult(
    DriveFileUploadCompletionCallback completion_callback,
    DriveFileUploadResult file_upload_result) {
  std::move(completion_callback).Run(file_upload_result);
  last_reported_file_upload_result_ = file_upload_result;
}

void TestDriveFileUploader::RunQuitClosure() {
  quit_closure_.Run();
}

DriveFolderResult TestDriveFileUploader::GetFolderSearchResult() const {
  if (folder_search_result_) {
    return *folder_search_result_;
  }
  return DriveFolderResult{.folder_identifier = nil, .error = nil};
}

DriveFolderResult TestDriveFileUploader::GetFolderCreationResult() const {
  if (folder_creation_result_) {
    return *folder_creation_result_;
  }
  return DriveFolderResult{.folder_identifier = @"test_folder_identifier",
                           .error = nil};
}

std::vector<DriveFileUploadProgress>
TestDriveFileUploader::GetFileUploadProgressElements() const {
  if (!file_upload_progress_elements_.empty()) {
    return file_upload_progress_elements_;
  }
  std::vector<DriveFileUploadProgress> progress_elements;
  for (DriveFileUploadProgress progress{0, 100};
       progress.total_bytes_uploaded <= progress.total_bytes_expected_to_upload;
       progress.total_bytes_uploaded += 10) {
    progress_elements.push_back(progress);
  }
  return progress_elements;
}

DriveFileUploadResult TestDriveFileUploader::GetFileUploadResult() const {
  if (file_upload_result_) {
    return *file_upload_result_;
  }
  if (behavior_ == TestDriveFileUploaderBehavior::kFailAndThenSucceed &&
      !last_reported_file_upload_result_) {
    NSError* error = [NSError errorWithDomain:@"test_error_domain"
                                         code:400
                                     userInfo:@{}];
    return DriveFileUploadResult{.file_link = nil, .error = error};
  }
  return DriveFileUploadResult{.file_link = @"test_file_link", .error = nil};
}
