// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive/model/drive_upload_task.h"

#import "base/apple/foundation_util.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/functional/bind.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/thread_pool.h"
#import "ios/chrome/browser/download/model/download_mimetype_util.h"
#import "ios/chrome/browser/drive/model/drive_file_uploader.h"
#import "ios/chrome/browser/drive/model/drive_metrics.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "net/base/apple/url_conversions.h"
#import "net/base/url_util.h"
#import "url/gurl.h"

namespace {

constexpr int64_t kBytesPerMegabyte = 1024 * 1024;
constexpr char kHistogramSuffixTaskNotStarted[] = ".NotStarted";
constexpr char kHistogramSuffixTaskInProgress[] = ".InProgress";
constexpr char kHistogramSuffixTaskCancelled[] = ".Cancelled";
constexpr char kHistogramSuffixTaskComplete[] = ".Complete";
constexpr char kHistogramSuffixTaskFailed[] = ".Failed";

// Returns the appropriate histogram suffix for upload task state `state`.
const char* HistogramSuffixForUploadTaskState(UploadTask::State state) {
  switch (state) {
    case UploadTask::State::kNotStarted:
      return kHistogramSuffixTaskNotStarted;
    case UploadTask::State::kInProgress:
      return kHistogramSuffixTaskInProgress;
    case UploadTask::State::kCancelled:
      return kHistogramSuffixTaskCancelled;
    case UploadTask::State::kComplete:
      return kHistogramSuffixTaskComplete;
    case UploadTask::State::kFailed:
      return kHistogramSuffixTaskFailed;
  }
}

// Converts `state` to `UploadTaskStateHistogram`.
UploadTaskStateHistogram UploadTaskStateToHistogram(UploadTask::State state) {
  switch (state) {
    case UploadTask::State::kNotStarted:
      return UploadTaskStateHistogram::kNotStarted;
    case UploadTask::State::kInProgress:
      return UploadTaskStateHistogram::kInProgress;
    case UploadTask::State::kCancelled:
      return UploadTaskStateHistogram::kCancelled;
    case UploadTask::State::kComplete:
      return UploadTaskStateHistogram::kComplete;
    case UploadTask::State::kFailed:
      return UploadTaskStateHistogram::kFailed;
  }
}

// Records whether `result.error == nil` for boolean histogram `histogram`.
// Returns `result`.
DriveFolderResult RecordDriveFolderResultSuccessful(
    const char* histogram,
    const DriveFolderResult& result) {
  base::UmaHistogramBoolean(histogram, result.error == nil);
  return result;
}

// If `result.error`, records `result.error.code` for histogram `histogram`.
// Returns `result`.
DriveFolderResult RecordDriveFolderResultErrorCode(
    const char* histogram,
    const DriveFolderResult& result) {
  if (result.error) {
    base::UmaHistogramSparse(histogram, result.error.code);
  }
  return result;
}

// Name of query parameter for the user ID to open a Drive response link.
constexpr const char kHashedUserIdQueryParameterName[] = "huid";

}  // namespace

DriveUploadTask::DriveUploadTask(std::unique_ptr<DriveFileUploader> uploader)
    : uploader_{std::move(uploader)} {}

DriveUploadTask::~DriveUploadTask() {
  if (GetState() == State::kInProgress) {
    Cancel();
  }
  // Record histograms for the task at destruction.
  base::UmaHistogramEnumeration(kDriveUploadTaskFinalState,
                                UploadTaskStateToHistogram(GetState()));
  if (!file_path_) {
    return;
  }
  // Only record file details histograms if a file was given to upload.
  const char* histogram_suffix = HistogramSuffixForUploadTaskState(GetState());
  base::UmaHistogramCounts100(
      std::string(kDriveUploadTaskNumberOfAttempts) + histogram_suffix,
      number_of_attempts_);
  const DownloadMimeTypeResult mime_type_result =
      GetDownloadMimeTypeResultFromMimeType(file_mime_type_);
  base::UmaHistogramEnumeration(
      std::string(kDriveUploadTaskMimeType) + histogram_suffix,
      mime_type_result);
  if (file_total_bytes_ != -1) {
    base::UmaHistogramMemoryMB(
        std::string(kDriveUploadTaskFileSize) + histogram_suffix,
        file_total_bytes_ / kBytesPerMegabyte);
  }
}

