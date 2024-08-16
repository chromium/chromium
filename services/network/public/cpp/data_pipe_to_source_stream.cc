// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/data_pipe_to_source_stream.h"

#include <utility>

#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "base/numerics/checked_math.h"
#include "base/task/sequenced_task_runner.h"
#include "net/base/io_buffer.h"

namespace network {

DataPipeToSourceStream::DataPipeToSourceStream(
    mojo::ScopedDataPipeConsumerHandle body)
    : net::SourceStream(net::SourceStream::TYPE_NONE),
      body_(std::move(body)),
      handle_watcher_(FROM_HERE,
                      mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                      base::SequencedTaskRunner::GetCurrentDefault()) {
  handle_watcher_.Watch(
      body_.get(), MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      base::BindRepeating(&DataPipeToSourceStream::OnReadable,
                          base::Unretained(this)));
}

DataPipeToSourceStream::~DataPipeToSourceStream() = default;

std::string DataPipeToSourceStream::Description() const {
  return "DataPipe";
}

bool DataPipeToSourceStream::MayHaveMoreBytes() const {
  return !complete_;
}

int DataPipeToSourceStream::Read(net::IOBuffer* buf,
                                 int buf_size,
                                 net::CompletionOnceCallback callback) {
  base::AutoReset<bool> inside_read_checker(&inside_read_, true);

  if (!body_.get()) {
    // We have finished reading the pipe.
    return 0;
  }

  base::span<const uint8_t> buffer;
  MojoResult result = body_->BeginReadData(MOJO_READ_DATA_FLAG_NONE, buffer);
  switch (result) {
    case MOJO_RESULT_OK: {
      size_t consume =
          std::min(base::checked_cast<size_t>(buf_size), buffer.size());
      buf->span().copy_prefix_from(buffer.first(consume));
      body_->EndReadData(consume);
      return base::checked_cast<int>(consume);
    }
    case MOJO_RESULT_FAILED_PRECONDITION:
      // Finished reading.
      FinishReading();
      return 0;
    case MOJO_RESULT_SHOULD_WAIT:
      // Data is not available yet.
      pending_callback_ = std::move(callback);
      output_buf_ = buf;
      output_buf_size_ = buf_size;
      handle_watcher_.ArmOrNotify();
      return net::ERR_IO_PENDING;
  }
  NOTREACHED_IN_MIGRATION() << static_cast<int>(result);
  return net::ERR_UNEXPECTED;
}

void DataPipeToSourceStream::OnReadable(MojoResult unused) {
  // It's not expected that we call this synchronously inside Read.
  DCHECK(!inside_read_);
  DCHECK(pending_callback_);
  DCHECK(output_buf_);
  base::span<const uint8_t> buffer;
  MojoResult result = body_->BeginReadData(MOJO_READ_DATA_FLAG_NONE, buffer);
  switch (result) {
    case MOJO_RESULT_OK: {
      size_t consume =
          std::min(base::checked_cast<size_t>(output_buf_size_), buffer.size());
      output_buf_->span().copy_prefix_from(buffer.first(consume));
      body_->EndReadData(consume);
      std::move(pending_callback_).Run(consume);
      return;
    }
    case MOJO_RESULT_FAILED_PRECONDITION:
      FinishReading();
      std::move(pending_callback_).Run(0);
      return;
    case MOJO_RESULT_SHOULD_WAIT:
      handle_watcher_.ArmOrNotify();
      return;
  }
  NOTREACHED_IN_MIGRATION() << static_cast<int>(result);
}

void DataPipeToSourceStream::FinishReading() {
  complete_ = true;
  handle_watcher_.Cancel();
  body_.reset();
}

}  // namespace network
