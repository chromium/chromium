// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/source_stream_to_data_pipe.h"

#include "base/functional/bind.h"
#include "base/numerics/checked_math.h"
#include "base/task/sequenced_task_runner.h"
#include "net/base/net_errors.h"
#include "net/filter/source_stream.h"

namespace network {

SourceStreamToDataPipe::SourceStreamToDataPipe(
    std::unique_ptr<net::SourceStream> source,
    mojo::ScopedDataPipeProducerHandle dest,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : source_(std::move(source)),
      dest_(std::move(dest)),
      writable_handle_watcher_(FROM_HERE,
                               mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                               std::move(task_runner)) {
  writable_handle_watcher_.Watch(
      dest_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
      base::BindRepeating(&SourceStreamToDataPipe::OnDataPipeWritable,
                          base::Unretained(this)));
}

SourceStreamToDataPipe::~SourceStreamToDataPipe() = default;

void SourceStreamToDataPipe::Start(
    base::OnceCallback<void(int)> completion_callback) {
  completion_callback_ = std::move(completion_callback);
  next_state_ = State::kBeginWrite;
  int result = DoLoop(net::OK);
  if (result != net::ERR_IO_PENDING) {
    CleanupAndRunCallback(result);
  }
}

int SourceStreamToDataPipe::DoLoop(int result) {
  int rv = result;
  do {
    State state = next_state_;
    next_state_ = State::kNone;
    switch (state) {
      case State::kBeginWrite:
        CHECK_EQ(rv, net::OK);
        rv = DoBeginWrite();
        break;
      case State::kBeginWriteComplete:
        rv = DoBeginWriteComplete(rv);
        break;
      case State::kReadData:
        CHECK_EQ(rv, net::OK);
        rv = DoReadData();
        break;
      case State::kReadDataComplete:
        rv = DoReadDataComplete(rv);
        break;
      default:
        NOTREACHED() << "bad state";
    }
  } while (rv != net::ERR_IO_PENDING && next_state_ != State::kNone);
  return rv;
}

int SourceStreamToDataPipe::DoBeginWrite() {
  DCHECK(!pending_write_.get());
  next_state_ = State::kBeginWriteComplete;
  MojoResult mojo_result =
      network::NetToMojoPendingBuffer::BeginWrite(&dest_, &pending_write_);
  switch (mojo_result) {
    case MOJO_RESULT_OK:
      return net::OK;
    case MOJO_RESULT_SHOULD_WAIT:
      // The pipe is full.  We need to wait for it to have more space.
      writable_handle_watcher_.ArmOrNotify();
      return net::ERR_IO_PENDING;
    case MOJO_RESULT_FAILED_PRECONDITION:
      // The data pipe consumer handle has been closed.
      return net::ERR_ABORTED;
    default:
      // The body stream is in a bad state. Bail out.
      return net::ERR_UNEXPECTED;
  }
  NOTREACHED();
}

void SourceStreamToDataPipe::OnDataPipeWritable(MojoResult mojo_result) {
  CHECK_EQ(next_state_, State::kBeginWriteComplete);
  CHECK((mojo_result == MOJO_RESULT_OK) ||
        (mojo_result == MOJO_RESULT_FAILED_PRECONDITION))
      << mojo_result;
  if (mojo_result == MOJO_RESULT_OK) {
    // When the pipe is writable, call BeginWrite() again.
    next_state_ = State::kBeginWrite;
  }
  OnIOComplete(mojo_result == MOJO_RESULT_OK ? net::OK : net::ERR_ABORTED);
}

int SourceStreamToDataPipe::DoBeginWriteComplete(int result) {
  next_state_ = result == net::OK ? State::kReadData : State::kNone;
  return result;
}

int SourceStreamToDataPipe::DoReadData() {
  next_state_ = State::kReadDataComplete;
  CHECK(pending_write_);
  CHECK(source_);
  int num_bytes = base::checked_cast<int>(pending_write_->size());
  auto buffer = base::MakeRefCounted<NetToMojoIOBuffer>(pending_write_);
  return source_->Read(buffer.get(), num_bytes,
                       base::BindOnce(&SourceStreamToDataPipe::DidRead,
                                      weak_factory_.GetWeakPtr()));
}

void SourceStreamToDataPipe::DidRead(int result) {
  CHECK_EQ(next_state_, State::kReadDataComplete);
  OnIOComplete(result);
}

void SourceStreamToDataPipe::OnIOComplete(int result) {
  result = DoLoop(result);

  if (result != net::ERR_IO_PENDING) {
    CleanupAndRunCallback(result);
  }
}

int SourceStreamToDataPipe::DoReadDataComplete(int result) {
  DCHECK(pending_write_);
  if (result <= 0) {
    // An error, or end of the stream.
    pending_write_->Complete(0);  // Closes the data pipe.
    return result;
  }
  dest_ = pending_write_->Complete(result);
  pending_write_.reset();
  transferred_bytes_ += result;

  // Don't hop through an extra ReadMore just to find out there's no more data.
  if (source_->MayHaveMoreBytes()) {
    next_state_ = State::kBeginWrite;
  }
  return net::OK;
}

void SourceStreamToDataPipe::CleanupAndRunCallback(int result) {
  // Resets the watchers, pipes and the exchange handler, so that
  // we will never be called back.
  writable_handle_watcher_.Cancel();
  pending_write_ = nullptr;
  dest_.reset();
  CHECK(completion_callback_);
  std::move(completion_callback_).Run(result);
}

}  // namespace network
