// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_request_info.h"

#include <utility>

#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/uuid.h"
#include "components/download/public/common/download_item.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/public/browser/background_fetch_response.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/service_worker_context.h"
#include "net/http/http_response_headers.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_storage_context.h"

namespace content {

// Holds state on the IO thread. Needed because blobs are bound to the IO
// thread.
class BackgroundFetchRequestInfo::BlobDataOnIO {
 public:
  BlobDataOnIO() = default;
  ~BlobDataOnIO() = default;

  BlobDataOnIO(const BlobDataOnIO&) = delete;
  BlobDataOnIO& operator=(const BlobDataOnIO&) = delete;

  void CreateBlobDataHandle(
      scoped_refptr<ChromeBlobStorageContext> blob_storage_context,
      std::unique_ptr<storage::BlobDataHandle> blob_handle,
      const base::FilePath& file_path,
      uint64_t file_size,
      uint64_t expected_response_size) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    DCHECK(!blob_data_handle_);

    // In Incognito mode, |blob_handle| will be populated.
    if (blob_handle) {
      blob_data_handle_ = std::move(blob_handle);
      return;
    }

    // In a normal profile, |file_path| and |file_size| will
    // be populated.
    auto blob_builder = std::make_unique<storage::BlobDataBuilder>(
        base::Uuid::GenerateRandomV4().AsLowercaseString());
    blob_builder->AppendFile(file_path, /* offset= */ 0, file_size,
                             /* expected_modification_time= */ base::Time());

    blob_data_handle_ = GetBlobStorageContext(blob_storage_context.get())
                            ->AddFinishedBlob(std::move(blob_builder));
    DCHECK_EQ(expected_response_size,
              blob_data_handle_ ? blob_data_handle_->size() : 0);
  }

  std::unique_ptr<storage::BlobDataHandle> TakeBlobDataHandle() {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    return std::move(blob_data_handle_);
  }

 private:
  std::unique_ptr<storage::BlobDataHandle> blob_data_handle_;
};

BackgroundFetchRequestInfo::BackgroundFetchRequestInfo(
    int request_index,
    blink::mojom::FetchAPIRequestPtr fetch_request,
    uint64_t request_body_size)
    : RefCountedDeleteOnSequence<BackgroundFetchRequestInfo>(
          base::SequencedTaskRunner::GetCurrentDefault()),
      request_index_(request_index),
      fetch_request_(std::move(fetch_request)),
      request_body_size_(request_body_size) {}

BackgroundFetchRequestInfo::~BackgroundFetchRequestInfo() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void BackgroundFetchRequestInfo::InitializeDownloadGuid() {
  DCHECK(download_guid_.empty());

  download_guid_ = base::Uuid::GenerateRandomV4().AsLowercaseString();
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
  // The BackgroundFetchResponse was extracted when the download started.
  // This is sent over again when the download was complete in case the
  // browser was restarted.
  if (response_headers_.empty())
    PopulateWithResponse(std::move(result_->response));
  else
    result_->response.reset();

  // Get the response size.
  if (result_->blob_handle)
    response_size_ = result_->blob_handle->size();
  else
    response_size_ = result_->file_size;
}

void BackgroundFetchRequestInfo::SetEmptyResultWithFailureReason(
    BackgroundFetchResult::FailureReason failure_reason) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  result_ = std::make_unique<BackgroundFetchResult>(
      /* response= */ nullptr, base::Time::Now(), failure_reason);
}

void BackgroundFetchRequestInfo::PopulateWithResponse(
    std::unique_ptr<BackgroundFetchResponse> response) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
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

void BackgroundFetchRequestInfo::CreateResponseBlobDataHandle(
    scoped_refptr<ChromeBlobStorageContext> blob_storage_context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(result_);
  DCHECK(!io_blob_data_);

  if (!result_->blob_handle && result_->file_path.empty())
    return;

  io_blob_data_.reset(new BlobDataOnIO());
  std::unique_ptr<storage::BlobDataHandle> handle =
      result_->blob_handle ? std::make_unique<storage::BlobDataHandle>(
                                 result_->blob_handle.value())
                           : nullptr;
  result_->blob_handle.reset();

  // base::Unretained is safe because |io_blob_data_| is deleted on the IO
  // thread in a task that must run after this task.
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&BlobDataOnIO::CreateBlobDataHandle,
                     base::Unretained(io_blob_data_.get()),
                     std::move(blob_storage_context), std::move(handle),
                     result_->file_path, result_->file_size, response_size_));
}

std::unique_ptr<storage::BlobDataHandle>
BackgroundFetchRequestInfo::TakeResponseBlobDataHandleOnIO() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!io_blob_data_)
    return nullptr;
  return io_blob_data_->TakeBlobDataHandle();
}

uint64_t BackgroundFetchRequestInfo::GetResponseSize() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(result_);
  return response_size_;
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
