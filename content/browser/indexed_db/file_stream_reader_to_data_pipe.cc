// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/file_stream_reader_to_data_pipe.h"

#include "base/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "net/base/net_errors.h"

namespace content {

FileStreamReaderToDataPipe::FileStreamReaderToDataPipe(
    std::unique_ptr<storage::FileStreamReader> reader,
    mojo::ScopedDataPipeProducerHandle dest)
    : reader_(std::move(reader)), dest_(std::move(dest)) {}

FileStreamReaderToDataPipe::~FileStreamReaderToDataPipe() = default;

void FileStreamReaderToDataPipe::Start(
    base::OnceCallback<void(int)> completion_callback,
    uint64_t read_length) {
  DCHECK(!writable_handle_watcher_.has_value());
  writable_handle_watcher_.emplace(
      FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL,
      base::SequencedTaskRunner::GetCurrentDefault());
  writable_handle_watcher_->Watch(
      dest_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
      base::BindRepeating(&FileStreamReaderToDataPipe::OnDataPipeWritable,
                          base::Unretained(this)));

  read_length_ = read_length;
  completion_callback_ = std::move(completion_callback);
  ReadMore();
}

void FileStreamReaderToDataPipe::ReadMore() {
  DCHECK(!pending_write_.get());

  uint32_t num_bytes;
  MojoResult mojo_result = network::NetToMojoPendingBuffer::BeginWrite(
      &dest_, &pending_write_, &num_bytes);
  if (mojo_result == MOJO_RESULT_SHOULD_WAIT) {
    // The pipe is full.  We need to wait for it to have more space.
    writable_handle_watcher_->ArmOrNotify();
    return;
  } else if (mojo_result == MOJO_RESULT_FAILED_PRECONDITION) {
    // The data pipe consumer handle has been closed.
    OnComplete(net::ERR_ABORTED);
    return;
  } else if (mojo_result != MOJO_RESULT_OK) {
    // The body stream is in a bad state. Bail out.
    OnComplete(net::ERR_UNEXPECTED);
    return;
  }

  uint64_t read_bytes = std::min(static_cast<uint64_t>(num_bytes),
                                 read_length_ - transferred_bytes_);

  auto buffer =
      base::MakeRefCounted<network::NetToMojoIOBuffer>(pending_write_.get());
  int result =
      reader_->Read(buffer.get(), base::checked_cast<int>(read_bytes),
                    base::BindOnce(&FileStreamReaderToDataPipe::DidRead,
                                   base::Unretained(this)));

  if (result != net::ERR_IO_PENDING)
    DidRead(result);
}

void FileStreamReaderToDataPipe::DidRead(int result) {
  DCHECK(pending_write_);
  if (result <= 0) {
    // An error, or end of the stream.
    pending_write_->Complete(0);  // Closes the data pipe.
    OnComplete(result);
    return;
  }

  dest_ = pending_write_->Complete(result);
  transferred_bytes_ += result;

  if (transferred_bytes_ >= read_length_) {
    OnComplete(net::OK);
    return;
  }

  pending_write_ = nullptr;

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&FileStreamReaderToDataPipe::ReadMore,
                                weak_factory_.GetWeakPtr()));
}

void FileStreamReaderToDataPipe::OnDataPipeWritable(MojoResult result) {
  if (result == MOJO_RESULT_FAILED_PRECONDITION) {
    OnComplete(net::ERR_ABORTED);
    return;
  }
  DCHECK_EQ(result, MOJO_RESULT_OK) << result;

  ReadMore();
}

void FileStreamReaderToDataPipe::OnComplete(int result) {
  // Resets the watchers, pipes and the exchange handler, so that
  // we will never be called back.
  writable_handle_watcher_->Cancel();
  pending_write_ = nullptr;
  dest_.reset();

  std::move(completion_callback_).Run(result);
}

}  // namespace content
