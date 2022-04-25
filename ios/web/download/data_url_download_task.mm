// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/download/data_url_download_task.h"

#import "base/check.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/logging.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/post_task.h"
#import "base/task/task_traits.h"
#import "base/task/thread_pool.h"
#import "ios/web/download/download_result.h"
#import "net/base/data_url.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {
namespace internal {

// Helper struct that store the error code and data (in case of success) of
// parsing the data: URL.
struct ParseDataUrlResult {
  int net_error_code = net::OK;

  std::string data;
  std::string mime_type;
  base::FilePath path;

  explicit ParseDataUrlResult(int net_error_code)
      : net_error_code(net_error_code) {
    DCHECK_NE(net_error_code, net::OK);
  }

  ParseDataUrlResult(std::string data,
                     std::string mime_type,
                     base::FilePath path)
      : data(std::move(data)), mime_type(std::move(mime_type)), path(path) {}

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

  if (!path.empty() && !base::WriteFile(path, data)) {
    return ParseDataUrlResult(
        net::MapSystemError(logging::GetLastSystemErrorCode()));
  }

  return ParseDataUrlResult(std::move(data), std::move(mime_type),
                            std::move(path));
}

}  // anonymous namespace
}  // namespace internal

DataUrlDownloadTask::DataUrlDownloadTask(WebState* web_state,
                                         const GURL& original_url,
                                         NSString* http_method,
                                         const std::string& content_disposition,
                                         int64_t total_bytes,
                                         const std::string& mime_type,
                                         NSString* identifier)
    : DownloadTaskImpl(web_state,
                       original_url,
                       http_method,
                       content_disposition,
                       total_bytes,
                       mime_type,
                       identifier) {
  DCHECK(original_url_.SchemeIs(url::kDataScheme));
}

DataUrlDownloadTask::~DataUrlDownloadTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

NSData* DataUrlDownloadTask::GetResponseData() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsDone());
  return data_;
}

const base::FilePath& DataUrlDownloadTask::GetResponsePath() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsDone());
  return path_;
}

void DataUrlDownloadTask::Start(const base::FilePath& path,
                                Destination destination_hint) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DownloadTaskImpl::Start(path, destination_hint);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(
          &internal::ParseDataUrlAndSaveToFile, original_url_,
          destination_hint != Destination::kToMemory ? path : base::FilePath()),
      base::BindOnce(&DataUrlDownloadTask::OnDataUrlParsed,
                     weak_factory_.GetWeakPtr()));
}

void DataUrlDownloadTask::OnDataUrlParsed(internal::ParseDataUrlResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (result.net_error_code == net::OK) {
    percent_complete_ = 100;
    total_bytes_ = result.data.size();
    received_bytes_ = total_bytes_;

    mime_type_ = std::move(result.mime_type);
    data_ = [NSData dataWithBytes:result.data.data() length:result.data.size()];
    path_ = std::move(result.path);
  }

  OnDownloadFinished(DownloadResult(result.net_error_code));
}

}  // namespace web
