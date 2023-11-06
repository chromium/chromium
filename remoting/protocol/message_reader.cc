// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/message_reader.h"

#include <utility>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/task/single_thread_task_runner.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "remoting/base/compound_buffer.h"
#include "remoting/proto/internal.pb.h"
#include "remoting/protocol/p2p_stream_socket.h"

namespace remoting::protocol {

static const int kReadBufferSize = 4096;

MessageReader::MessageReader() = default;
MessageReader::~MessageReader() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void MessageReader::StartReading(
    P2PStreamSocket* socket,
    const MessageReceivedCallback& message_received_callback,
    ReadFailedCallback read_failed_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!socket_);
  DCHECK(socket);
  DCHECK(message_received_callback);
  DCHECK(read_failed_callback);

  socket_ = socket;
  message_received_callback_ = message_received_callback;
  read_failed_callback_ = std::move(read_failed_callback);
  DoRead();
}

void MessageReader::DoRead() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Don't try to read again if there is another read pending or we
  // have messages that we haven't finished processing yet.
  while (!closed_ && !read_pending_) {
    read_buffer_ = base::MakeRefCounted<net::IOBufferWithSize>(kReadBufferSize);
    int result = socket_->Read(
        read_buffer_.get(), kReadBufferSize,
        base::BindOnce(&MessageReader::OnRead, weak_factory_.GetWeakPtr()));

    if (!HandleReadResult(result)) {
      break;
    }
  }
}

void MessageReader::OnRead(int result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(read_pending_);
  read_pending_ = false;

  if (!closed_) {
    HandleReadResult(result);
    DoRead();
  }
}

bool MessageReader::HandleReadResult(int result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!closed_);

  if (result > 0) {
    OnDataReceived(read_buffer_.get(), result);
    return true;
  }

  if (result == net::ERR_IO_PENDING) {
    read_pending_ = true;
    return true;
  }

  // Stop reading after any error.
  closed_ = true;
  LOG(ERROR) << "Read() returned error " << result;
  std::move(read_failed_callback_).Run(result);
  // |this| may be deleted.
  return false;
}

void MessageReader::OnDataReceived(net::IOBuffer* data, int data_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  message_decoder_.AddData(data, data_size);

  // Get list of all new messages first, and then call the callback
  // for all of them.
  while (true) {
    CompoundBuffer* buffer = message_decoder_.GetNextMessage();
    if (!buffer) {
      break;
    }
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&MessageReader::RunCallback, weak_factory_.GetWeakPtr(),
                       base::WrapUnique(buffer)));
  }
}

void MessageReader::RunCallback(std::unique_ptr<CompoundBuffer> message) {
  if (message_received_callback_) {
    message_received_callback_.Run(std::move(message));
  }
}

}  // namespace remoting::protocol
