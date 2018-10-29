// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_request_info.h"

#include <utility>

#include "base/guid.h"
#include "base/strings/string_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/download/public/common/download_item.h"
#include "content/public/browser/background_fetch_response.h"
#include "content/public/browser/browser_thread.h"
#include "net/http/http_response_headers.h"

namespace content {

BackgroundFetchRequestInfo::BackgroundFetchRequestInfo(
    int request_index,
    const ServiceWorkerFetchRequest& fetch_request)
    : RefCountedDeleteOnSequence<BackgroundFetchRequestInfo>(
          base::SequencedTaskRunnerHandle::Get()),
      request_index_(request_index),
      fetch_request_(fetch_request) {}

BackgroundFetchRequestInfo::~BackgroundFetchRequestInfo() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void BackgroundFetchRequestInfo::InitializeDownloadGuid() {
  DCHECK(download_guid_.empty());

  download_guid_ = base::GenerateGUID();
}

void BackgroundFetchRequestInfo::SetDownloadGuid(
    const std::string& download_guid) {
  DCHECK(!download_guid.empty());
  DCHECK(download_guid_.empty());

  download_guid_ = download_guid;
}

void BackgroundFetchRequestInfo::SetResult(
    std::unique_ptr<BackgroundFetchResult> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(result);

  result_ = std::move(result);
  PopulateWithResponse(std::move(result_->response));
}

void BackgroundFetchRequestInfo::SetEmptyResultWithFailureReason(
    BackgroundFetchResult::FailureReason failure_reason) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  result_ = std::make_unique<BackgroundFetchResult>(
      nullptr /* response */, base::Time::Now(), failure_reason);
}

void BackgroundFetchRequestInfo::PopulateWithResponse(
    std::unique_ptr<BackgroundFetchResponse> response) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(response);

  url_chain_ = response->url_chain;

  // |headers| can be null when the request fails.
  if (!response->headers)
    return;

  // The response code, text and headers all are stored in the
  // net::HttpResponseHeaders object, shared by the |download_item|.
  response_code_ = response->headers->response_code();
  response_text_ = response->headers->GetStatusText();

  size_t iter = 0;
  std::string name, value;
  while (response->headers->EnumerateHeaderLines(&iter, &name, &value))
    response_headers_[base::ToLowerASCII(name)] = value;
}

int BackgroundFetchRequestInfo::GetResponseCode() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return response_code_;
}

const std::string& BackgroundFetchRequestInfo::GetResponseText() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return response_text_;
}

const std::map<std::string, std::string>&
BackgroundFetchRequestInfo::GetResponseHeaders() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return response_headers_;
}

const std::vector<GURL>& BackgroundFetchRequestInfo::GetURLChain() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return url_chain_;
}

const base::Optional<storage::BlobDataHandle>&
BackgroundFetchRequestInfo::GetBlobDataHandle() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(result_);
  return result_->blob_handle;
}

const base::FilePath& BackgroundFetchRequestInfo::GetFilePath() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(result_);
  return result_->file_path;
}

int64_t BackgroundFetchRequestInfo::GetFileSize() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(result_);
  return result_->file_size;
}

const base::Time& BackgroundFetchRequestInfo::GetResponseTime() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(result_);
  return result_->response_time;
}

bool BackgroundFetchRequestInfo::IsResultSuccess() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(result_);
  return result_->failure_reason == BackgroundFetchResult::FailureReason::NONE;
}

}  // namespace content
