// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/fileapi/file_writer_impl.h"

#include "base/callback_helpers.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_storage_context.h"

namespace storage {

FileWriterImpl::FileWriterImpl(
    FileSystemURL url,
    std::unique_ptr<FileSystemOperationRunner> operation_runner,
    base::WeakPtr<BlobStorageContext> blob_context)
    : operation_runner_(std::move(operation_runner)),
      blob_context_(std::move(blob_context)),
      url_(std::move(url)) {
  DCHECK(url_.is_valid());
}

FileWriterImpl::~FileWriterImpl() = default;

void FileWriterImpl::Write(uint64_t position,
                           blink::mojom::BlobPtr blob,
                           WriteCallback callback) {
  blob_context_->GetBlobDataFromBlobPtr(
      std::move(blob),
      base::BindOnce(&FileWriterImpl::DoWrite, base::Unretained(this),
                     std::move(callback), position));
}

void FileWriterImpl::WriteStream(uint64_t position,
                                 mojo::ScopedDataPipeConsumerHandle stream,
                                 WriteStreamCallback callback) {
  // FileSystemOperationRunner assumes that positions passed to Write are always
  // valid, and will NOTREACHED() if that is not the case, so first check the
  // size of the file to make sure the position passed in from the renderer is
  // in fact valid.
  // Of course the file could still change between checking its size and the
  // write operation being started, but this is at least a lot better than the
  // old implementation where the renderer only checks against how big it thinks
  // the file currently is.
  operation_runner_->GetMetadata(
      url_, FileSystemOperation::GET_METADATA_FIELD_SIZE,
      base::BindRepeating(&FileWriterImpl::DoWriteStreamWithFileInfo,
                          base::Unretained(this),
                          base::AdaptCallbackForRepeating(std::move(callback)),
                          position, base::Passed(std::move(stream))));
}

void FileWriterImpl::Truncate(uint64_t length, TruncateCallback callback) {
  operation_runner_->Truncate(
      url_, length,
      base::BindRepeating(
          &FileWriterImpl::DidTruncate, base::Unretained(this),
          base::AdaptCallbackForRepeating(std::move(callback))));
}

void FileWriterImpl::DoWrite(WriteCallback callback,
                             uint64_t position,
                             std::unique_ptr<BlobDataHandle> blob) {
  if (!blob) {
    std::move(callback).Run(base::File::FILE_ERROR_FAILED, 0);
    return;
  }

  // FileSystemOperationRunner assumes that positions passed to Write are always
  // valid, and will NOTREACHED() if that is not the case, so first check the
  // size of the file to make sure the position passed in from the renderer is
  // in fact valid.
  // Of course the file could still change between checking its size and the
  // write operation being started, but this is at least a lot better than the
  // old implementation where the renderer only checks against how big it thinks
  // the file currently is.
  operation_runner_->GetMetadata(
      url_, FileSystemOperation::GET_METADATA_FIELD_SIZE,
      base::BindRepeating(&FileWriterImpl::DoWriteWithFileInfo,
                          base::Unretained(this),
                          base::AdaptCallbackForRepeating(std::move(callback)),
                          position, base::Passed(std::move(blob))));
}

void FileWriterImpl::DoWriteWithFileInfo(WriteCallback callback,
                                         uint64_t position,
                                         std::unique_ptr<BlobDataHandle> blob,
                                         base::File::Error result,
                                         const base::File::Info& file_info) {
  if (file_info.size < 0 || position > static_cast<uint64_t>(file_info.size)) {
    std::move(callback).Run(base::File::FILE_ERROR_FAILED, 0);
    return;
  }
  operation_runner_->Write(
      url_, std::move(blob), position,
      base::BindRepeating(&FileWriterImpl::DidWrite, base::Unretained(this),
                          base::AdaptCallbackForRepeating(std::move(callback)),
                          base::Owned(new WriteState())));
}

void FileWriterImpl::DoWriteStreamWithFileInfo(
    WriteStreamCallback callback,
    uint64_t position,
    mojo::ScopedDataPipeConsumerHandle data_pipe,
    base::File::Error result,
    const base::File::Info& file_info) {
  if (file_info.size < 0 || position > static_cast<uint64_t>(file_info.size)) {
    std::move(callback).Run(base::File::FILE_ERROR_FAILED, 0);
    return;
  }
  operation_runner_->Write(
      url_, std::move(data_pipe), position,
      base::BindRepeating(&FileWriterImpl::DidWrite, base::Unretained(this),
                          base::AdaptCallbackForRepeating(std::move(callback)),
                          base::Owned(new WriteState())));
}

void FileWriterImpl::DidWrite(WriteCallback callback,
                              WriteState* state,
                              base::File::Error result,
                              int64_t bytes,
                              bool complete) {
  DCHECK(state);
  state->bytes_written += bytes;
  if (complete)
    std::move(callback).Run(result, state->bytes_written);
}

void FileWriterImpl::DidTruncate(TruncateCallback callback,
                                 base::File::Error result) {
  std::move(callback).Run(result);
}

}  // namespace storage
