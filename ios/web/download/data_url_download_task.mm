// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/download/data_url_download_task.h"

#import "base/check.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/task/sequenced_task_runner.h"
#import "ios/web/download/download_result.h"
#import "net/base/data_url.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {
namespace download {
namespace internal {

// Helper struct that store the error code and data (in case of success) of
// parsing the data: URL.
struct ParseDataUrlResult {
  int net_error_code = net::OK;

  int64_t data_size = 0;
  std::string mime_type;

  explicit ParseDataUrlResult(int net_error_code)
      : net_error_code(net_error_code) {
    DCHECK_NE(net_error_code, net::OK);
  }

  ParseDataUrlResult(int64_t data_size, std::string mime_type)
      : data_size(data_size), mime_type(std::move(mime_type)) {}

  ParseDataUrlResult(ParseDataUrlResult&& other) = default;
  ParseDataUrlResult& operator=(ParseDataUrlResult&& other) = default;

  ~ParseDataUrlResult() = default;
};

namespace {

// Helper function that extract data from `url` and write it to `path` if
// not empty. Returns a net error code to indicate if the operation was a
// success or not.
ParseDataUrlResult ParseDataUrlAndSaveToFile(GURL url, base::FilePath path) {
  std::string data;
  std::string charset;
  std::string mime_type;
  if (!net::DataURL::Parse(url, &mime_type, &charset, &data)) {
    return ParseDataUrlResult(net::ERR_INVALID_URL);
  }

  if (!base::WriteFile(path, data)) {
    return ParseDataUrlResult(
        net::MapSystemError(logging::GetLastSystemErrorCode()));
  }

  return ParseDataUrlResult(data.size(), std::move(mime_type));
}

}  // anonymous namespace
}  // namespace internal
}  // namespace download

DataUrlDownloadTask::DataUrlDownloadTask(
    WebState* web_state,
    const GURL& original_url,
    NSString* http_method,
    const std::string& content_disposition,
    int64_t total_bytes,
    const std::string& mime_type,
    NSString* identifier,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : DownloadTaskImpl(web_state,
                       original_url,
                       http_method,
                       content_disposition,
                       total_bytes,
                       mime_type,
                       identifier,
                       task_runner) {
  DCHECK(original_url_.SchemeIs(url::kDataScheme));
}

DataUrlDownloadTask::~DataUrlDownloadTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CancelInternal();
}

void DataUrlDownloadTask::StartInternal(const base::FilePath& path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!path.empty());

  using download::internal::ParseDataUrlAndSaveToFile;
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&ParseDataUrlAndSaveToFile, original_url_, path),
      base::BindOnce(&DataUrlDownloadTask::OnDataUrlParsed,
                     weak_factory_.GetWeakPtr()));
}

void DataUrlDownloadTask::CancelInternal() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  weak_factory_.InvalidateWeakPtrs();
}

void DataUrlDownloadTask::OnDataUrlParsed(
    download::internal::ParseDataUrlResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (result.net_error_code == net::OK) {
    percent_complete_ = 100;
    total_bytes_ = result.data_size;
    received_bytes_ = total_bytes_;

    mime_type_ = std::move(result.mime_type);
  }

  OnDownloadFinished(DownloadResult(result.net_error_code));
}

}  // namespace web
