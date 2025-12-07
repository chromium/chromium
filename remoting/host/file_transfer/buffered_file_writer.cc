// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/file_transfer/buffered_file_writer.h"
#include "base/functional/bind.h"

#include <utility>

namespace remoting {

BufferedFileWriter::BufferedFileWriter(
    std::unique_ptr<FileOperations::Writer> file_writer,
    base::OnceClosure on_complete,
    base::OnceCallback<void(protocol::FileTransfer_Error)> on_error)
    : writer_(std::move(file_writer)),
      on_complete_(std::move(on_complete)),
      on_error_(std::move(on_error)) {
  DCHECK(writer_);
  DCHECK(writer_->state() == FileOperations::kCreated);
}

BufferedFileWriter::~BufferedFileWriter() = default;

void BufferedFileWriter::Start(const base::FilePath& filename) {
  DCHECK_EQ(kNotStarted, state_);
  SetState(kWorking);
  // Unretained is sound because no Writer callbacks will be invoked after the
  // Writer is destroyed.
  writer_->Open(filename, base::BindOnce(&BufferedFileWriter::OnOperationResult,
                                         base::Unretained(this)));
}

void BufferedFileWriter::Write(std::vector<std::uint8_t> data) {
  if (state_ == kFailed) {
    return;
  }
  DCHECK(state_ == kWorking || state_ == kWaiting);
  chunks_.push(std::move(data));

  if (state_ == kWaiting) {
    SetState(kWorking);
    WriteNextChunk();
  }
}

void BufferedFileWriter::Close() {
  if (state_ == kFailed) {
    return;
  }
  DCHECK(state_ == kWorking || state_ == kWaiting);

  State old_state = state_;
  SetState(kClosing);
  if (old_state != kWorking) {
    DoClose();
  }
}

void BufferedFileWriter::WriteNextChunk() {
  DCHECK(!chunks_.empty());
  DCHECK(state_ == kWorking || state_ == kClosing);
  std::vector<std::uint8_t> data = std::move(chunks_.front());
  chunks_.pop();
  writer_->WriteChunk(std::move(data),
                      base::BindOnce(&BufferedFileWriter::OnOperationResult,
                                     base::Unretained(this)));
}

// Handles the result from both Open and WriteChunk. For the former, it is
// called by OnWriteFileResult after setting writer_.
void BufferedFileWriter::OnOperationResult(
    FileOperations::Writer::Result result) {
  if (!result) {
    SetState(kFailed);
    std::move(on_error_).Run(std::move(result.error()));
    return;
  }

  if (!chunks_.empty()) {
    WriteNextChunk();
  } else if (state_ == kClosing) {
    DoClose();
  } else {
    SetState(kWaiting);
  }
}

void BufferedFileWriter::DoClose() {
  DCHECK(chunks_.empty());
  DCHECK_EQ(kClosing, state_);
  writer_->Close(base::BindOnce(&BufferedFileWriter::OnCloseResult,
                                base::Unretained(this)));
}

void BufferedFileWriter::OnCloseResult(FileOperations::Writer::Result result) {
  if (!result) {
    SetState(kFailed);
    std::move(on_error_).Run(std::move(result.error()));
    return;
  }

  SetState(kClosed);
  std::move(on_complete_).Run();
}

void BufferedFileWriter::SetState(BufferedFileWriter::State state) {
  switch (state) {
    case kNotStarted:
      // This is the initial state, but should never be reached again.
      NOTREACHED();
    case kWorking:
      DCHECK(state_ == kNotStarted || state_ == kWaiting);
      break;
    case kWaiting:
      DCHECK(state_ == kWorking);
      break;
    case kClosing:
      DCHECK(state_ == kWorking || state_ == kWaiting);
      break;
    case kClosed:
      DCHECK(state_ == kClosing);
      break;
    case kFailed:
      // Any state can change to kFailed.
      break;
  }

  state_ = state;
}

}  // namespace remoting
