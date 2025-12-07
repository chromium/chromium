// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/socket_data_pump.h"

#include <optional>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/safe_conversions.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/log/net_log.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/client_socket_handle.h"
#include "services/network/tls_client_socket.h"

namespace network {

SocketDataPump::SocketDataPump(
    net::StreamSocket* socket,
    Delegate* delegate,
    mojo::ScopedDataPipeProducerHandle receive_pipe_handle,
    mojo::ScopedDataPipeConsumerHandle send_pipe_handle,
    const net::NetworkTrafficAnnotationTag& traffic_annotation)
    : socket_(socket),
      delegate_(delegate),
      receive_stream_(std::move(receive_pipe_handle)),
      receive_stream_watcher_(FROM_HERE,
                              mojo::SimpleWatcher::ArmingPolicy::MANUAL),
      receive_stream_close_watcher_(FROM_HERE,
                                    mojo::SimpleWatcher::ArmingPolicy::MANUAL),
      send_stream_(std::move(send_pipe_handle)),
      send_stream_watcher_(FROM_HERE,
                           mojo::SimpleWatcher::ArmingPolicy::MANUAL),
      traffic_annotation_(traffic_annotation) {
  send_stream_watcher_.Watch(
      send_stream_.get(),
      MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      base::BindRepeating(&SocketDataPump::OnSendStreamReadable,
                          base::Unretained(this)));
  receive_stream_watcher_.Watch(
      receive_stream_.get(),
      MOJO_HANDLE_SIGNAL_WRITABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      base::BindRepeating(&SocketDataPump::OnReceiveStreamWritable,
                          base::Unretained(this)));
  receive_stream_close_watcher_.Watch(
      receive_stream_.get(), MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      base::BindRepeating(&SocketDataPump::OnReceiveStreamClosed,
                          base::Unretained(this)));
  ReceiveMore();
  SendMore();
}

SocketDataPump::~SocketDataPump() {}

void SocketDataPump::ReceiveMore() {
  CHECK(!receive_is_shutdown_);
  CHECK(receive_stream_.is_valid());
  CHECK(!read_if_ready_pending_);

  scoped_refptr<NetToMojoPendingBuffer> pending_receive_buffer;
  MojoResult result = NetToMojoPendingBuffer::BeginWrite(
      &receive_stream_, &pending_receive_buffer);
  switch (result) {
    case MOJO_RESULT_OK:
      break;
    case MOJO_RESULT_SHOULD_WAIT:
      receive_stream_watcher_.ArmOrNotify();
      return;
    default:
      ShutdownReceive();
      return;
  }
  uint32_t num_bytes = pending_receive_buffer->size();
  auto buf = base::MakeRefCounted<NetToMojoIOBuffer>(pending_receive_buffer);
  int read_result = socket_->ReadIfReady(
      buf.get(), base::saturated_cast<int>(num_bytes),
      base::BindOnce(&SocketDataPump::OnNetworkReadCompleted,
                     // WeakPtr because `socket_` may outlive `this`.
                     weak_factory_.GetWeakPtr(),
                     /* pending_receive_buffer=*/nullptr));

  if (read_result == net::ERR_READ_IF_READY_NOT_IMPLEMENTED) {
    read_result = socket_->Read(
        buf.get(), base::saturated_cast<int>(num_bytes),
        base::BindOnce(&SocketDataPump::OnNetworkReadCompleted,
                       // WeakPtr because `socket_` may outlive `this`.
                       weak_factory_.GetWeakPtr(), pending_receive_buffer));
    if (read_result == net::ERR_IO_PENDING) {
      receive_stream_close_watcher_.ArmOrNotify();
      return;
    }
  } else if (read_result == net::ERR_IO_PENDING) {
    // The callback passed to `ReadIfReady()` will be invoked asynchronously
    // when data is available on when an error occurs. Invoke `Complete()` now
    // since the buffer won't be used.
    receive_stream_ = pending_receive_buffer->Complete(/* num_bytes=*/0);
    read_if_ready_pending_ = true;
    receive_stream_close_watcher_.ArmOrNotify();
    return;
  }

  CHECK_NE(read_result, net::ERR_IO_PENDING);
  OnNetworkReadCompleted(std::move(pending_receive_buffer), read_result);
}

void SocketDataPump::OnReceiveStreamClosed(MojoResult result) {
  ShutdownReceive();
  return;
}

void SocketDataPump::OnReceiveStreamWritable(MojoResult result) {
  CHECK(!receive_is_shutdown_);
  CHECK(receive_stream_.is_valid());

  if (result != MOJO_RESULT_OK) {
    ShutdownReceive();
    return;
  }
  ReceiveMore();
}

void SocketDataPump::OnNetworkReadCompleted(
    scoped_refptr<NetToMojoPendingBuffer> pending_receive_buffer,
    int result) {
  // `ShutdownReceive()` cancels a pending `ReadIfReady()` but not a pending
  // `Read()`, so this can be invoked after `ShutdownReceive()`. No-op in that
  // case.
  if (receive_is_shutdown_) {
    return;
  }

  // When a `ReadIfReady` is pending:
  // - Result = net::OK (0) means that data is available on the socket.
  // - Result < net::OK (0) means that an error occurred.
  // - Result > net::OK (0) should not happen.
  //
  // Otherwise:
  // - Result = net::OK (0) means that the end of file is reached.
  // - Result < net::OK (0) means that an error occurred.
  // - Result > net::OK (0) means that  `result` bytes were read in
  //   `pending_receive_buffer`.
  bool is_error_or_end_of_file;

  if (read_if_ready_pending_) {
    CHECK_GE(net::OK, result);
    CHECK(!pending_receive_buffer);
    read_if_ready_pending_ = false;
    is_error_or_end_of_file = result < net::OK;
  } else {
    CHECK(pending_receive_buffer);
    receive_stream_ = pending_receive_buffer->Complete(
        /* num_bytes=*/result >= 0 ? result : 0);
    is_error_or_end_of_file = result <= net::OK;
  }

  if (is_error_or_end_of_file) {
    if (delegate_)
      delegate_->OnNetworkReadError(result);
    ShutdownReceive();
    return;
  }

  ReceiveMore();
}

void SocketDataPump::ShutdownReceive() {
  CHECK(!receive_is_shutdown_);

  receive_is_shutdown_ = true;
  receive_stream_watcher_.Cancel();
  receive_stream_close_watcher_.Cancel();
  receive_stream_.reset();
  if (read_if_ready_pending_) {
    int result = socket_->CancelReadIfReady();
    DCHECK_EQ(net::OK, result);
    read_if_ready_pending_ = false;
  }
  MaybeNotifyDelegate();
}

void SocketDataPump::SendMore() {
  DCHECK(send_stream_.is_valid());
  DCHECK(!pending_send_buffer_);

  MojoResult result =
      MojoToNetPendingBuffer::BeginRead(&send_stream_, &pending_send_buffer_);
  if (result == MOJO_RESULT_SHOULD_WAIT) {
    send_stream_watcher_.ArmOrNotify();
    return;
  }
  if (result != MOJO_RESULT_OK) {
    ShutdownSend();
    return;
  }
  auto buf = base::MakeRefCounted<net::WrappedIOBuffer>(*pending_send_buffer_);

  // Use WeakPtr here because |this| doesn't outlive |socket_|.
  int write_result =
      socket_->Write(buf.get(), buf->size(),
                     base::BindOnce(&SocketDataPump::OnNetworkWriteCompleted,
                                    weak_factory_.GetWeakPtr()),
                     traffic_annotation_);
  if (write_result == net::ERR_IO_PENDING)
    return;
  OnNetworkWriteCompleted(write_result);
}

void SocketDataPump::OnSendStreamReadable(MojoResult result) {
  DCHECK(!pending_send_buffer_);
  DCHECK(send_stream_.is_valid());

  if (result != MOJO_RESULT_OK) {
    ShutdownSend();
    return;
  }
  SendMore();
}

void SocketDataPump::OnNetworkWriteCompleted(int result) {
  DCHECK(pending_send_buffer_);
  DCHECK(!send_stream_.is_valid());

  // Partial write is possible.
  pending_send_buffer_->CompleteRead(result >= 0 ? result : 0);
  send_stream_ = pending_send_buffer_->ReleaseHandle();
  pending_send_buffer_ = nullptr;

  if (result <= 0) {
    if (delegate_)
      delegate_->OnNetworkWriteError(result);
    ShutdownSend();
    return;
  }
  SendMore();
}

void SocketDataPump::ShutdownSend() {
  DCHECK(send_stream_.is_valid());
  DCHECK(!pending_send_buffer_);

  send_stream_watcher_.Cancel();
  pending_send_buffer_ = nullptr;
  send_stream_.reset();
  MaybeNotifyDelegate();
}

void SocketDataPump::MaybeNotifyDelegate() {
  if (!delegate_ || send_stream_.is_valid() || !receive_is_shutdown_) {
    return;
  }
  delegate_->OnShutdown();
}

}  // namespace network
