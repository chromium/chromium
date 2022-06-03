// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/source_stream_to_data_pipe.h"

#include "base/bind.h"
#include "net/filter/source_stream.h"

namespace network {

SourceStreamToDataPipe::SourceStreamToDataPipe(
    std::unique_ptr<net::SourceStream> source,
    mojo::ScopedDataPipeProducerHandle dest)
    : source_(std::move(source)),
      dest_(std::move(dest)),
      writable_handle_watcher_(FROM_HERE,
                               mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                               base::SequencedTaskRunnerHandle::Get()) {
  writable_handle_watcher_.Watch(
      dest_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
      base::BindRepeating(&SourceStreamToDataPipe::OnDataPipeWritable,
                          base::Unretained(this)));
}

SourceStreamToDataPipe::~SourceStreamToDataPipe() = default;

void SourceStreamToDataPipe::Start(
    base::OnceCallback<void(int)> completion_callback) {
  completion_callback_ = std::move(completion_callback);
  ReadMore();
}

void SourceStreamToDataPipe::ReadMore() {
  DCHECK(!pending_write_.get());

  uint32_t num_bytes;
  MojoResult mojo_result = network::NetToMojoPendingBuffer::BeginWrite(
      &dest_, &pending_write_, &num_bytes);
  if (mojo_result == MOJO_RESULT_SHOULD_WAIT) {
    // The pipe is full.  We need to wait for it to have more space.
    writable_handle_watcher_.ArmOrNotify();
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

  scoped_refptr<net::IOBuffer> buffer(
      new network::NetToMojoIOBuffer(pending_write_.get()));
  int result = source_->Read(
      buffer.get(), base::checked_cast<int>(num_bytes),
      base::BindOnce(&SourceStreamToDataPipe::DidRead, base::Unretained(this)));

  if (result != net::ERR_IO_PENDING)
    DidRead(result);
}

void SourceStreamToDataPipe::DidRead(int result) {
  DCHECK(pending_write_);
  if (result <= 0) {
    // An error, or end of the stream.
    pending_write_->Complete(0);  // Closes the data pipe.
    OnComplete(result);
    return;
  }

  dest_ = pending_write_->Complete(result);
  transferred_bytes_ += result;

  // Don't hop through an extra ReadMore just to find out there's no more data.
  if (!source_->MayHaveMoreBytes()) {
    OnComplete(net::OK);
    return;
  }

  pending_write_ = nullptr;

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&SourceStreamToDataPipe::ReadMore,
                                weak_factory_.GetWeakPtr()));
}

void SourceStreamToDataPipe::OnDataPipeWritable(MojoResult result) {
  if (result == MOJO_RESULT_FAILED_PRECONDITION) {
    OnComplete(net::ERR_ABORTED);
    return;
  }
  DCHECK_EQ(result, MOJO_RESULT_OK) << result;

  ReadMore();
}

void SourceStreamToDataPipe::OnComplete(int result) {
  // Resets the watchers, pipes and the exchange handler, so that
  // we will never be called back.
  writable_handle_watcher_.Cancel();
  pending_write_ = nullptr;
  dest_.reset();

  std::move(completion_callback_).Run(result);
}

}  // namespace network
