// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/socket_data_pump.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/optional.h"
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
  DCHECK(receive_stream_.is_valid());

  uint32_t num_bytes = 0;
  scoped_refptr<NetToMojoPendingBuffer> pending_receive_buffer;
  MojoResult result = NetToMojoPendingBuffer::BeginWrite(
      &receive_stream_, &pending_receive_buffer, &num_bytes);
  if (result == MOJO_RESULT_SHOULD_WAIT) {
    receive_stream_watcher_.ArmOrNotify();
    return;
  }
  if (result != MOJO_RESULT_OK) {
    ShutdownReceive();
    return;
  }
  DCHECK(pending_receive_buffer);
  scoped_refptr<net::IOBuffer> buf =
      base::MakeRefCounted<NetToMojoIOBuffer>(pending_receive_buffer.get());
  // Use WeakPtr here because |this| doesn't outlive |socket_|.
  int read_result = socket_->ReadIfReady(
      buf.get(), base::saturated_cast<int>(num_bytes),
      base::BindRepeating(&SocketDataPump::OnNetworkReadIfReadyCompleted,
                          weak_factory_.GetWeakPtr()));
  DCHECK_NE(net::ERR_READ_IF_READY_NOT_IMPLEMENTED, read_result);
  receive_stream_ =
      pending_receive_buffer->Complete(read_result >= 0 ? read_result : 0);
  if (read_result == net::ERR_IO_PENDING) {
    read_if_ready_pending_ = true;
    receive_stream_close_watcher_.ArmOrNotify();
    return;
  }
  // Handle EOF. Has to be done here rather than in
  // OnNetworkReadIfReadyCompleted because net::OK in the sync completion case
  // means EOF, but in the async case just means the socket is ready to be read
  // from again.
  if (read_result == net::OK) {
    if (delegate_)
      delegate_->OnNetworkReadError(read_result);
    ShutdownReceive();
    return;
  }
  OnNetworkReadIfReadyCompleted(read_result);
}

void SocketDataPump::OnReceiveStreamClosed(MojoResult result) {
  ShutdownReceive();
  return;
}

void SocketDataPump::OnReceiveStreamWritable(MojoResult result) {
  DCHECK(receive_stream_.is_valid());

  if (result != MOJO_RESULT_OK) {
    ShutdownReceive();
    return;
  }
  ReceiveMore();
}

void SocketDataPump::OnNetworkReadIfReadyCompleted(int result) {
  // This method is called either on ReadIfReady sync completion, except in the
  // EOF case, or on async completion. In the sync case, result is < 0 on error,
  // or > 0 on success. In the async case, result is < 0 on error, or 0 if we
  // should try to read from the socket again (And possibly get any of more
  // data, an EOF, or an error).
  DCHECK(receive_stream_.is_valid());
  if (read_if_ready_pending_) {
    DCHECK_GE(net::OK, result);
    read_if_ready_pending_ = false;
  }

  if (result < 0) {
    if (delegate_)
      delegate_->OnNetworkReadError(result);
    ShutdownReceive();
    return;
  }
  ReceiveMore();
}

void SocketDataPump::ShutdownReceive() {
  DCHECK(receive_stream_.is_valid());

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

  uint32_t num_bytes = 0;
  MojoResult result = MojoToNetPendingBuffer::BeginRead(
      &send_stream_, &pending_send_buffer_, &num_bytes);
  if (result == MOJO_RESULT_SHOULD_WAIT) {
    send_stream_watcher_.ArmOrNotify();
    return;
  }
  if (result != MOJO_RESULT_OK) {
    ShutdownSend();
    return;
  }
  DCHECK_EQ(MOJO_RESULT_OK, result);
  DCHECK(pending_send_buffer_);
  scoped_refptr<net::IOBuffer> buf = base::MakeRefCounted<net::WrappedIOBuffer>(
      pending_send_buffer_->buffer());
  // Use WeakPtr here because |this| doesn't outlive |socket_|.
  int write_result = socket_->Write(
      buf.get(), static_cast<int>(num_bytes),
      base::BindRepeating(&SocketDataPump::OnNetworkWriteCompleted,
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
  if (!delegate_ || send_stream_.is_valid() || receive_stream_.is_valid())
    return;
  delegate_->OnShutdown();
}

}  // namespace network