#pragma mark - Public

id<SystemIdentity> DriveUploadTask::GetIdentity() const {
  return uploader_->GetIdentity();
}

void DriveUploadTask::SetFileToUpload(const base::FilePath& path,
                                      const base::FilePath& suggested_name,
                                      const std::string& mime_type,
                                      const int64_t total_bytes) {
  file_path_ = path;
  suggested_file_name_ = suggested_name;
  file_mime_type_ = mime_type;
  file_total_bytes_ = total_bytes;
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

std::optional<GURL> DriveUploadTask::GetResponseLink(
    bool add_user_identifier) const {
  if (!upload_result_ || !upload_result_->file_link) {
    return std::nullopt;
  }
  GURL result(base::SysNSStringToUTF8(upload_result_->file_link));
  if (add_user_identifier) {
    NSString* user_identifier = GetIdentity().hashedGaiaID;
    result = net::AppendOrReplaceQueryParameter(
        result, kHashedUserIdQueryParameterName,
        base::SysNSStringToUTF8(user_identifier));
  }
  return result;
}

NSError* DriveUploadTask::GetError() const {
  if (!upload_result_) {
    return nil;
  }
  return upload_result_->error;
}

#pragma mark - Private

void DriveUploadTask::SearchFolderThenCreateFolderOrDirectlyUploadFile() {
  number_of_attempts_++;
  // Search a destination Drive folder using
  // `SearchSaveToDriveFolder(folder_name, ...)`;
  uploader_->SearchSaveToDriveFolder(
      base::SysUTF8ToNSString(folder_name_),
      base::BindOnce(&DriveUploadTask::CreateFolderOrDirectlyUploadFile,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DriveUploadTask::CreateFolderOrDirectlyUploadFile(
    const DriveFolderResult& folder_search_result) {
  // Record folder search success histogram.
  base::UmaHistogramBoolean(kDriveSearchFolderResultSuccessful,
                            !folder_search_result.error);
  // If folder search failed, update state and result with the error object.
  if (folder_search_result.error) {
    base::UmaHistogramSparse(kDriveSearchFolderResultErrorCode,
                             folder_search_result.error.code);
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
  auto record_result_successful_callback = base::BindOnce(
      RecordDriveFolderResultSuccessful, kDriveCreateFolderResultSuccessful);
  auto record_result_error_code_callback = base::BindOnce(
      RecordDriveFolderResultErrorCode, kDriveCreateFolderResultErrorCode);
  auto upload_file_callback = base::BindOnce(&DriveUploadTask::UploadFile,
                                             weak_ptr_factory_.GetWeakPtr());
  uploader_->CreateSaveToDriveFolder(
      base::SysUTF8ToNSString(folder_name_),
      std::move(record_result_successful_callback)
          .Then(std::move(record_result_error_code_callback))
          .Then(std::move(upload_file_callback)));
}

void DriveUploadTask::UploadFile(const DriveFolderResult& folder_result) {
  CHECK(file_path_);
  const auto file_path = *file_path_;
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
      base::apple::FilePathToNSURL(file_path),
      base::apple::FilePathToNSString(suggested_file_name_),
      base::SysUTF8ToNSString(file_mime_type_), folder_result.folder_identifier,
      base::BindRepeating(&DriveUploadTask::OnDriveFileUploadProgress,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&DriveUploadTask::OnDriveFileUploadResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DriveUploadTask::OnDriveFileUploadProgress(
    const DriveFileUploadProgress& progress) {
  upload_progress_ = progress;
  OnUploadUpdated();
}

void DriveUploadTask::OnDriveFileUploadResult(
    const DriveFileUploadResult& result) {
  // Record file upload result histograms.
  base::UmaHistogramBoolean(kDriveFileUploadResultSuccessful,
                            result.error == nil);
  if (result.error) {
    base::UmaHistogramSparse(kDriveFileUploadResultErrorCode,
                             result.error.code);
  }
  // Store result and update state.
  upload_result_ = result;
  SetState(result.error == nil ? State::kComplete : State::kFailed);
}

void DriveUploadTask::SetState(State state) {
  state_ = state;
  OnUploadUpdated();
}
