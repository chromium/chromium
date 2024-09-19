// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/socket/tcp_socket.h"

#include <errno.h>
#include <mstcpip.h>

#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/win/windows_version.h"
#include "net/base/address_list.h"
#include "net/base/features.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/network_activity_monitor.h"
#include "net/base/network_change_notifier.h"
#include "net/base/sockaddr_storage.h"
#include "net/base/winsock_init.h"
#include "net/base/winsock_util.h"
#include "net/log/net_log.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source.h"
#include "net/log/net_log_source_type.h"
#include "net/log/net_log_values.h"
#include "net/socket/socket_descriptor.h"
#include "net/socket/socket_net_log_params.h"
#include "net/socket/socket_options.h"
#include "net/socket/socket_tag.h"
#include "net/socket/tcp_socket_io_completion_port_win.h"
#include "net/socket/tcp_socket_win.h"

namespace net {

namespace {

const int kTCPKeepAliveSeconds = 45;

// Disable Nagle.
// Enable TCP Keep-Alive to prevent NAT routers from timing out TCP
// connections. See http://crbug.com/27400 for details.
bool SetTCPKeepAlive(SOCKET socket, BOOL enable, int delay_secs) {
  unsigned delay = delay_secs * 1000;
  struct tcp_keepalive keepalive_vals = {
      enable ? 1u : 0u,  // TCP keep-alive on.
      delay,  // Delay seconds before sending first TCP keep-alive packet.
      delay,  // Delay seconds between sending TCP keep-alive packets.
  };
  DWORD bytes_returned = 0xABAB;
  int rv = WSAIoctl(socket, SIO_KEEPALIVE_VALS, &keepalive_vals,
                    sizeof(keepalive_vals), nullptr, 0, &bytes_returned,
                    nullptr, nullptr);
  int os_error = WSAGetLastError();
  DCHECK(!rv) << "Could not enable TCP Keep-Alive for socket: " << socket
              << " [error: " << os_error << "].";

  // Disregard any failure in disabling nagle or enabling TCP Keep-Alive.
  return rv == 0;
}

int MapConnectError(int os_error) {
  switch (os_error) {
    // connect fails with WSAEACCES when Windows Firewall blocks the
    // connection.
    case WSAEACCES:
      return ERR_NETWORK_ACCESS_DENIED;
    case WSAETIMEDOUT:
      return ERR_CONNECTION_TIMED_OUT;
    default: {
      int net_error = MapSystemError(os_error);
      if (net_error == ERR_FAILED)
        return ERR_CONNECTION_FAILED;  // More specific than ERR_FAILED.

      // Give a more specific error when the user is offline.
      if (net_error == ERR_ADDRESS_UNREACHABLE &&
          NetworkChangeNotifier::IsOffline()) {
        return ERR_INTERNET_DISCONNECTED;
      }

      return net_error;
    }
  }
}

bool SetNonBlockingAndGetError(int fd, int* os_error) {
  bool ret = base::SetNonBlocking(fd);
  *os_error = WSAGetLastError();

  return ret;
}

}  // namespace

//-----------------------------------------------------------------------------

class NET_EXPORT TCPSocketDefaultWin : public TCPSocketWin {
 public:
  TCPSocketDefaultWin(
      std::unique_ptr<SocketPerformanceWatcher> socket_performance_watcher,
      NetLog* net_log,
      const NetLogSource& source);

  TCPSocketDefaultWin(
      std::unique_ptr<SocketPerformanceWatcher> socket_performance_watcher,
      NetLogWithSource net_log_source);

  ~TCPSocketDefaultWin() override;

  // TCPSocketWin:
  int Read(IOBuffer* buf,
           int buf_len,
           CompletionOnceCallback callback) override;
  int ReadIfReady(IOBuffer* buf,
                  int buf_len,
                  CompletionOnceCallback callback) override;
  int CancelReadIfReady() override;
  int Write(IOBuffer* buf,
            int buf_len,
            CompletionOnceCallback callback,
            const NetworkTrafficAnnotationTag& traffic_annotation) override;

 protected:
  // TCPSocketWin:
  scoped_refptr<Core> CreateCore() override;
  bool HasPendingRead() const override;
  void OnClosed() override;

 private:
  class CoreImpl;

  void RetryRead(int rv);
  void DidCompleteWrite();
  void DidSignalRead();

  CoreImpl& GetCoreImpl();

  // External callback; called when read is complete.
  CompletionOnceCallback read_callback_;

  // Non-null if a ReadIfReady() is to be completed asynchronously. This is an
  // external callback if user used ReadIfReady() instead of Read(), but a
  // wrapped callback on top of RetryRead() if Read() is used.
  CompletionOnceCallback read_if_ready_callback_;

  // External callback; called when write is complete.
  CompletionOnceCallback write_callback_;
};

class TCPSocketDefaultWin::CoreImpl : public TCPSocketWin::Core {
 public:
  explicit CoreImpl(TCPSocketDefaultWin* socket);

  CoreImpl(const CoreImpl&) = delete;
  CoreImpl& operator=(const CoreImpl&) = delete;

  // Start watching for the end of a read or write operation.
  void WatchForRead();
  void WatchForWrite();

  // Stops watching for read.
  void StopWatchingForRead();

  // TCPSocketWin::Core:
  void Detach() override;
  HANDLE GetConnectEvent() override;
  void WatchForConnect() override;

