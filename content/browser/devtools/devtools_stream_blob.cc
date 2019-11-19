// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_stream_blob.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/strings/string_piece.h"
#include "base/task/post_task.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/io_buffer.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_reader.h"
#include "storage/browser/blob/blob_storage_constants.h"
#include "storage/browser/blob/blob_storage_context.h"

namespace content {

using storage::BlobReader;

DevToolsStreamBlob::ReadRequest::ReadRequest(off_t position,
                                             size_t max_size,
                                             ReadCallback callback)
    : position(position), max_size(max_size), callback(std::move(callback)) {}

DevToolsStreamBlob::ReadRequest::~ReadRequest() = default;

DevToolsStreamBlob::DevToolsStreamBlob()
    : DevToolsIOContext::Stream(
          base::CreateSingleThreadTaskRunner({BrowserThread::IO})),
      last_read_pos_(0),
      failed_(false),
      is_binary_(false) {}

DevToolsStreamBlob::~DevToolsStreamBlob() {
  if (blob_reader_)
    blob_reader_->Kill();
}

namespace {
void UnregisterIfOpenFailed(base::WeakPtr<DevToolsIOContext> context,
                            const std::string& handle,
                            bool success) {
  if (!success && context)
    context->Close(handle);
}
}  // namespace

// static
scoped_refptr<DevToolsIOContext::Stream> DevToolsStreamBlob::Create(
    DevToolsIOContext* io_context,
    ChromeBlobStorageContext* blob_context,
    StoragePartition* partition,
    const std::string& handle,
    const std::string& uuid) {
  scoped_refptr<DevToolsStreamBlob> result = new DevToolsStreamBlob();
  result->Register(io_context, handle);
  result->Open(
      blob_context, partition, uuid,
      base::BindOnce(&UnregisterIfOpenFailed, io_context->AsWeakPtr(), handle));
  return std::move(result);
}

void DevToolsStreamBlob::ReadRequest::Fail() {
  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(std::move(callback), nullptr, false,
                                Stream::StatusFailure));
}

void DevToolsStreamBlob::Open(scoped_refptr<ChromeBlobStorageContext> context,
                              StoragePartition* partition,
                              const std::string& handle,
                              OpenCallback callback) {
  base::PostTask(FROM_HERE, {BrowserThread::IO},
                 base::BindOnce(&DevToolsStreamBlob::OpenOnIO, this, context,
                                handle, std::move(callback)));
}

void DevToolsStreamBlob::Read(off_t position,
                              size_t max_size,
                              ReadCallback callback) {
  std::unique_ptr<ReadRequest> request(
      new ReadRequest(position, max_size, std::move(callback)));
  base::PostTask(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&DevToolsStreamBlob::ReadOnIO, this, std::move(request)));
}

void DevToolsStreamBlob::OpenOnIO(
    scoped_refptr<ChromeBlobStorageContext> blob_context,
    const std::string& uuid,
    OpenCallback callback) {
  DCHECK(!blob_handle_);

  storage::BlobStorageContext* bsc = blob_context->context();
  blob_handle_ = bsc->GetBlobDataFromUUID(uuid);
  if (!blob_handle_) {
    LOG(ERROR) << "No blob with uuid: " << uuid;
    FailOnIO(std::move(callback));
    return;
  }
  is_binary_ = !DevToolsIOContext::IsTextMimeType(blob_handle_->content_type());
  open_callback_ = std::move(callback);
  blob_handle_->RunOnConstructionComplete(
      base::BindOnce(&DevToolsStreamBlob::OnBlobConstructionComplete, this));
}

void DevToolsStreamBlob::OnBlobConstructionComplete(
    storage::BlobStatus status) {
  DCHECK(!BlobStatusIsPending(status));
  if (BlobStatusIsError(status)) {
    LOG(ERROR) << "Blob building failed: " << static_cast<int>(status);
    FailOnIO(std::move(open_callback_));
    return;
  }
  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(std::move(open_callback_), true));
  if (!pending_reads_.empty())
    StartReadRequest();
}

void DevToolsStreamBlob::ReadOnIO(std::unique_ptr<ReadRequest> request) {
  if (failed_) {
    request->Fail();
    return;
  }
  pending_reads_.push(std::move(request));
  if (pending_reads_.size() > 1 || open_callback_)
    return;
  StartReadRequest();
}

