// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/file_transfer/session_file_operations_handler.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "remoting/protocol/file_transfer_helpers.h"

namespace remoting {

SessionFileOperationsHandler::SessionFileOperationsHandler(
    IpcFileOperations::ResultHandler* result_handler,
    std::unique_ptr<FileOperations> file_operations)
    : result_handler_(result_handler),
      file_operations_(std::move(file_operations)) {}

void SessionFileOperationsHandler::ReadFile(std::uint64_t file_id) {
  auto location = readers_.emplace(file_id, file_operations_->CreateReader());
  if (!location.second) {
    // Strange, we've already received a ReadFile message for this ID. Cancel
    // it and send an error to be safe.
    readers_.erase(location.first);
    result_handler_->OnInfoResult(
        file_id,
        protocol::MakeFileTransferError(
            FROM_HERE, protocol::FileTransfer_Error_Type_UNEXPECTED_ERROR));
    return;
  }
  // Unretained is sound because no Reader callbacks will be invoked after the
  // Writer is destroyed.
  location.first->second->Open(
      base::BindOnce(&SessionFileOperationsHandler::OnReaderOpenResult,
                     base::Unretained(this), file_id));
}

void SessionFileOperationsHandler::ReadChunk(std::uint64_t file_id,
                                             std::uint64_t size) {
  auto reader_iter = readers_.find(file_id);
  if (reader_iter == readers_.end()) {
    result_handler_->OnDataResult(
        file_id,
        protocol::MakeFileTransferError(
            FROM_HERE, protocol::FileTransfer_Error_Type_UNEXPECTED_ERROR));
    return;
  }
  reader_iter->second->ReadChunk(
      size, base::BindOnce(&SessionFileOperationsHandler::OnReaderReadResult,
                           base::Unretained(this), file_id));
}

void SessionFileOperationsHandler::WriteFile(uint64_t file_id,
                                             const base::FilePath& filename) {
  auto location = writers_.emplace(file_id, file_operations_->CreateWriter());
  if (!location.second) {
    // Strange, we've already received a WriteFile message for this ID. Cancel
    // it and send an error to be safe.
    writers_.erase(location.first);
    result_handler_->OnResult(
        file_id,
        protocol::MakeFileTransferError(
            FROM_HERE, protocol::FileTransfer_Error_Type_UNEXPECTED_ERROR));
    return;
  }
  // Unretained is sound because no Writer callbacks will be invoked after the
  // Writer is destroyed.
  location.first->second->Open(
      filename,
      base::BindOnce(&SessionFileOperationsHandler::OnWriterOperationResult,
                     base::Unretained(this), file_id));
}

void SessionFileOperationsHandler::WriteChunk(uint64_t file_id,
                                              std::vector<std::uint8_t> data) {
  auto writer_iter = writers_.find(file_id);
  if (writer_iter == writers_.end()) {
    result_handler_->OnResult(
        file_id,
        protocol::MakeFileTransferError(
            FROM_HERE, protocol::FileTransfer_Error_Type_UNEXPECTED_ERROR));
    return;
  }
  writer_iter->second->WriteChunk(
      std::move(data),
      base::BindOnce(&SessionFileOperationsHandler::OnWriterOperationResult,
                     base::Unretained(this), file_id));
}

void SessionFileOperationsHandler::Close(uint64_t file_id) {
  auto reader_iter = readers_.find(file_id);
  if (reader_iter != readers_.end()) {
    readers_.erase(reader_iter);
    return;
  }

  auto writer_iter = writers_.find(file_id);
  if (writer_iter != writers_.end()) {
    writer_iter->second->Close(
        base::BindOnce(&SessionFileOperationsHandler::OnWriterCloseResult,
                       base::Unretained(this), file_id));
    return;
  }

  // |file_id| is not a known reader or writer. Send an error in case the
  // network process is waiting for a response.
  result_handler_->OnResult(
      file_id,
      protocol::MakeFileTransferError(
          FROM_HERE, protocol::FileTransfer_Error_Type_UNEXPECTED_ERROR));
}

void SessionFileOperationsHandler::Cancel(uint64_t file_id) {
  // It's possible for a cancel request and an error response (from a previous
  // request) to pass each other in the message pipe, so the lack of a reader or
  // writer matching |file_id| is not necessarily an error. Since a cancel
  // request doesn't expect a response, it's fine to do nothing in that case.

  auto reader_iter = readers_.find(file_id);
  if (reader_iter != readers_.end()) {
    readers_.erase(reader_iter);
  }

  auto writer_iter = writers_.find(file_id);
  if (writer_iter != writers_.end()) {
    writers_.erase(writer_iter);
  }
}

void SessionFileOperationsHandler::OnReaderOpenResult(
    std::uint64_t file_id,
    FileOperations::Reader::OpenResult result) {
  if (!result) {
    readers_.erase(file_id);
    result_handler_->OnInfoResult(file_id, std::move(result.error()));
    return;
  }

  auto reader_iter = readers_.find(file_id);
  // Should never get a callback if the reader has been destroyed.
  DCHECK(reader_iter != readers_.end());
  result_handler_->OnInfoResult(file_id,
                                {kSuccessTag, reader_iter->second->filename(),
                                 reader_iter->second->size()});
}

void SessionFileOperationsHandler::OnReaderReadResult(
    std::uint64_t file_id,
    FileOperations::Reader::ReadResult result) {
  if (!result || result->size() == 0) {
    readers_.erase(file_id);
  }

  result_handler_->OnDataResult(file_id, std::move(result));
}

void SessionFileOperationsHandler::OnWriterOperationResult(
    uint64_t file_id,
    FileOperations::Writer::Result result) {
  if (!result) {
    writers_.erase(file_id);
  }
  result_handler_->OnResult(file_id, std::move(result));
}

void SessionFileOperationsHandler::OnWriterCloseResult(
    uint64_t file_id,
    FileOperations::Writer::Result result) {
  writers_.erase(file_id);
  result_handler_->OnResult(file_id, std::move(result));
}

SessionFileOperationsHandler::~SessionFileOperationsHandler() = default;

}  // namespace remoting