  // Event handle for monitoring connect and read events through WSAEventSelect.
  HANDLE read_event_;

  // OVERLAPPED variable for overlapped writes.
  // TODO(mmenke): Can writes be switched to WSAEventSelect as well? That would
  // allow removing this class. The only concern is whether that would have a
  // negative perf impact.
  OVERLAPPED write_overlapped_;

  // The buffers used in Read() and Write().
  scoped_refptr<IOBuffer> read_iobuffer_;
  scoped_refptr<IOBuffer> write_iobuffer_;
  int read_buffer_length_ = 0;
  int write_buffer_length_ = 0;

  bool non_blocking_reads_initialized_ = false;

 private:
  class ReadDelegate : public base::win::ObjectWatcher::Delegate {
   public:
    explicit ReadDelegate(CoreImpl* core) : core_(core) {}
    ~ReadDelegate() override = default;

    // base::ObjectWatcher::Delegate methods:
    void OnObjectSignaled(HANDLE object) override;

   private:
    const raw_ptr<CoreImpl> core_;
  };

  class WriteDelegate : public base::win::ObjectWatcher::Delegate {
   public:
    explicit WriteDelegate(CoreImpl* core) : core_(core) {}
    ~WriteDelegate() override = default;

    // base::ObjectWatcher::Delegate methods:
    void OnObjectSignaled(HANDLE object) override;

   private:
    const raw_ptr<CoreImpl> core_;
  };

  ~CoreImpl() override;

  // The socket that created this object.
  raw_ptr<TCPSocketDefaultWin> socket_;

  // |reader_| handles the signals from |read_watcher_|.
  ReadDelegate reader_;
  // |writer_| handles the signals from |write_watcher_|.
  WriteDelegate writer_;

  // |read_watcher_| watches for events from Connect() and Read().
  base::win::ObjectWatcher read_watcher_;
  // |write_watcher_| watches for events from Write();
  base::win::ObjectWatcher write_watcher_;
};

TCPSocketWin::Core::Core() = default;
TCPSocketWin::Core::~Core() = default;

TCPSocketDefaultWin::CoreImpl::CoreImpl(TCPSocketDefaultWin* socket)
    : read_event_(WSACreateEvent()),
      socket_(socket),
      reader_(this),
      writer_(this) {
  memset(&write_overlapped_, 0, sizeof(write_overlapped_));
  write_overlapped_.hEvent = WSACreateEvent();
}

TCPSocketDefaultWin::CoreImpl::~CoreImpl() {
  // Detach should already have been called.
  DCHECK(!socket_);

  // Stop the write watcher.  The read watcher should already have been stopped
  // in Detach().
  write_watcher_.StopWatching();
  WSACloseEvent(write_overlapped_.hEvent);
  memset(&write_overlapped_, 0xaf, sizeof(write_overlapped_));
}

void TCPSocketDefaultWin::CoreImpl::WatchForRead() {
  // Reads use WSAEventSelect, which closesocket() cancels so unlike writes,
  // there's no need to increment the reference count here.
  read_watcher_.StartWatchingOnce(read_event_, &reader_);
}

void TCPSocketDefaultWin::CoreImpl::WatchForWrite() {
  // We grab an extra reference because there is an IO operation in progress.
  // Balanced in WriteDelegate::OnObjectSignaled().
  AddRef();
  write_watcher_.StartWatchingOnce(write_overlapped_.hEvent, &writer_);
}

void TCPSocketDefaultWin::CoreImpl::StopWatchingForRead() {
  DCHECK(!socket_->connect_callback_);

  read_watcher_.StopWatching();
}

void TCPSocketDefaultWin::CoreImpl::Detach() {
  // Stop watching the read watcher. A read won't be signalled after the Detach
  // call, since the socket has been closed, but it's possible the event was
  // signalled when the socket was closed, but hasn't been handled yet, so need
  // to stop watching now to avoid trying to handle the event. See
  // https://crbug.com/831149
  read_watcher_.StopWatching();
  WSACloseEvent(read_event_);

  socket_ = nullptr;
}

HANDLE TCPSocketDefaultWin::CoreImpl::GetConnectEvent() {
  // `read_event_` is used to watch for connect.
  return read_event_;
}

void TCPSocketDefaultWin::CoreImpl::WatchForConnect() {
  // `read_event_` is used to watch for connect.
  WatchForRead();
}

void TCPSocketDefaultWin::CoreImpl::ReadDelegate::OnObjectSignaled(
    HANDLE object) {
  DCHECK_EQ(object, core_->read_event_);
  DCHECK(core_->socket_);
  if (core_->socket_->connect_callback_) {
    core_->socket_->DidCompleteConnect();
  } else {
    core_->socket_->DidSignalRead();
  }
}

void TCPSocketDefaultWin::CoreImpl::WriteDelegate::OnObjectSignaled(
    HANDLE object) {
  DCHECK_EQ(object, core_->write_overlapped_.hEvent);
  if (core_->socket_)
    core_->socket_->DidCompleteWrite();

  // Matches the AddRef() in WatchForWrite().
  core_->Release();
}

//-----------------------------------------------------------------------------

// static
std::unique_ptr<TCPSocketWin> TCPSocketWin::Create(
    std::unique_ptr<SocketPerformanceWatcher> socket_performance_watcher,
    NetLog* net_log,
    const NetLogSource& source) {
  if (base::FeatureList::IsEnabled(features::kTcpSocketIoCompletionPortWin)) {
    return std::make_unique<TcpSocketIoCompletionPortWin>(
        std::move(socket_performance_watcher), net_log, source);
  }
  return std::make_unique<TCPSocketDefaultWin>(
      std::move(socket_performance_watcher), net_log, source);
}

// static
std::unique_ptr<TCPSocketWin> TCPSocketWin::Create(
    std::unique_ptr<SocketPerformanceWatcher> socket_performance_watcher,
    NetLogWithSource net_log_source) {
  if (base::FeatureList::IsEnabled(features::kTcpSocketIoCompletionPortWin)) {
    return std::make_unique<TcpSocketIoCompletionPortWin>(
        std::move(socket_performance_watcher), net_log_source);
  }
  return std::make_unique<TCPSocketDefaultWin>(
      std::move(socket_performance_watcher), std::move(net_log_source));
}

TCPSocketWin::TCPSocketWin(
    std::unique_ptr<SocketPerformanceWatcher> socket_performance_watcher,
    net::NetLog* net_log,
    const net::NetLogSource& source)
    : socket_(INVALID_SOCKET),
      socket_performance_watcher_(std::move(socket_performance_watcher)),
      accept_event_(WSA_INVALID_EVENT),
      net_log_(NetLogWithSource::Make(net_log, NetLogSourceType::SOCKET)) {
  net_log_.BeginEventReferencingSource(NetLogEventType::SOCKET_ALIVE, source);
  EnsureWinsockInit();
}

TCPSocketWin::TCPSocketWin(
    std::unique_ptr<SocketPerformanceWatcher> socket_performance_watcher,
    NetLogWithSource net_log_source)
    : socket_(INVALID_SOCKET),
      socket_performance_watcher_(std::move(socket_performance_watcher)),
      accept_event_(WSA_INVALID_EVENT),
      net_log_(std::move(net_log_source)) {
  net_log_.BeginEvent(NetLogEventType::SOCKET_ALIVE);
  EnsureWinsockInit();
}

TCPSocketWin::~TCPSocketWin() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // The subclass must call `Close`. See comment in header file.
  CHECK(!core_);

