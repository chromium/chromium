// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/file_transfer/file_transfer_message_handler.h"

#include <cstddef>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "net/base/filename_util.h"
#include "remoting/base/compound_buffer.h"
#include "remoting/protocol/file_transfer_helpers.h"
#include "url/gurl.h"

namespace remoting {

namespace {

// Used if the provided filename can't be used. (E.g., if it is empty, or if
// it consists entirely of disallowed characters.)
constexpr char kDefaultFileName[] = "crd_transfer";

// The max SCTP message size that can be safely sent in a cross-browser fashion
// is 16 KiB. Thus, 8 KiB should be a safe value even with messaging overhead.
constexpr std::size_t kChunkSize = 8192;  // 8 KiB

// The max number of chunks that should be queued for sending at one time. This
// helps smooth out spiky IO latency.
constexpr std::size_t kMaxQueuedChunks = 128;  // 128 * 8 KiB = 1 MiB

}  // namespace

FileTransferMessageHandler::FileTransferMessageHandler(
    const std::string& name,
    std::unique_ptr<protocol::MessagePipe> pipe,
    std::unique_ptr<FileOperations> file_operations)
    : protocol::NamedMessagePipeHandler(name, std::move(pipe)),
      file_operations_(std::move(file_operations)) {
  DCHECK(file_operations_);
}

FileTransferMessageHandler::~FileTransferMessageHandler() = default;

void FileTransferMessageHandler::OnConnected() {}

void FileTransferMessageHandler::OnIncomingMessage(
    std::unique_ptr<CompoundBuffer> buffer) {
  if (state_ == kFailed) {
    // Ignore any messages that come in after cancel or error.
    return;
  }

  protocol::FileTransfer message;
  CompoundBufferInputStream buffer_stream(buffer.get());
  if (!message.ParseFromZeroCopyStream(&buffer_stream)) {
    LOG(ERROR) << "Failed to parse message.";
    Cancel();
    SendError(protocol::MakeFileTransferError(
        FROM_HERE, protocol::FileTransfer_Error_Type_PROTOCOL_ERROR));
    return;
  }

  switch (message.message_case()) {
    // Writing messages.
    case protocol::FileTransfer::kMetadata:
      if (state_ != kConnected) {
        UnexpectedMessage(FROM_HERE, "metadata");
        return;
      }
      OnMetadata(std::move(*message.mutable_metadata()));
      return;
    case protocol::FileTransfer::kData:
      if (state_ != kWriting) {
        UnexpectedMessage(FROM_HERE, "data");
        return;
      }
      OnData(std::move(*message.mutable_data()->mutable_data()));
      return;
    case protocol::FileTransfer::kEnd:
      if (state_ != kWriting) {
        UnexpectedMessage(FROM_HERE, "end");
        return;
      }
      OnEnd();
      return;

    // Reading messages.
    case protocol::FileTransfer::kRequestTransfer:
      if (state_ != kConnected) {
        UnexpectedMessage(FROM_HERE, "request_transfer");
        return;
      }
      OnRequestTransfer();
      return;
    case protocol::FileTransfer::kSuccess:
      if (state_ != kEof) {
        UnexpectedMessage(FROM_HERE, "success");
        return;
      }
      OnSuccess();
      return;

    // Common messages.
    case protocol::FileTransfer::kError:
      OnError(std::move(*message.mutable_error()));
      return;

    case protocol::FileTransfer::MESSAGE_NOT_SET:
      LOG(ERROR) << "Received invalid file-transfer message.";
      Cancel();
      SendError(protocol::MakeFileTransferError(
          FROM_HERE, protocol::FileTransfer_Error_Type_PROTOCOL_ERROR));
      return;
  }
}

void FileTransferMessageHandler::OnDisconnecting() {}

void FileTransferMessageHandler::OnMetadata(
    protocol::FileTransfer::Metadata metadata) {
  SetState(kWriting);
  // Unretained is sound because the callbacks won't be called after
  // BufferedFileWriter is destroyed, which is in turn owned by this
  // FileTransferMessageHandler.
  buffered_file_writer_.emplace(
      file_operations_->CreateWriter(),
      base::BindOnce(&FileTransferMessageHandler::OnWritingComplete,
                     base::Unretained(this)),
      base::BindOnce(&FileTransferMessageHandler::OnWriteError,
                     base::Unretained(this)));
  buffered_file_writer_->Start(
      // Ensure filename is safe, and convert from UTF-8 to a FilePath.
      net::GenerateFileName(GURL(), std::string(), std::string(),
                            metadata.filename(), std::string(),
                            kDefaultFileName));
}

void FileTransferMessageHandler::OnData(std::string data) {
  DCHECK_EQ(kWriting, state_);
  buffered_file_writer_->Write(std::move(data));
}

void FileTransferMessageHandler::OnEnd() {
  DCHECK_EQ(kWriting, state_);
  SetState(kClosed);
  buffered_file_writer_->Close();
}

void FileTransferMessageHandler::OnRequestTransfer() {
  SetState(kReading);
  file_reader_ = file_operations_->CreateReader();
  // Unretained is sound because FileReader will not call us after it is
  // destroyed, and we own it.
  file_reader_->Open(base::BindOnce(&FileTransferMessageHandler::OnOpenResult,
                                    base::Unretained(this)));
}

void FileTransferMessageHandler::OnSuccess() {
  DCHECK_EQ(kEof, state_);
  SetState(kClosed);
}

void FileTransferMessageHandler::OnError(protocol::FileTransfer_Error error) {
  if (error.type() != protocol::FileTransfer_Error_Type_CANCELED) {
    LOG(ERROR) << "File transfer error from client: " << error;
  }
  Cancel();
}

void FileTransferMessageHandler::OnOpenResult(
    FileOperations::Reader::OpenResult result) {
  if (!result) {
    Cancel();
    SendError(result.error());
    return;
  }

  protocol::FileTransfer metadata_message;
  metadata_message.mutable_metadata()->set_filename(
      file_reader_->filename().AsUTF8Unsafe());
  metadata_message.mutable_metadata()->set_size(file_reader_->size());
  protocol::NamedMessagePipeHandler::Send(metadata_message, base::DoNothing());
  ReadNextChunk();
}

void FileTransferMessageHandler::OnReadResult(
    FileOperations::Reader::ReadResult result) {
  if (!result) {
    Cancel();
    SendError(result.error());
    return;
  }

  if (result->empty()) {
    SetState(kEof);
    protocol::FileTransfer end_message;
    end_message.mutable_end();
    protocol::NamedMessagePipeHandler::Send(end_message, base::DoNothing());
  } else {
    ++queued_chunks_;
    if (queued_chunks_ < kMaxQueuedChunks) {
      ReadNextChunk();
    }
    protocol::FileTransfer data_message;
    data_message.mutable_data()->set_data(std::move(*result));
    // Call Send last in case it invokes ReadNextChunk synchronously.
    protocol::NamedMessagePipeHandler::Send(
        data_message, base::BindOnce(&FileTransferMessageHandler::OnChunkSent,
                                     weak_ptr_factory_.GetWeakPtr()));
  }
}

void FileTransferMessageHandler::OnChunkSent() {
  --queued_chunks_;
  ReadNextChunk();
}

void FileTransferMessageHandler::OnWritingComplete() {
  protocol::FileTransfer success_message;
  success_message.mutable_success();
  protocol::NamedMessagePipeHandler::Send(success_message, base::DoNothing());
}

void FileTransferMessageHandler::OnWriteError(
    protocol::FileTransfer_Error error) {
  Cancel();
  SendError(std::move(error));
}

void FileTransferMessageHandler::ReadNextChunk() {
  // Make sure we haven't received an error from the client and that we're not
  // currently reading a chunk.
  if (state_ != kReading || file_reader_->state() != FileOperations::kReady) {
    return;
  }

  // Unretained is sound because file_reader_ is guaranteed not to execute any
  // callbacks after it is destroyed.
  file_reader_->ReadChunk(
      kChunkSize, base::BindOnce(&FileTransferMessageHandler::OnReadResult,
                                 base::Unretained(this)));
}

void FileTransferMessageHandler::Cancel() {
  SetState(kFailed);
  file_reader_.reset();
  // Will implicitly cancel if still in progress.
  buffered_file_writer_.reset();
}

void FileTransferMessageHandler::SendError(protocol::FileTransfer_Error error) {
  protocol::FileTransfer error_message;
  *error_message.mutable_error() = std::move(error);
  protocol::NamedMessagePipeHandler::Send(error_message, base::DoNothing());
}

void FileTransferMessageHandler::UnexpectedMessage(base::Location from_here,
                                                   const char* message) {
  LOG(ERROR) << "Unexpected file-transfer message received: " << message
             << ". Current state: " << state_;
  Cancel();
  SendError(protocol::MakeFileTransferError(
      from_here, protocol::FileTransfer_Error_Type_PROTOCOL_ERROR));
}

void FileTransferMessageHandler::SetState(State state) {
  switch (state) {
    case kConnected:
      // This is the initial state, but should never be reached again.
      NOTREACHED();
      break;
    case kReading:
      DCHECK_EQ(kConnected, state_);
      break;
    case kWriting:
      DCHECK_EQ(kConnected, state_);
      break;
    case kEof:
      DCHECK_EQ(kReading, state_);
      break;
    case kClosed:
      DCHECK(state_ == kWriting || state_ == kEof);
      break;
    case kFailed:
      // Any state can change to kFailed.
      break;
  }

  state_ = state;
}

}  // namespace remoting
