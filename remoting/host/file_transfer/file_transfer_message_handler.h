// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_FILE_TRANSFER_FILE_TRANSFER_MESSAGE_HANDLER_H_
#define REMOTING_HOST_FILE_TRANSFER_FILE_TRANSFER_MESSAGE_HANDLER_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/queue.h"
#include "base/memory/weak_ptr.h"
#include "remoting/host/file_transfer/buffered_file_writer.h"
#include "remoting/host/file_transfer/file_operations.h"
#include "remoting/protocol/file_transfer_helpers.h"
#include "remoting/protocol/named_message_pipe_handler.h"

namespace remoting {

constexpr char kFileTransferDataChannelPrefix[] = "filetransfer-";

class FileTransferMessageHandler : public protocol::NamedMessagePipeHandler {
 public:
  FileTransferMessageHandler(const std::string& name,
                             std::unique_ptr<protocol::MessagePipe> pipe,
                             std::unique_ptr<FileOperations> file_operations);
  ~FileTransferMessageHandler() override;

  // protocol::NamedMessagePipeHandler implementation.
  void OnConnected() override;
  void OnIncomingMessage(std::unique_ptr<CompoundBuffer> message) override;
  void OnDisconnecting() override;

 private:
  enum State {
    // Initial state.
    kConnected,
    // Writing a file from the client.
    kWriting,
    // Reading a file and sending to the client.
    kReading,
    // Reading complete and waiting for confirmation from client.
    kEof,
    // End states
    // File successfully written.
    kClosed,
    // An error occured or the transfer was canceled.
    kFailed,
  };

  // Handlers for specific messages from the client.
  void OnMetadata(protocol::FileTransfer_Metadata metadata);
  void OnData(std::vector<std::uint8_t> data);
  void OnEnd();
  void OnRequestTransfer();
  void OnSuccess();
  void OnError(protocol::FileTransfer_Error error);

  // File reading callbacks.
  void OnOpenResult(FileOperations::Reader::OpenResult result);
  void OnReadResult(FileOperations::Reader::ReadResult result);
  void OnChunkSent();

  // File writing callbacks.
  void OnWritingComplete();
  void OnWriteError(protocol::FileTransfer_Error error);

  // Reads the next chunk in reading mode.
  void ReadNextChunk();

  // Cancels any current operation and transitions to failed state.
  void Cancel();

  // Sends an error message to the client.
  void SendError(protocol::FileTransfer_Error error);

  // Handles an unexpected message being received.
  void UnexpectedMessage(base::Location from_here, const char* message);

  void SetState(State state);

  State state_ = kConnected;
  std::unique_ptr<FileOperations> file_operations_;
  std::optional<BufferedFileWriter> buffered_file_writer_;
  std::unique_ptr<FileOperations::Reader> file_reader_;
  std::size_t queued_chunks_ = 0;
  base::WeakPtrFactory<FileTransferMessageHandler> weak_ptr_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_FILE_TRANSFER_FILE_TRANSFER_MESSAGE_HANDLER_H_