  net_log_.EndEvent(NetLogEventType::SOCKET_ALIVE);
}

int TCPSocketWin::Open(AddressFamily family) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_EQ(socket_, INVALID_SOCKET);

  socket_ = CreatePlatformSocket(ConvertAddressFamily(family), SOCK_STREAM,
                                 IPPROTO_TCP);
  int os_error = WSAGetLastError();
  if (socket_ == INVALID_SOCKET) {
    PLOG(ERROR) << "CreatePlatformSocket() returned an error";
    return MapSystemError(os_error);
  }

  if (!SetNonBlockingAndGetError(socket_, &os_error)) {
    int result = MapSystemError(os_error);
    Close();
    return result;
  }

  return OK;
}

int TCPSocketWin::AdoptConnectedSocket(SocketDescriptor socket,
                                       const IPEndPoint& peer_address) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_EQ(socket_, INVALID_SOCKET);
  DCHECK(!core_.get());

  socket_ = socket;

  int os_error;
  if (!SetNonBlockingAndGetError(socket_, &os_error)) {
    int result = MapSystemError(os_error);
    Close();
    return result;
  }

  core_ = CreateCore();
  peer_address_ = std::make_unique<IPEndPoint>(peer_address);

  return OK;
}

int TCPSocketWin::AdoptUnconnectedSocket(SocketDescriptor socket) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_EQ(socket_, INVALID_SOCKET);

  socket_ = socket;

  int os_error;
  if (!SetNonBlockingAndGetError(socket_, &os_error)) {
    int result = MapSystemError(os_error);
    Close();
    return result;
  }

  // |core_| is not needed for sockets that are used to accept connections.
  // The operation here is more like Open but with an existing socket.

  return OK;
}

int TCPSocketWin::Bind(const IPEndPoint& address) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_NE(socket_, INVALID_SOCKET);

  SockaddrStorage storage;
  if (!address.ToSockAddr(storage.addr, &storage.addr_len))
    return ERR_ADDRESS_INVALID;

  int result = bind(socket_, storage.addr, storage.addr_len);
  int os_error = WSAGetLastError();
  if (result < 0) {
    PLOG(ERROR) << "bind() returned an error";
    return MapSystemError(os_error);
  }

  return OK;
}

int TCPSocketWin::Listen(int backlog) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_GT(backlog, 0);
  DCHECK_NE(socket_, INVALID_SOCKET);
  DCHECK_EQ(accept_event_, WSA_INVALID_EVENT);

  accept_event_ = WSACreateEvent();
  int os_error = WSAGetLastError();
  if (accept_event_ == WSA_INVALID_EVENT) {
    PLOG(ERROR) << "WSACreateEvent()";
    return MapSystemError(os_error);
  }

  int result = listen(socket_, backlog);
  os_error = WSAGetLastError();
  if (result < 0) {
    PLOG(ERROR) << "listen() returned an error";
    return MapSystemError(os_error);
  }

  return OK;
}

