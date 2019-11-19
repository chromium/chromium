// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/message_reader.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "remoting/base/compound_buffer.h"
#include "remoting/proto/internal.pb.h"
#include "remoting/protocol/p2p_stream_socket.h"

namespace remoting {
namespace protocol {

static const int kReadBufferSize = 4096;

MessageReader::MessageReader() {}
MessageReader::~MessageReader() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void MessageReader::StartReading(
    P2PStreamSocket* socket,
    const MessageReceivedCallback& message_received_callback,
    const ReadFailedCallback& read_failed_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!socket_);
  DCHECK(socket);
  DCHECK(!message_received_callback.is_null());
  DCHECK(!read_failed_callback.is_null());

  socket_ = socket;
  message_received_callback_ = message_received_callback;
  read_failed_callback_ = read_failed_callback;
  DoRead();
}

void MessageReader::DoRead() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Don't try to read again if there is another read pending or we
  // have messages that we haven't finished processing yet.
  bool read_succeeded = true;
  while (read_succeeded && !closed_ && !read_pending_) {
    read_buffer_ = base::MakeRefCounted<net::IOBuffer>(kReadBufferSize);
    int result = socket_->Read(
        read_buffer_.get(),
        kReadBufferSize,
        base::Bind(&MessageReader::OnRead, weak_factory_.GetWeakPtr()));

    HandleReadResult(result, &read_succeeded);
  }
}

void MessageReader::OnRead(int result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(read_pending_);
  read_pending_ = false;

  if (!closed_) {
    bool read_succeeded;
    HandleReadResult(result, &read_succeeded);
    if (read_succeeded)
      DoRead();
  }
}

void MessageReader::HandleReadResult(int result, bool* read_succeeded) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (closed_)
    return;

  *read_succeeded = true;

  if (result > 0) {
    OnDataReceived(read_buffer_.get(), result);
    *read_succeeded = true;
  } else if (result == net::ERR_IO_PENDING) {
    read_pending_ = true;
  } else {
    // Stop reading after any error.
    closed_ = true;
    *read_succeeded = false;

    LOG(ERROR) << "Read() returned error " << result;
    read_failed_callback_.Run(result);
  }
}

void MessageReader::OnDataReceived(net::IOBuffer* data, int data_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  message_decoder_.AddData(data, data_size);

  // Get list of all new messages first, and then call the callback
  // for all of them.
  while (true) {
    CompoundBuffer* buffer = message_decoder_.GetNextMessage();
    if (!buffer)
      break;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&MessageReader::RunCallback, weak_factory_.GetWeakPtr(),
                       base::Passed(base::WrapUnique(buffer))));
  }
}

void MessageReader::RunCallback(std::unique_ptr<CompoundBuffer> message) {
  if (!message_received_callback_.is_null()) {
    message_received_callback_.Run(std::move(message));
  }
}

}  // namespace protocol
}  // namespace remoting
