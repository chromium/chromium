// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/download/download_task_impl.h"

#import <WebKit/WebKit.h>

#import <limits>

#import "base/apple/foundation_util.h"
#import "base/files/file.h"
#import "base/files/file_util.h"
#import "base/functional/bind.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/bind_post_task.h"
#import "base/task/sequenced_task_runner.h"
#import "ios/web/download/download_result.h"
#import "ios/web/public/download/download_task_observer.h"
#import "ios/web/public/web_state.h"
#import "net/base/filename_util.h"
#import "net/base/net_errors.h"

namespace web {
namespace download {
namespace internal {

// Helper struct that store the error code and the opened file object (in case
// of success).
struct CreateFileResult {
  int net_error_code = net::OK;
  base::FilePath file_path;

  explicit CreateFileResult(int error_code) : net_error_code(error_code) {
    DCHECK_NE(net_error_code, net::OK);
  }

  explicit CreateFileResult(base::FilePath path) : file_path(std::move(path)) {
    DCHECK(!file_path.empty());
  }

  CreateFileResult(CreateFileResult&& other) = default;
  CreateFileResult& operator=(CreateFileResult&& other) = default;

  ~CreateFileResult() = default;
};

namespace {

CreateFileResult CreateFileForDownload(base::FilePath path) {
  if (path.empty()) {
    if (!base::CreateTemporaryFile(&path)) {
      return CreateFileResult(
          net::MapSystemError(logging::GetLastSystemErrorCode()));
    }
    DCHECK(!path.empty());
  } else {
    base::File::Error error;
    if (!base::CreateDirectoryAndGetError(path.DirName(), &error)) {
      return CreateFileResult(net::FileErrorToNetError(error));
    }
  }

  // If `path` exists and is a directory, fail with an error as we don't
  // want the download task to delete an existing directory.
  if (base::DirectoryExists(path)) {
    return CreateFileResult(net::ERR_ACCESS_DENIED);
  }

  // Try to delete any existing file at `path` (deleting a non-existent
  // file is not an error for `base::DeleteFile(...)`). This is needed
  // as some sub-classes of DownloadTaskImpl fail if the destination
  // file already exists.
  if (!base::DeleteFile(path)) {
    return CreateFileResult(
        net::MapSystemError(logging::GetLastSystemErrorCode()));
  }

  return CreateFileResult(std::move(path));
}

NSData* ReadDataFromFile(base::FilePath path, int64_t bytes) {
  // base::ReadFile uses int for the count value, so it will fail if we
  // try to read more than INT_MAX bytes. Given that this is already 2GB
  // and we can't allocate that much memory, there is no point trying to
  // read the data in 2GB chunks, instead just fail.
  if (bytes < 0 || std::numeric_limits<int>::max() < bytes) {
    return nil;
  }

  NSMutableData* data = [NSMutableData dataWithLength:bytes];
  std::optional<uint64_t> bytes_read =
      base::ReadFile(path, base::apple::NSMutableDataToSpan(data));
  if (!bytes_read || *bytes_read != static_cast<uint64_t>(bytes)) {
    return nil;
  }

  return [data copy];
}

}  // anonymous namespace
}  // namespace internal
}  // namespace download

DownloadTaskImpl::DownloadTaskImpl(
    WebState* web_state,
    const GURL& original_url,
    NSString* http_method,
    const std::string& content_disposition,
    int64_t total_bytes,
    const std::string& mime_type,
    NSString* identifier,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : original_url_(original_url),
      http_method_(http_method),
      total_bytes_(total_bytes),
      content_disposition_(content_disposition),
      original_mime_type_(mime_type),
      mime_type_(mime_type),
      identifier_([identifier copy]),
      web_state_(web_state),
      task_runner_(task_runner) {
  DCHECK(web_state_);
  DCHECK(task_runner_);

  base::RepeatingClosure closure = base::BindPostTaskToCurrentDefault(
      base::BindRepeating(&DownloadTaskImpl::OnAppWillResignActive,
                          weak_factory_.GetWeakPtr()));

  base::WeakPtr<DownloadTaskImpl> weak_Task = weak_factory_.GetWeakPtr();
  observer_ = [NSNotificationCenter.defaultCenter
      addObserverForName:UIApplicationWillResignActiveNotification
                  object:nil
                   queue:nil
              usingBlock:^(NSNotification* _Nonnull) {
                closure.Run();
              }];
}

DownloadTaskImpl::~DownloadTaskImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  [NSNotificationCenter.defaultCenter removeObserver:observer_];
  for (auto& observer : observers_)
    observer.OnDownloadDestroyed(this);

  // Delete the downloaded file if it was a temporary file or if the download
  // failed (it is not an error to delete a non-existent file).
  if (owns_file_ || state_ != State::kComplete) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(base::IgnoreResult(&base::DeleteFile), path_));
  }
}