int TCPSocketWin::Accept(std::unique_ptr<TCPSocketWin>* socket,
                         IPEndPoint* address,
                         CompletionOnceCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(socket);
  DCHECK(address);
  DCHECK(!callback.is_null());
  DCHECK(accept_callback_.is_null());

  net_log_.BeginEvent(NetLogEventType::TCP_ACCEPT);

  int result = AcceptInternal(socket, address);

  if (result == ERR_IO_PENDING) {
    // Start watching.
    WSAEventSelect(socket_, accept_event_, FD_ACCEPT);
    accept_watcher_.StartWatchingOnce(accept_event_, this);

    accept_socket_ = socket;
    accept_address_ = address;
    accept_callback_ = std::move(callback);
  }

  return result;
}

int TCPSocketWin::Connect(const IPEndPoint& address,
                          CompletionOnceCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_NE(socket_, INVALID_SOCKET);
  DCHECK(!connect_callback_);
  DCHECK(callback);

  // |peer_address_| and |core_| will be non-NULL if Connect() has been called.
  // Unless Close() is called to reset the internal state, a second call to
  // Connect() is not allowed.
  // Please note that we enforce this even if the previous Connect() has
  // completed and failed. Although it is allowed to connect the same |socket_|
  // again after a connection attempt failed on Windows, it results in
  // unspecified behavior according to POSIX. Therefore, we make it behave in
  // the same way as TCPSocketPosix.
  DCHECK(!peer_address_ && !core_.get());

  if (!logging_multiple_connect_attempts_)
    LogConnectBegin(AddressList(address));

  peer_address_ = std::make_unique<IPEndPoint>(address);

  int rv = DoConnect();
  if (rv == ERR_IO_PENDING) {
    // Synchronous operation not supported.
    connect_callback_ = std::move(callback);
  } else {
    DoConnectComplete(rv);
  }

  return rv;
}

bool TCPSocketWin::IsConnected() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (socket_ == INVALID_SOCKET || connect_callback_) {
    // Outstanding connect attempt pending.
    return false;
  }

  if (HasPendingRead()) {
    return true;
  }

  char c;
  int rv = recv(socket_, &c, 1, MSG_PEEK);
  if (rv == 0) {
    // Connection gracefully closed.
    return false;
  }
  int os_error = WSAGetLastError();
  if (rv == SOCKET_ERROR && os_error != WSAEWOULDBLOCK) {
    // Connection dropped/terminated due to error.
    return false;
  }

  // One byte available or would block waiting for one byte.
  return true;
}

bool TCPSocketWin::IsConnectedAndIdle() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (socket_ == INVALID_SOCKET || connect_callback_) {
    // Outstanding connect attempt pending.
    return false;
  }

  if (HasPendingRead()) {
    return true;
  }

  char c;
  int rv = recv(socket_, &c, 1, MSG_PEEK);
  if (rv >= 0) {
    // Connection gracefully closed or one byte available to read without
    // blocking.
    return false;
  }
  int os_error = WSAGetLastError();
  if (os_error != WSAEWOULDBLOCK) {
    // Connection dropped/terminated due to error.
    return false;
  }

  // No data available; blocking required.
  return true;
}

int TCPSocketDefaultWin::Read(IOBuffer* buf,
                              int buf_len,
                              CompletionOnceCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CoreImpl& core = GetCoreImpl();
  DCHECK(!core.read_iobuffer_.get());
  // base::Unretained() is safe because RetryRead() won't be called when |this|
  // is gone.
  int rv = ReadIfReady(
      buf, buf_len,
      base::BindOnce(&TCPSocketDefaultWin::RetryRead, base::Unretained(this)));
  if (rv != ERR_IO_PENDING)
    return rv;
  read_callback_ = std::move(callback);
  core.read_iobuffer_ = buf;
  core.read_buffer_length_ = buf_len;
  return ERR_IO_PENDING;
}

int TCPSocketDefaultWin::ReadIfReady(IOBuffer* buf,
                                     int buf_len,
                                     CompletionOnceCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_NE(socket_, INVALID_SOCKET);
  DCHECK(read_if_ready_callback_.is_null());

  CoreImpl& core = GetCoreImpl();
  if (!core.non_blocking_reads_initialized_) {
    WSAEventSelect(socket_, core.read_event_, FD_READ | FD_CLOSE);
    core.non_blocking_reads_initialized_ = true;
  }
  int rv = recv(socket_, buf->data(), buf_len, 0);
  int os_error = WSAGetLastError();
  if (rv == SOCKET_ERROR) {
    if (os_error != WSAEWOULDBLOCK) {
      int net_error = MapSystemError(os_error);
      NetLogSocketError(net_log_, NetLogEventType::SOCKET_READ_ERROR, net_error,
                        os_error);
      return net_error;
    }
  } else {
    net_log_.AddByteTransferEvent(NetLogEventType::SOCKET_BYTES_RECEIVED, rv,
                                  buf->data());
    activity_monitor::IncrementBytesReceived(rv);
    return rv;
  }

  read_if_ready_callback_ = std::move(callback);
  core.WatchForRead();
  return ERR_IO_PENDING;
}

int TCPSocketDefaultWin::CancelReadIfReady() {
  DCHECK(read_callback_.is_null());
  DCHECK(!read_if_ready_callback_.is_null());

  GetCoreImpl().StopWatchingForRead();
  read_if_ready_callback_.Reset();
  return net::OK;
}

