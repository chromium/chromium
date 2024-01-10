// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive/model/test_drive_file_uploader.h"

#import "base/task/sequenced_task_runner.h"

TestDriveFileUploader::TestDriveFileUploader(id<SystemIdentity> identity)
    : identity_(identity) {}

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
  if (!folder_search_result_) {
    return;
  }
  auto quit_closure =
      base::BindRepeating(&TestDriveFileUploader::RunQuitClosure,
                          callbacks_weak_ptr_factory_.GetWeakPtr());
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&TestDriveFileUploader::ReportFolderSearchResult,
                     callbacks_weak_ptr_factory_.GetWeakPtr(),
                     std::move(completion_callback), *folder_search_result_)
          .Then(quit_closure));
  folder_search_result_.reset();
}

void TestDriveFileUploader::CreateSaveToDriveFolder(
    NSString* folder_name,
    DriveFolderCompletionCallback completion_callback) {
  created_folder_name_ = folder_name;
  if (!folder_creation_result_) {
    return;
  }
  auto quit_closure =
      base::BindRepeating(&TestDriveFileUploader::RunQuitClosure,
                          callbacks_weak_ptr_factory_.GetWeakPtr());
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&TestDriveFileUploader::ReportFolderCreationResult,
                     callbacks_weak_ptr_factory_.GetWeakPtr(),
                     std::move(completion_callback), *folder_creation_result_)
          .Then(quit_closure));
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
  for (const DriveFileUploadProgress& progress :
       file_upload_progress_elements_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&TestDriveFileUploader::ReportFileUploadProgress,
                       callbacks_weak_ptr_factory_.GetWeakPtr(),
                       progress_callback, progress)
            .Then(quit_closure));
  }
  file_upload_progress_elements_.clear();
  // Then report result.
  if (!file_upload_result_) {
    return;
  }
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&TestDriveFileUploader::ReportFileUploadResult,
                     callbacks_weak_ptr_factory_.GetWeakPtr(),
                     std::move(completion_callback), *file_upload_result_)
          .Then(quit_closure));
  file_upload_result_.reset();
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
}

void TestDriveFileUploader::RunQuitClosure() {
  quit_closure_.Run();
}
