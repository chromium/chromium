// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_FILE_TRANSFER_BUFFERED_FILE_WRITER_H_
#define REMOTING_HOST_FILE_TRANSFER_BUFFERED_FILE_WRITER_H_

#include <cstdint>
#include <memory>
#include <vector>

#include "base/containers/queue.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "remoting/host/file_transfer/file_operations.h"
#include "remoting/proto/file_transfer.pb.h"

namespace remoting {

// Wrapper around FileOperations::WriteFile that automatically handles queuing
// operations. Write can be called immediately after start, can be called
// multiple times in sequence, and close can be called at any time. Internally,
// BufferedFileWriter will maintain a queue of written chunks and feed them to
// the Writer as the latter is ready for them.
class BufferedFileWriter {
 public:
  // Constructor.
  // |file_writer| should be in the kCreated state. |on_error| may be called at
  // any time if any operation fails. If no error occurs, |on_complete| will be
  // called after Close() has been called and all chunks have been successfully
  // written. Callbacks will never be called after BufferedFileWriter is
  // destroyed.
  BufferedFileWriter(
      std::unique_ptr<FileOperations::Writer> file_writer,
      base::OnceClosure on_complete,
      base::OnceCallback<void(protocol::FileTransfer_Error)> on_error);

  // Cancels the underlying Writer. If Close has already been called, this will
  // either do nothing (if writing the file has already completed) or cancel
  // writing out the file (if there are still chunks waiting be be written).
  // No callbacks will be invoked.
  ~BufferedFileWriter();

  // Start writing a new file using the provided FileOperations implementation.
  // Must be called exactly once before any other methods.
  void Start(const base::FilePath& filename);

  // Enqueue the provided chunk to be written to the file.
  void Write(std::vector<std::uint8_t> data);

  // Close the file. If any chunks are currently queued, they will be written
  // before the file is closed.
  void Close();

 private:
  enum State {
    // Initial state.
    kNotStarted,
    // A file operation is in progress.
    kWorking,
    // Waiting for data.
    kWaiting,
    // Close called, but file operations still pending.
    kClosing,
    // End states
    // File successfully written.
    kClosed,
    // An error occured or the transfer was canceled.
    kFailed,
  };

  void WriteNextChunk();
  void OnOperationResult(FileOperations::Writer::Result result);
  void DoClose();
  void OnCloseResult(FileOperations::Writer::Result result);
  void SetState(State state);

  // Tracks internal state.
  State state_ = kNotStarted;

  // Underlying Writer instance.
  std::unique_ptr<FileOperations::Writer> writer_;

  // Called once all writes are completed and the file is closed.
  base::OnceClosure on_complete_;

  // Called if there is an error at any stage. If this is called, on_complete_
  // won't be.
  base::OnceCallback<void(protocol::FileTransfer_Error)> on_error_;

  // Chunks that have been provided to Write but have not yet been passed to the
  // Writer instance.
  base::queue<std::vector<std::uint8_t>> chunks_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_FILE_TRANSFER_BUFFERED_FILE_WRITER_H_