int TCPSocketDefaultWin::Write(
    IOBuffer* buf,
    int buf_len,
    CompletionOnceCallback callback,
    const NetworkTrafficAnnotationTag& /* traffic_annotation */) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_NE(socket_, INVALID_SOCKET);
  CHECK(write_callback_.is_null());
  DCHECK_GT(buf_len, 0);
  CoreImpl& core = GetCoreImpl();
  DCHECK(!core.write_iobuffer_.get());

  WSABUF write_buffer;
  write_buffer.len = buf_len;
  write_buffer.buf = buf->data();

  DWORD num;
  int rv = WSASend(socket_, &write_buffer, 1, &num, 0, &core.write_overlapped_,
                   nullptr);
  int os_error = WSAGetLastError();
  if (rv == 0) {
    if (ResetEventIfSignaled(core.write_overlapped_.hEvent)) {
      rv = static_cast<int>(num);
      if (rv > buf_len || rv < 0) {
        // It seems that some winsock interceptors report that more was written
        // than was available. Treat this as an error.  http://crbug.com/27870
        LOG(ERROR) << "Detected broken LSP: Asked to write " << buf_len
                   << " bytes, but " << rv << " bytes reported.";
        return ERR_WINSOCK_UNEXPECTED_WRITTEN_BYTES;
      }
      net_log_.AddByteTransferEvent(NetLogEventType::SOCKET_BYTES_SENT, rv,
                                    buf->data());
      return rv;
    }
  } else {
    if (os_error != WSA_IO_PENDING) {
      int net_error = MapSystemError(os_error);
      NetLogSocketError(net_log_, NetLogEventType::SOCKET_WRITE_ERROR,
                        net_error, os_error);
      return net_error;
    }
  }
  write_callback_ = std::move(callback);
  core.write_iobuffer_ = buf;
  core.write_buffer_length_ = buf_len;
  core.WatchForWrite();
  return ERR_IO_PENDING;
}

int TCPSocketWin::GetLocalAddress(IPEndPoint* address) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(address);

  SockaddrStorage storage;
  if (getsockname(socket_, storage.addr, &storage.addr_len)) {
    int os_error = WSAGetLastError();
    return MapSystemError(os_error);
  }
  if (!address->FromSockAddr(storage.addr, storage.addr_len))
    return ERR_ADDRESS_INVALID;

  return OK;
}

int TCPSocketWin::GetPeerAddress(IPEndPoint* address) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(address);
  if (!IsConnected())
    return ERR_SOCKET_NOT_CONNECTED;
  *address = *peer_address_;
  return OK;
}

int TCPSocketWin::SetDefaultOptionsForServer() {
  return SetExclusiveAddrUse();
}

void TCPSocketWin::SetDefaultOptionsForClient() {
  SetTCPNoDelay(socket_, /*no_delay=*/true);
  SetTCPKeepAlive(socket_, true, kTCPKeepAliveSeconds);
}

int TCPSocketWin::SetExclusiveAddrUse() {
  // On Windows, a bound end point can be hijacked by another process by
  // setting SO_REUSEADDR. Therefore a Windows-only option SO_EXCLUSIVEADDRUSE
  // was introduced in Windows NT 4.0 SP4. If the socket that is bound to the
  // end point has SO_EXCLUSIVEADDRUSE enabled, it is not possible for another
  // socket to forcibly bind to the end point until the end point is unbound.
  // It is recommend that all server applications must use SO_EXCLUSIVEADDRUSE.
  // MSDN: http://goo.gl/M6fjQ.
  //
  // Unlike on *nix, on Windows a TCP server socket can always bind to an end
  // point in TIME_WAIT state without setting SO_REUSEADDR, therefore it is not
  // needed here.
  //
  // SO_EXCLUSIVEADDRUSE will prevent a TCP client socket from binding to an end
  // point in TIME_WAIT status. It does not have this effect for a TCP server
  // socket.

  BOOL true_value = 1;
  int rv = setsockopt(socket_, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
                      reinterpret_cast<const char*>(&true_value),
                      sizeof(true_value));
  if (rv < 0)
    return MapSystemError(errno);
  return OK;
}

int TCPSocketWin::SetReceiveBufferSize(int32_t size) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return SetSocketReceiveBufferSize(socket_, size);
}

int TCPSocketWin::SetSendBufferSize(int32_t size) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return SetSocketSendBufferSize(socket_, size);
}

bool TCPSocketWin::SetKeepAlive(bool enable, int delay) {
  if (socket_ == INVALID_SOCKET)
    return false;

  return SetTCPKeepAlive(socket_, enable, delay);
}

bool TCPSocketWin::SetNoDelay(bool no_delay) {
  if (socket_ == INVALID_SOCKET)
    return false;

  return SetTCPNoDelay(socket_, no_delay) == OK;
}

int TCPSocketWin::SetIPv6Only(bool ipv6_only) {
  return ::net::SetIPv6Only(socket_, ipv6_only);
}