WebState* DownloadTaskImpl::GetWebState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return web_state_;
}

DownloadTask::State DownloadTaskImpl::GetState() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return state_;
}

void DownloadTaskImpl::Start(const base::FilePath& path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(state_, State::kInProgress);

  state_ = State::kInProgress;
  percent_complete_ = 0;
  received_bytes_ = 0;
  owns_file_ = path.empty();

  using download::internal::CreateFileForDownload;
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&CreateFileForDownload, path),
      base::BindOnce(&DownloadTaskImpl::OnDownloadFileCreated,
                     weak_factory_.GetWeakPtr()));
}

void DownloadTaskImpl::Cancel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  state_ = State::kCancelled;
  CancelInternal();
  OnDownloadUpdated();
}

NSString* DownloadTaskImpl::GetIdentifier() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return identifier_;
}

const GURL& DownloadTaskImpl::GetOriginalUrl() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return original_url_;
}

NSString* DownloadTaskImpl::GetHttpMethod() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return http_method_;
}

bool DownloadTaskImpl::IsDone() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (state_) {
    case State::kNotStarted:
    case State::kInProgress:
      return false;
    case State::kCancelled:
    case State::kComplete:
    case State::kFailed:
    case State::kFailedNotResumable:
      return true;
  }
}

int DownloadTaskImpl::GetErrorCode() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return download_result_.error_code();
}

int DownloadTaskImpl::GetHttpCode() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return http_code_;
}

int64_t DownloadTaskImpl::GetTotalBytes() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return total_bytes_;
}

int64_t DownloadTaskImpl::GetReceivedBytes() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return received_bytes_;
}

int DownloadTaskImpl::GetPercentComplete() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return percent_complete_;
}

std::string DownloadTaskImpl::GetContentDisposition() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return content_disposition_;
}

std::string DownloadTaskImpl::GetOriginalMimeType() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return original_mime_type_;
}

std::string DownloadTaskImpl::GetMimeType() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return mime_type_;
}

base::FilePath DownloadTaskImpl::GenerateFileName() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return net::GenerateFileName(original_url_, content_disposition_,
                               /*referrer_charset=*/std::string(),
                               /*suggested_name=*/GetSuggestedName(),
                               /*mime_type=*/std::string(),
                               /*default_name=*/"document");
}

bool DownloadTaskImpl::HasPerformedBackgroundDownload() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return has_performed_background_download_;
}

void DownloadTaskImpl::AddObserver(DownloadTaskObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}

void DownloadTaskImpl::RemoveObserver(DownloadTaskObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

void DownloadTaskImpl::GetResponseData(
    ResponseDataReadCallback callback) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsDone());
  using download::internal::ReadDataFromFile;
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&ReadDataFromFile, path_, received_bytes_),
      std::move(callback));
}

const base::FilePath& DownloadTaskImpl::GetResponsePath() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsDone());
  static const base::FilePath kEmptyPath;
  return owns_file_ ? kEmptyPath : path_;
}

std::string DownloadTaskImpl::GetSuggestedName() const {
  return std::string();
}

void DownloadTaskImpl::OnAppWillResignActive() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (GetState() == DownloadTask::State::kInProgress) {
    has_performed_background_download_ = YES;
  }
}

void DownloadTaskImpl::OnDownloadFileCreated(
    download::internal::CreateFileResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (result.net_error_code == net::OK) {
    path_ = std::move(result.file_path);
    StartInternal(path_);
    return;
  }

  OnDownloadFinished(DownloadResult(result.net_error_code));
}

void DownloadTaskImpl::OnDownloadFinished(DownloadResult download_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  download_result_ = download_result;
  if (download_result_.error_code()) {
    state_ = download_result_.can_retry() ? State::kFailed
                                          : State::kFailedNotResumable;
  } else {
    state_ = State::kComplete;
  }

  OnDownloadUpdated();
}

void DownloadTaskImpl::OnDownloadUpdated() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : observers_)
    observer.OnDownloadUpdated(this);
}

}  // namespace web