void DevToolsStreamBlob::FailOnIO() {
  failed_ = true;
  while (!pending_reads_.empty()) {
    pending_reads_.front()->Fail();
    pending_reads_.pop();
  }
}

void DevToolsStreamBlob::FailOnIO(OpenCallback callback) {
  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(std::move(callback), false));
  FailOnIO();
}

void DevToolsStreamBlob::StartReadRequest() {
  DCHECK_GE(pending_reads_.size(), 1UL);
  DCHECK(blob_handle_);
  DCHECK(!failed_);

  ReadRequest& request = *pending_reads_.front();
  if (request.position < 0)
    request.position = last_read_pos_;
  if (request.position != last_read_pos_)
    blob_reader_.reset();
  if (!blob_reader_)
    CreateReader();
  else
    BeginRead();
}

void DevToolsStreamBlob::BeginRead() {
  DCHECK_GE(pending_reads_.size(), 1UL);
  ReadRequest& request = *pending_reads_.front();
  if (!io_buf_ || static_cast<size_t>(io_buf_->size()) < request.max_size)
    io_buf_ = base::MakeRefCounted<net::IOBufferWithSize>(request.max_size);
  int bytes_read;
  BlobReader::Status status = blob_reader_->Read(
      io_buf_.get(), request.max_size, &bytes_read,
      base::BindOnce(&DevToolsStreamBlob::OnReadComplete, this));
  if (status == BlobReader::Status::IO_PENDING)
    return;
  // This is for uniformity with the asynchronous case.
  if (status == BlobReader::Status::NET_ERROR) {
    bytes_read = blob_reader_->net_error();
    DCHECK_LT(0, bytes_read);
  }
  base::PostTask(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&DevToolsStreamBlob::OnReadComplete, this, bytes_read));
}

void DevToolsStreamBlob::OnReadComplete(int bytes_read) {
  std::unique_ptr<ReadRequest> request = std::move(pending_reads_.front());
  pending_reads_.pop();

  Status status;
  std::unique_ptr<std::string> data(new std::string());
  bool base64_encoded = false;

  if (bytes_read < 0) {
    status = StatusFailure;
    LOG(ERROR) << "Error reading blob: " << net::ErrorToString(bytes_read);
  } else if (!bytes_read) {
    status = StatusEOF;
  } else {
    last_read_pos_ += bytes_read;
    status = blob_reader_->remaining_bytes() ? StatusSuccess : StatusEOF;
    if (is_binary_) {
      base64_encoded = true;
      Base64Encode(base::StringPiece(io_buf_->data(), bytes_read), data.get());
    } else {
      // TODO(caseq): truncate at UTF8 boundary.
      *data = std::string(io_buf_->data(), bytes_read);
    }
  }
  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(std::move(request->callback), std::move(data),
                                base64_encoded, status));
  if (!pending_reads_.empty())
    StartReadRequest();
}

void DevToolsStreamBlob::CreateReader() {
  DCHECK(!blob_reader_);
  blob_reader_ = blob_handle_->CreateReader();
  BlobReader::Status status = blob_reader_->CalculateSize(
      base::BindOnce(&DevToolsStreamBlob::OnCalculateSizeComplete, this));
  if (status != BlobReader::Status::IO_PENDING) {
    OnCalculateSizeComplete(status == BlobReader::Status::NET_ERROR
                                ? blob_reader_->net_error()
                                : net::OK);
  }
}

void DevToolsStreamBlob::OnCalculateSizeComplete(int net_error) {
  if (net_error != net::OK) {
    FailOnIO();
    return;
  }
  off_t seek_to = pending_reads_.front()->position;
  if (seek_to != 0UL) {
    if (seek_to >= static_cast<off_t>(blob_reader_->total_size())) {
      OnReadComplete(0);
      return;
    }
    BlobReader::Status status = blob_reader_->SetReadRange(
        seek_to, blob_reader_->total_size() - seek_to);
    if (status != BlobReader::Status::DONE) {
      FailOnIO();
      return;
    }
  }
  BeginRead();
}

}  // namespace content