void TCPSocketWin::Close() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (socket_ != INVALID_SOCKET) {
    // Only log the close event if there's actually a socket to close.
    net_log_.AddEvent(NetLogEventType::SOCKET_CLOSED);

    // Note: don't use CancelIo to cancel pending IO because it doesn't work
    // when there is a Winsock layered service provider.

    // In most socket implementations, closing a socket results in a graceful
    // connection shutdown, but in Winsock we have to call shutdown explicitly.
    // See the MSDN page "Graceful Shutdown, Linger Options, and Socket Closure"
    // at http://msdn.microsoft.com/en-us/library/ms738547.aspx
    shutdown(socket_, SD_SEND);

    // This cancels any pending IO.
    if (closesocket(socket_) < 0)
      PLOG(ERROR) << "closesocket";
    socket_ = INVALID_SOCKET;
  }

  if (!accept_callback_.is_null()) {
    accept_watcher_.StopWatching();
    accept_socket_ = nullptr;
    accept_address_ = nullptr;
    accept_callback_.Reset();
  }

  if (accept_event_) {
    WSACloseEvent(accept_event_);
    accept_event_ = WSA_INVALID_EVENT;
  }

  if (core_.get()) {
    core_->Detach();
    core_ = nullptr;

    // |core_| may still exist and own a reference to itself, if there's a
    // pending write. It has to stay alive until the operation completes, even
    // when the socket is closed. This is not the case for reads.
  }

  connect_callback_.Reset();
  OnClosed();

  peer_address_.reset();
  connect_os_error_ = 0;
}

void TCPSocketWin::DetachFromThread() {
  DETACH_FROM_THREAD(thread_checker_);
}

void TCPSocketWin::StartLoggingMultipleConnectAttempts(
    const AddressList& addresses) {
  if (!logging_multiple_connect_attempts_) {
    logging_multiple_connect_attempts_ = true;
    LogConnectBegin(addresses);
  } else {
    NOTREACHED_IN_MIGRATION();
  }
}

void TCPSocketWin::EndLoggingMultipleConnectAttempts(int net_error) {
  if (logging_multiple_connect_attempts_) {
    LogConnectEnd(net_error);
    logging_multiple_connect_attempts_ = false;
  } else {
    NOTREACHED_IN_MIGRATION();
  }
}

SocketDescriptor TCPSocketWin::ReleaseSocketDescriptorForTesting() {
  CHECK(!registered_as_io_handler_);

  SocketDescriptor socket_descriptor = socket_;
  socket_ = INVALID_SOCKET;
  Close();
  return socket_descriptor;
}

SocketDescriptor TCPSocketWin::SocketDescriptorForTesting() const {
  return socket_;
}

void TCPSocketWin::CloseSocketDescriptorForTesting() {
  CHECK_NE(socket_, INVALID_SOCKET);
  CHECK_EQ(closesocket(socket_), 0);
  // Clear `socket_` so that `Close()` doesn't attempt to close it again.
  socket_ = INVALID_SOCKET;
}

int TCPSocketWin::AcceptInternal(std::unique_ptr<TCPSocketWin>* socket,
                                 IPEndPoint* address) {
  SockaddrStorage storage;
  int new_socket = accept(socket_, storage.addr, &storage.addr_len);
  int os_error = WSAGetLastError();
  if (new_socket < 0) {
    int net_error = MapSystemError(os_error);
    if (net_error != ERR_IO_PENDING)
      net_log_.EndEventWithNetErrorCode(NetLogEventType::TCP_ACCEPT, net_error);
    return net_error;
  }

  IPEndPoint ip_end_point;
  if (!ip_end_point.FromSockAddr(storage.addr, storage.addr_len)) {
    NOTREACHED_IN_MIGRATION();
    if (closesocket(new_socket) < 0)
      PLOG(ERROR) << "closesocket";
    int net_error = ERR_ADDRESS_INVALID;
    net_log_.EndEventWithNetErrorCode(NetLogEventType::TCP_ACCEPT, net_error);
    return net_error;
  }
  auto tcp_socket =
      TCPSocketWin::Create(nullptr, net_log_.net_log(), net_log_.source());
  int adopt_result = tcp_socket->AdoptConnectedSocket(new_socket, ip_end_point);
  if (adopt_result != OK) {
    net_log_.EndEventWithNetErrorCode(NetLogEventType::TCP_ACCEPT,
                                      adopt_result);
    return adopt_result;
  }
  *socket = std::move(tcp_socket);
  *address = ip_end_point;
  net_log_.EndEvent(NetLogEventType::TCP_ACCEPT, [&] {
    return CreateNetLogIPEndPointParams(&ip_end_point);
  });
  return OK;
}

void TCPSocketWin::OnObjectSignaled(HANDLE object) {
  WSANETWORKEVENTS ev;
  if (WSAEnumNetworkEvents(socket_, accept_event_, &ev) == SOCKET_ERROR) {
    PLOG(ERROR) << "WSAEnumNetworkEvents()";
    return;
  }

  if (ev.lNetworkEvents & FD_ACCEPT) {
    int result = AcceptInternal(accept_socket_, accept_address_);
    if (result != ERR_IO_PENDING) {
      accept_socket_ = nullptr;
      accept_address_ = nullptr;
      std::move(accept_callback_).Run(result);
    }
  } else {
    // This happens when a client opens a connection and closes it before we
    // have a chance to accept it.
    DCHECK(ev.lNetworkEvents == 0);

    // Start watching the next FD_ACCEPT event.
    WSAEventSelect(socket_, accept_event_, FD_ACCEPT);
    accept_watcher_.StartWatchingOnce(accept_event_, this);
  }
}

