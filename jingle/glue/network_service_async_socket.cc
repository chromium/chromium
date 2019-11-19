// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/glue/network_service_async_socket.h"

#include <stddef.h>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <utility>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/host_port_pair.h"
#include "net/base/io_buffer.h"

namespace jingle_glue {

NetworkServiceAsyncSocket::NetworkServiceAsyncSocket(
    GetProxyResolvingSocketFactoryCallback get_socket_factory_callback,
    bool use_fake_tls_handshake,
    size_t read_buf_size,
    size_t write_buf_size,
    const net::NetworkTrafficAnnotationTag& traffic_annotation)
    : get_socket_factory_callback_(get_socket_factory_callback),
      use_fake_tls_handshake_(use_fake_tls_handshake),
      state_(STATE_CLOSED),
      error_(ERROR_NONE),
      net_error_(net::OK),
      read_state_(IDLE),
      read_buf_(read_buf_size),
      read_start_(0U),
      read_end_(0U),
      saw_error_on_read_pipe_(false),
      saw_error_on_write_pipe_(false),
      saw_read_error_on_socket_observer_pipe_(net::ERR_IO_PENDING),
      saw_write_error_on_socket_observer_pipe_(net::ERR_IO_PENDING),
      write_state_(IDLE),
      write_buf_(write_buf_size),
      write_end_(0U),
      traffic_annotation_(traffic_annotation) {
  DCHECK(get_socket_factory_callback_);
  DCHECK_GT(read_buf_size, 0U);
  DCHECK_GT(write_buf_size, 0U);
}

NetworkServiceAsyncSocket::~NetworkServiceAsyncSocket() {}

NetworkServiceAsyncSocket::State NetworkServiceAsyncSocket::state() {
  return state_;
}

NetworkServiceAsyncSocket::Error NetworkServiceAsyncSocket::error() {
  return error_;
}

int NetworkServiceAsyncSocket::GetError() {
  return net_error_;
}

bool NetworkServiceAsyncSocket::IsOpen() const {
  return (state_ == STATE_OPEN) || (state_ == STATE_TLS_OPEN);
}

void NetworkServiceAsyncSocket::DoNonNetError(Error error) {
  DCHECK_NE(error, ERROR_NONE);
  DCHECK_NE(error, ERROR_WINSOCK);
  error_ = error;
  net_error_ = net::OK;
}

void NetworkServiceAsyncSocket::DoNetError(net::Error net_error) {
  error_ = ERROR_WINSOCK;
  net_error_ = net_error;
}

void NetworkServiceAsyncSocket::DoNetErrorFromStatus(int status) {
  DCHECK_LT(status, net::OK);
  DoNetError(static_cast<net::Error>(status));
}

void NetworkServiceAsyncSocket::ProcessSocketObserverError() {
  if (saw_read_error_on_socket_observer_pipe_ == net::ERR_IO_PENDING &&
      saw_write_error_on_socket_observer_pipe_ == net::ERR_IO_PENDING) {
    // Haven't seen an error, and the socket observer pipe got broken.
    // This shouldn't normally happen, but as the trust level of network service
    // is lower than of browser process, it needs to be handled.
    DoNetError(net::ERR_FAILED);
    DoClose();
  }
  // In case an error came in on the socket observer pipe, it will
  // get handled at time of read data pipe's closing.
}

void NetworkServiceAsyncSocket::OnReadError(int32_t net_error) {
  // Ignore redundant error messages.
  if (saw_read_error_on_socket_observer_pipe_ != net::ERR_IO_PENDING)
    return;

  // Sanitize any invalid error code,
  if (net_error > 0 || net_error == net::ERR_IO_PENDING)
    net_error = net::ERR_FAILED;
  if (saw_error_on_read_pipe_) {
    // Read pipe closure got delivered first, and so with the socket observer
    // notification here, both pipes got fully handled.
    ReportReadError(net_error);
  } else {
    // Read error notification on socket observer pipe got delivered first;
    // save error code for read pipe closure to deliver.
    saw_read_error_on_socket_observer_pipe_ = net_error;
  }
}

void NetworkServiceAsyncSocket::OnWriteError(int32_t net_error) {
  // Ignore redundant error messages.
  if (saw_write_error_on_socket_observer_pipe_ != net::ERR_IO_PENDING)
    return;

  // Sanitize any invalid error code,
  if (net_error >= 0 || net_error == net::ERR_IO_PENDING)
    net_error = net::ERR_FAILED;
  if (saw_error_on_write_pipe_) {
    // Write pipe closure got delivered first, and so with the socket observer
    // notification here, both pipes got fully handled.
    DoNetErrorFromStatus(net_error);
    DoClose();
  } else {
    // Write error notification on socket observer pipe got delivered first;
    // save error code for write pipe closure to deliver.
    saw_write_error_on_socket_observer_pipe_ = net_error;
  }
}

// STATE_CLOSED -> STATE_CONNECTING

bool NetworkServiceAsyncSocket::Connect(const net::HostPortPair& address) {
  if (state_ != STATE_CLOSED) {
    LOG(DFATAL) << "Connect() called on non-closed socket";
    DoNonNetError(ERROR_WRONGSTATE);
    return false;
  }
  if (address.host().empty() || address.port() == 0) {
    DoNonNetError(ERROR_DNS);
    return false;
  }

  DCHECK_EQ(state_, jingle_xmpp::AsyncSocket::STATE_CLOSED);
  DCHECK_EQ(read_state_, IDLE);
  DCHECK_EQ(write_state_, IDLE);

  state_ = STATE_CONNECTING;

  get_socket_factory_callback_.Run(
      socket_factory_.BindNewPipeAndPassReceiver());

  mojo::PendingRemote<network::mojom::SocketObserver> socket_observer;
  auto socket_observer_receiver =
      socket_observer.InitWithNewPipeAndPassReceiver();
  network::mojom::ProxyResolvingSocketOptionsPtr options =
      network::mojom::ProxyResolvingSocketOptions::New();
  options->use_tls = false;
  options->fake_tls_handshake = use_fake_tls_handshake_;
  socket_factory_->CreateProxyResolvingSocket(
      GURL("https://" + address.ToString()), std::move(options),
      net::MutableNetworkTrafficAnnotationTag(traffic_annotation_),
      socket_.BindNewPipeAndPassReceiver(), std::move(socket_observer),
      base::BindOnce(&NetworkServiceAsyncSocket::ProcessConnectDone,
                     base::Unretained(this),
                     std::move(socket_observer_receiver)));
  return true;
}

// STATE_CONNECTING -> STATE_OPEN
// read_state_ == IDLE -> read_state_ == WAITING (via WatchForReadReady())

void NetworkServiceAsyncSocket::ProcessConnectDone(
    mojo::PendingReceiver<network::mojom::SocketObserver>
        socket_observer_receiver,
    int status,
    const base::Optional<net::IPEndPoint>& local_addr,
    const base::Optional<net::IPEndPoint>& peer_addr,
    mojo::ScopedDataPipeConsumerHandle receive_stream,
    mojo::ScopedDataPipeProducerHandle send_stream) {
  DCHECK_NE(status, net::ERR_IO_PENDING);
  DCHECK_EQ(read_state_, IDLE);
  DCHECK_EQ(write_state_, IDLE);
  DCHECK_EQ(state_, STATE_CONNECTING);
  if (status != net::OK) {
    DoNetErrorFromStatus(status);
    DoClose();
    return;
  }
  state_ = STATE_OPEN;
  ConnectPipes(std::move(receive_stream), std::move(send_stream));
  BindSocketObserver(std::move(socket_observer_receiver));

  WatchForReadReady();
  // Write buffer should be empty.
  DCHECK_EQ(write_end_, 0U);
  SignalConnected();
}

// read_state_ == IDLE -> read_state_ == WAITING

void NetworkServiceAsyncSocket::WatchForReadReady() {
  // Note that this never transitions to ProcessReadReady immediately; which
  // avoids potentially error-prone synchronous notifications from within
  // methods like Connect() and Read().
  DCHECK(IsOpen());
  DCHECK_EQ(read_state_, IDLE);
  DCHECK_EQ(read_start_, 0U);
  DCHECK_EQ(read_end_, 0U);

  // Once we call Read(), we cannot call StartTls() until the read
  // finishes.  This is okay, as StartTls() is called only from a read
  // handler (i.e., after a read finishes and before another read is
  // done).
  read_state_ = WAITING;
  read_watcher_->ArmOrNotify();
}

// read_state_ == WAITING -> read_state_ == IDLE

void NetworkServiceAsyncSocket::ProcessReadReady(
    MojoResult result,
    const mojo::HandleSignalsState& state) {
  DCHECK(IsOpen());
  DCHECK_EQ(read_state_, WAITING);
  DCHECK_EQ(read_start_, 0U);
  DCHECK_EQ(read_end_, 0U);
  read_state_ = IDLE;

  uint32_t num_bytes = read_buf_.size();
  if (result == MOJO_RESULT_OK && !state.peer_closed()) {
    result = read_pipe_->ReadData(read_buf_.data(), &num_bytes,
                                  MOJO_READ_DATA_FLAG_NONE);
    if (result == MOJO_RESULT_SHOULD_WAIT) {
      WatchForReadReady();
      return;
    }
  }

  if (result != MOJO_RESULT_OK || !num_bytes || state.peer_closed()) {
    // The pipe is closed on any error, or EOF.
    if (saw_read_error_on_socket_observer_pipe_ != net::ERR_IO_PENDING) {
      // Already saw socket observer's notification, report result.
      ReportReadError(saw_read_error_on_socket_observer_pipe_);
    } else {
      // This got delivered before the error code from socket observer, let it
      // know it's responsible for reporting the error/EOF.
      saw_error_on_read_pipe_ = true;
    }
    return;
  }

  read_end_ = num_bytes;
  SignalRead();
}

void NetworkServiceAsyncSocket::ReportReadError(int net_error) {
  if (net_error == 0) {
    // Other side closed the connection.
    error_ = ERROR_NONE;
    net_error_ = net::OK;
  } else {
    DoNetErrorFromStatus(net_error);
  }
  DoClose();
}

// (maybe) read_state_ == IDLE -> read_state_ == WAITING (via
// WatchForReadReady())

bool NetworkServiceAsyncSocket::Read(char* data, size_t len, size_t* len_read) {
  if (!IsOpen() && (state_ != STATE_TLS_CONNECTING)) {
    // Read() may be called on a closed socket if a previous read
    // causes a socket close (e.g., client sends wrong password and
    // server terminates connection).
    //
    // TODO(akalin): Fix handling of this on the libjingle side.
    if (state_ != STATE_CLOSED) {
      LOG(DFATAL) << "Read() called on non-open non-tls-connecting socket";
    }
    DoNonNetError(ERROR_WRONGSTATE);
    return false;
  }
  DCHECK_LE(read_start_, read_end_);
  if ((state_ == STATE_TLS_CONNECTING) || read_end_ == 0U) {
    if (state_ == STATE_TLS_CONNECTING) {
      DCHECK_EQ(read_state_, IDLE);
      DCHECK_EQ(read_end_, 0U);
    } else {
      DCHECK_NE(read_state_, IDLE);
    }
    *len_read = 0;
    return true;
  }
  DCHECK_EQ(read_state_, IDLE);
  *len_read = std::min(len, read_end_ - read_start_);
  DCHECK_GT(*len_read, 0U);
  std::memcpy(data, read_buf_.data() + read_start_, *len_read);
  read_start_ += *len_read;
  if (read_start_ == read_end_) {
    read_start_ = 0U;
    read_end_ = 0U;
    WatchForReadReady();
  }
  return true;
}

// (maybe) write_state_ == IDLE -> write_state_ == WAITING (via
// WatchForWriteReady())

bool NetworkServiceAsyncSocket::Write(const char* data, size_t len) {
  if (!IsOpen() && (state_ != STATE_TLS_CONNECTING)) {
    LOG(DFATAL) << "Write() called on non-open non-tls-connecting socket";
    DoNonNetError(ERROR_WRONGSTATE);
    return false;
  }
  // TODO(akalin): Avoid this check by modifying the interface to have
  // a "ready for writing" signal.
  if ((static_cast<size_t>(write_buf_.size()) - write_end_) < len) {
    LOG(DFATAL) << "queueing " << len << " bytes would exceed the "
                << "max write buffer size = " << write_buf_.size() << " by "
                << (len - write_buf_.size()) << " bytes";
    DoNetError(net::ERR_INSUFFICIENT_RESOURCES);
    return false;
  }
  std::memcpy(write_buf_.data() + write_end_, data, len);
  write_end_ += len;
  // If we're TLS-connecting, the write buffer will get flushed once
  // the TLS-connect finishes.  Otherwise, start writing if we're not
  // already writing and we have something to write.
  if ((state_ != STATE_TLS_CONNECTING) && (write_state_ == IDLE) &&
      (write_end_ > 0U)) {
    WatchForWriteReady();
  }
  return true;
}

// write_state_ == IDLE -> write_state_ == WAITING

void NetworkServiceAsyncSocket::WatchForWriteReady() {
  // Note that this never transitions to ProcessWriteReady immediately; which
  // avoids potentially error-prone synchronous notifications from within
  // methods like Write().
  DCHECK(IsOpen());
  DCHECK_EQ(write_state_, IDLE);
  DCHECK_GT(write_end_, 0U);

  // Once we call Write(), we cannot call StartTls() until the write
  // finishes.  This is okay, as StartTls() is called only after we
  // have received a reply to a message we sent to the server and
  // before we send the next message.
  write_state_ = WAITING;
  write_watcher_->ArmOrNotify();
}

// write_state_ == WAITING -> write_state_ == IDLE or WAITING (the
// latter via WatchForWriteReady())

void NetworkServiceAsyncSocket::ProcessWriteReady(
    MojoResult result,
    const mojo::HandleSignalsState& state) {
  DCHECK(IsOpen());
  DCHECK_EQ(write_state_, WAITING);
  DCHECK_GT(write_end_, 0U);
  write_state_ = IDLE;

  // Write errors are handled in ProcessWriteClosed.
  uint32_t written = write_end_;
  if (result == MOJO_RESULT_OK) {
    result = write_pipe_->WriteData(write_buf_.data(), &written,
                                    MOJO_WRITE_DATA_FLAG_NONE);
  }

  if (result == MOJO_RESULT_SHOULD_WAIT) {
    WatchForWriteReady();
    return;
  }

  if (result != MOJO_RESULT_OK) {
    DCHECK(socket_observer_receiver_.is_bound());
    // Unlike with reads, as the pipe close notifier for writes is independent
    // and always armed, it can take care of all the errors.
    return;
  }

  if (written > write_end_) {
    LOG(DFATAL) << "bytes written = " << written
                << " exceeds bytes requested = " << write_end_;
    DoNetError(net::ERR_UNEXPECTED);
    DoClose();
    return;
  }
  // TODO(akalin): Figure out a better way to do this; perhaps a queue
  // of DrainableIOBuffers.  This'll also allow us to not have an
  // artificial buffer size limit.
  std::memmove(write_buf_.data(), write_buf_.data() + written,
               write_end_ - written);
  write_end_ -= written;
  if (write_end_ > 0U) {
    WatchForWriteReady();
  }
}

void NetworkServiceAsyncSocket::ProcessWriteClosed(
    MojoResult result,
    const mojo::HandleSignalsState& state) {
  DCHECK(state.peer_closed());

  // The pipe is closed on any error, or EOF.
  if (saw_write_error_on_socket_observer_pipe_ != net::ERR_IO_PENDING) {
    // Already saw socket observer's notification, report result.
    DoNetErrorFromStatus(saw_write_error_on_socket_observer_pipe_);
    DoClose();
  } else {
    // This got delivered before the error code from socket observer, let it
    // know it's responsible for reporting the error/EOF.
    saw_error_on_write_pipe_ = true;
  }
}

// * -> STATE_CLOSED

bool NetworkServiceAsyncSocket::Close() {
  DoClose();
  return true;
}

// (not STATE_CLOSED) -> STATE_CLOSED

void NetworkServiceAsyncSocket::DoClose() {
  // As this closes all the mojo pipes and destroys all the watchers it also
  // cancels all pending async operations.
  read_state_ = IDLE;
  read_start_ = 0U;
  read_end_ = 0U;
  read_pipe_.reset();
  read_watcher_.reset();
  saw_error_on_read_pipe_ = false;
  saw_error_on_write_pipe_ = false;
  saw_read_error_on_socket_observer_pipe_ = net::ERR_IO_PENDING;
  saw_write_error_on_socket_observer_pipe_ = net::ERR_IO_PENDING;
  write_state_ = IDLE;
  write_end_ = 0U;
  write_pipe_.reset();
  write_watcher_.reset();
  write_close_watcher_.reset();

  socket_.reset();
  tls_socket_.reset();
  socket_observer_receiver_.reset();
  socket_factory_.reset();

  if (state_ != STATE_CLOSED) {
    state_ = STATE_CLOSED;
    SignalClosed();
  }
  // Reset error variables after SignalClosed() so slots connected
  // to it can read it.
  error_ = ERROR_NONE;
  net_error_ = net::OK;
}

// STATE_OPEN -> STATE_TLS_CONNECTING

bool NetworkServiceAsyncSocket::StartTls(const std::string& domain_name) {
  DCHECK_EQ(IDLE, write_state_);
  if (state_ != STATE_OPEN) {
    LOG(DFATAL) << "StartTls() called in wrong state";
    DoNonNetError(ERROR_WRONGSTATE);
    return false;
  }

  state_ = STATE_TLS_CONNECTING;
  read_state_ = IDLE;
  read_start_ = 0U;
  read_end_ = 0U;
  DCHECK_EQ(write_end_, 0U);

  read_watcher_ = nullptr;
  read_pipe_.reset();
  write_watcher_ = nullptr;
  write_close_watcher_ = nullptr;
  write_pipe_.reset();
  socket_observer_receiver_.reset();
  mojo::PendingRemote<network::mojom::SocketObserver> socket_observer;
  auto socket_observer_receiver =
      socket_observer.InitWithNewPipeAndPassReceiver();

  socket_->UpgradeToTLS(
      net::HostPortPair(domain_name, 443),
      net::MutableNetworkTrafficAnnotationTag(traffic_annotation_),
      tls_socket_.BindNewPipeAndPassReceiver(), std::move(socket_observer),
      base::BindOnce(&NetworkServiceAsyncSocket::ProcessSSLConnectDone,
                     base::Unretained(this),
                     std::move(socket_observer_receiver)));
  return true;
}

// STATE_TLS_CONNECTING -> STATE_TLS_OPEN
// read_state_ == IDLE -> read_state_ == WAITING (via WatchForReadReady())
// (maybe) write_state_ == IDLE -> write_state_ == WAITING (via
// WatchForWriteReady())

void NetworkServiceAsyncSocket::ProcessSSLConnectDone(
    mojo::PendingReceiver<network::mojom::SocketObserver>
        socket_observer_receiver,
    int status,
    mojo::ScopedDataPipeConsumerHandle receive_stream,
    mojo::ScopedDataPipeProducerHandle send_stream) {
  DCHECK_NE(status, net::ERR_IO_PENDING);
  DCHECK_EQ(state_, STATE_TLS_CONNECTING);
  DCHECK_EQ(read_state_, IDLE);
  DCHECK_EQ(read_start_, 0U);
  DCHECK_EQ(read_end_, 0U);
  DCHECK_EQ(write_state_, IDLE);
  if (status != net::OK) {
    DoNetErrorFromStatus(status);
    DoClose();
    return;
  }
  state_ = STATE_TLS_OPEN;
  ConnectPipes(std::move(receive_stream), std::move(send_stream));
  BindSocketObserver(std::move(socket_observer_receiver));

  WatchForReadReady();
  if (write_end_ > 0U) {
    WatchForWriteReady();
  }
  SignalSSLConnected();
}

void NetworkServiceAsyncSocket::ConnectPipes(
    mojo::ScopedDataPipeConsumerHandle receive_stream,
    mojo::ScopedDataPipeProducerHandle send_stream) {
  read_pipe_ = std::move(receive_stream);
  read_watcher_ = std::make_unique<mojo::SimpleWatcher>(
      FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL);
  read_watcher_->Watch(
      read_pipe_.get(),
      MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
      base::BindRepeating(&NetworkServiceAsyncSocket::ProcessReadReady,
                          base::Unretained(this)));

  write_pipe_ = std::move(send_stream);
  write_watcher_ = std::make_unique<mojo::SimpleWatcher>(
      FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL);
  write_watcher_->Watch(
      write_pipe_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
      MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
      base::BindRepeating(&NetworkServiceAsyncSocket::ProcessWriteReady,
                          base::Unretained(this)));

  // Write pipe close gets a separate  watcher to look for signs of trouble
  // even when no write is pending. (Read doesn't need one since reads are
  // always watched for).
  write_close_watcher_ = std::make_unique<mojo::SimpleWatcher>(
      FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL);
  write_close_watcher_->Watch(
      write_pipe_.get(), MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
      base::BindRepeating(&NetworkServiceAsyncSocket::ProcessWriteClosed,
                          base::Unretained(this)));
  write_close_watcher_->ArmOrNotify();
}

void NetworkServiceAsyncSocket::BindSocketObserver(
    mojo::PendingReceiver<network::mojom::SocketObserver>
        socket_observer_receiver) {
  socket_observer_receiver_.Bind(std::move(socket_observer_receiver));
  socket_observer_receiver_.set_disconnect_handler(
      base::BindOnce(&NetworkServiceAsyncSocket::ProcessSocketObserverError,
                     base::Unretained(this)));
}

}  // namespace jingle_glue
