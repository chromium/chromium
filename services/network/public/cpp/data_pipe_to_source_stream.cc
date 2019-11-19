// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/data_pipe_to_source_stream.h"

#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "net/base/io_buffer.h"

namespace network {

DataPipeToSourceStream::DataPipeToSourceStream(
    mojo::ScopedDataPipeConsumerHandle body)
    : net::SourceStream(net::SourceStream::TYPE_NONE),
      body_(std::move(body)),
      handle_watcher_(FROM_HERE,
                      mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                      base::SequencedTaskRunnerHandle::Get()) {
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

  const void* buffer = nullptr;
  uint32_t available = 0;
  MojoResult result =
      body_->BeginReadData(&buffer, &available, MOJO_READ_DATA_FLAG_NONE);
  switch (result) {
    case MOJO_RESULT_OK: {
      uint32_t consume =
          std::min(base::checked_cast<uint32_t>(buf_size), available);
      memcpy(buf->data(), buffer, consume);
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
  NOTREACHED() << static_cast<int>(result);
  return net::ERR_UNEXPECTED;
}

void DataPipeToSourceStream::OnReadable(MojoResult unused) {
  // It's not expected that we call this synchronously inside Read.
  DCHECK(!inside_read_);
  DCHECK(pending_callback_);
  DCHECK(output_buf_);
  const void* buffer = nullptr;
  uint32_t available = 0;
  MojoResult result =
      body_->BeginReadData(&buffer, &available, MOJO_READ_DATA_FLAG_NONE);
  switch (result) {
    case MOJO_RESULT_OK: {
      uint32_t consume =
          std::min(base::checked_cast<uint32_t>(output_buf_size_), available);
      memcpy(output_buf_->data(), buffer, consume);
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
  NOTREACHED() << static_cast<int>(result);
}

void DataPipeToSourceStream::FinishReading() {
  complete_ = true;
  handle_watcher_.Cancel();
  body_.reset();
}

}  // namespace network