int TCPSocketWin::DoConnect() {
  DCHECK_EQ(connect_os_error_, 0);
  DCHECK(!core_.get());

  net_log_.BeginEvent(NetLogEventType::TCP_CONNECT_ATTEMPT, [&] {
    return CreateNetLogIPEndPointParams(peer_address_.get());
  });

  core_ = CreateCore();

  // WSAEventSelect sets the socket to non-blocking mode as a side effect.
  // Our connect() and recv() calls require that the socket be non-blocking.
  WSAEventSelect(socket_, core_->GetConnectEvent(), FD_CONNECT);

  SockaddrStorage storage;
  if (!peer_address_->ToSockAddr(storage.addr, &storage.addr_len))
    return ERR_ADDRESS_INVALID;

  // Set option to choose a random port, if the socket is not already bound.
  // Ignore failures, which may happen if the socket was already bound.
  if (base::win::GetVersion() >= base::win::Version::WIN10_20H1 &&
      base::FeatureList::IsEnabled(features::kEnableTcpPortRandomization)) {
    BOOL randomize_port = TRUE;
    setsockopt(socket_, SOL_SOCKET, SO_RANDOMIZE_PORT,
               reinterpret_cast<const char*>(&randomize_port),
               sizeof(randomize_port));
  }

  if (!connect(socket_, storage.addr, storage.addr_len)) {
    // Connected without waiting!
    //
    // The MSDN page for connect says:
    //   With a nonblocking socket, the connection attempt cannot be completed
    //   immediately. In this case, connect will return SOCKET_ERROR, and
    //   WSAGetLastError will return WSAEWOULDBLOCK.
    // which implies that for a nonblocking socket, connect never returns 0.
    // It's not documented whether the event object will be signaled or not
    // if connect does return 0.  So the code below is essentially dead code
    // and we don't know if it's correct.
    NOTREACHED_IN_MIGRATION();

    if (ResetEventIfSignaled(core_->GetConnectEvent())) {
      return OK;
    }
  } else {
    int os_error = WSAGetLastError();
    if (os_error != WSAEWOULDBLOCK) {
      LOG(ERROR) << "connect failed: " << os_error;
      connect_os_error_ = os_error;
      int rv = MapConnectError(os_error);
      CHECK_NE(ERR_IO_PENDING, rv);
      return rv;
    }
  }

  core_->WatchForConnect();
  return ERR_IO_PENDING;
}

void TCPSocketWin::DoConnectComplete(int result) {
  // Log the end of this attempt (and any OS error it threw).
  int os_error = connect_os_error_;
  connect_os_error_ = 0;
  if (result != OK) {
    net_log_.EndEventWithIntParams(NetLogEventType::TCP_CONNECT_ATTEMPT,
                                   "os_error", os_error);
  } else {
    net_log_.EndEvent(NetLogEventType::TCP_CONNECT_ATTEMPT);
  }

  if (!logging_multiple_connect_attempts_)
    LogConnectEnd(result);
}

void TCPSocketWin::LogConnectBegin(const AddressList& addresses) {
  net_log_.BeginEvent(NetLogEventType::TCP_CONNECT,
                      [&] { return addresses.NetLogParams(); });
}

void TCPSocketWin::LogConnectEnd(int net_error) {
  if (net_error != OK) {
    net_log_.EndEventWithNetErrorCode(NetLogEventType::TCP_CONNECT, net_error);
    return;
  }

  net_log_.EndEvent(NetLogEventType::TCP_CONNECT, [&] {
    net::IPEndPoint local_address;
    int net_error = GetLocalAddress(&local_address);
    net::IPEndPoint remote_address;
    if (net_error == net::OK)
      net_error = GetPeerAddress(&remote_address);
    if (net_error != net::OK)
      return NetLogParamsWithInt("get_address_net_error", net_error);
    return CreateNetLogAddressPairParams(local_address, remote_address);
  });
}

void TCPSocketDefaultWin::RetryRead(int rv) {
  CoreImpl& core = GetCoreImpl();
  DCHECK(core.read_iobuffer_);

  if (rv == OK) {
    // base::Unretained() is safe because RetryRead() won't be called when
    // |this| is gone.
    rv = ReadIfReady(core.read_iobuffer_.get(), core.read_buffer_length_,
                     base::BindOnce(&TCPSocketDefaultWin::RetryRead,
                                    base::Unretained(this)));
    if (rv == ERR_IO_PENDING)
      return;
  }
  core.read_iobuffer_ = nullptr;
  core.read_buffer_length_ = 0;
  std::move(read_callback_).Run(rv);
}

void TCPSocketWin::DidCompleteConnect() {
  DCHECK(connect_callback_);
  int result;

  WSANETWORKEVENTS events;
  int rv = WSAEnumNetworkEvents(socket_, core_->GetConnectEvent(), &events);
  int os_error = WSAGetLastError();
  if (rv == SOCKET_ERROR) {
    DLOG(FATAL)
        << "WSAEnumNetworkEvents() failed with SOCKET_ERROR, os_error = "
        << os_error;
    result = MapSystemError(os_error);
  } else if (events.lNetworkEvents & FD_CONNECT) {
    os_error = events.iErrorCode[FD_CONNECT_BIT];
    result = MapConnectError(os_error);
  } else {
    DLOG(FATAL) << "WSAEnumNetworkEvents() failed, rv = " << rv;
    result = ERR_UNEXPECTED;
  }

  connect_os_error_ = os_error;
  DoConnectComplete(result);

  DCHECK_NE(result, ERR_IO_PENDING);
  std::move(connect_callback_).Run(result);
}

void TCPSocketDefaultWin::DidCompleteWrite() {
  DCHECK(!write_callback_.is_null());

  CoreImpl& core = GetCoreImpl();
  DWORD num_bytes, flags;
  BOOL ok = WSAGetOverlappedResult(socket_, &core.write_overlapped_, &num_bytes,
                                   FALSE, &flags);
  int os_error = WSAGetLastError();
  WSAResetEvent(core.write_overlapped_.hEvent);
  int rv;
  if (!ok) {
    rv = MapSystemError(os_error);
    NetLogSocketError(net_log_, NetLogEventType::SOCKET_WRITE_ERROR, rv,
                      os_error);
  } else {
    rv = static_cast<int>(num_bytes);
    if (rv > core.write_buffer_length_ || rv < 0) {
      // It seems that some winsock interceptors report that more was written
      // than was available. Treat this as an error.  http://crbug.com/27870
      LOG(ERROR) << "Detected broken LSP: Asked to write "
                 << core.write_buffer_length_ << " bytes, but " << rv
                 << " bytes reported.";
      rv = ERR_WINSOCK_UNEXPECTED_WRITTEN_BYTES;
    } else {
      net_log_.AddByteTransferEvent(NetLogEventType::SOCKET_BYTES_SENT,
                                    num_bytes, core.write_iobuffer_->data());
    }
  }

  core.write_iobuffer_ = nullptr;

  DCHECK_NE(rv, ERR_IO_PENDING);
  std::move(write_callback_).Run(rv);
}

void TCPSocketDefaultWin::DidSignalRead() {
  DCHECK(!read_if_ready_callback_.is_null());

  CoreImpl& core = GetCoreImpl();
  int os_error = 0;
  WSANETWORKEVENTS network_events;
  int rv = WSAEnumNetworkEvents(socket_, core.read_event_, &network_events);
  os_error = WSAGetLastError();

  if (rv == SOCKET_ERROR) {
    rv = MapSystemError(os_error);
  } else if (network_events.lNetworkEvents) {
    DCHECK_EQ(network_events.lNetworkEvents & ~(FD_READ | FD_CLOSE), 0);
    // If network_events.lNetworkEvents is FD_CLOSE and
    // network_events.iErrorCode[FD_CLOSE_BIT] is 0, it is a graceful
    // connection closure. It is tempting to directly set rv to 0 in
    // this case, but the MSDN pages for WSAEventSelect and
    // WSAAsyncSelect recommend we still call RetryRead():
    //   FD_CLOSE should only be posted after all data is read from a
    //   socket, but an application should check for remaining data upon
    //   receipt of FD_CLOSE to avoid any possibility of losing data.
    //
    // If network_events.iErrorCode[FD_READ_BIT] or
    // network_events.iErrorCode[FD_CLOSE_BIT] is nonzero, still call
    // RetryRead() because recv() reports a more accurate error code
    // (WSAECONNRESET vs. WSAECONNABORTED) when the connection was
    // reset.
    rv = OK;
  } else {
    // This may happen because Read() may succeed synchronously and
    // consume all the received data without resetting the event object.
    core.WatchForRead();
    return;
  }

  DCHECK_NE(rv, ERR_IO_PENDING);
  std::move(read_if_ready_callback_).Run(rv);
}

bool TCPSocketWin::GetEstimatedRoundTripTime(base::TimeDelta* out_rtt) const {
  DCHECK(out_rtt);
  // TODO(bmcquade): Consider implementing using
  // GetPerTcpConnectionEStats/GetPerTcp6ConnectionEStats.
  return false;
}

void TCPSocketWin::ApplySocketTag(const SocketTag& tag) {
  // Windows does not support any specific SocketTags so fail if any non-default
  // tag is applied.
  CHECK(tag == SocketTag());
}

int TCPSocketWin::BindToNetwork(handles::NetworkHandle network) {
  NOTIMPLEMENTED();
  return ERR_NOT_IMPLEMENTED;
}

TCPSocketDefaultWin::TCPSocketDefaultWin(
    std::unique_ptr<SocketPerformanceWatcher> socket_performance_watcher,
    NetLog* net_log,
    const NetLogSource& source)
    : TCPSocketWin(std::move(socket_performance_watcher), net_log, source) {}

TCPSocketDefaultWin::TCPSocketDefaultWin(
    std::unique_ptr<SocketPerformanceWatcher> socket_performance_watcher,
    NetLogWithSource net_log_source)
    : TCPSocketWin(std::move(socket_performance_watcher),
                   std::move(net_log_source)) {}

TCPSocketDefaultWin::~TCPSocketDefaultWin() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  Close();
}

TCPSocketDefaultWin::CoreImpl& TCPSocketDefaultWin::GetCoreImpl() {
  return CHECK_DEREF(static_cast<CoreImpl*>(core_.get()));
}

scoped_refptr<TCPSocketWin::Core> TCPSocketDefaultWin::CreateCore() {
  return base::MakeRefCounted<CoreImpl>(this);
}

bool TCPSocketDefaultWin::HasPendingRead() const {
  CHECK(!read_callback_ || read_if_ready_callback_);
  return !read_if_ready_callback_.is_null();
}

void TCPSocketDefaultWin::OnClosed() {
  read_callback_.Reset();
  read_if_ready_callback_.Reset();
  write_callback_.Reset();
}

}  // namespace net
