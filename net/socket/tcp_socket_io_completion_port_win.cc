// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/tcp_socket_io_completion_port_win.h"

#include <functional>
#include <utility>

#include "base/dcheck_is_on.h"
#include "base/memory/scoped_refptr.h"
#include "base/message_loop/message_pump_win.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/current_thread.h"
#include "base/threading/thread_checker.h"
#include "base/win/object_watcher.h"
#include "base/win/scoped_handle.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/log/net_log.h"
#include "net/socket/socket_net_log_params.h"

namespace net {

namespace {

class WSAEventHandleTraits {
 public:
  using Handle = WSAEVENT;

  WSAEventHandleTraits() = delete;
  WSAEventHandleTraits(const WSAEventHandleTraits&) = delete;
  WSAEventHandleTraits& operator=(const WSAEventHandleTraits&) = delete;

  static bool CloseHandle(Handle handle) {
    return ::WSACloseEvent(handle) != FALSE;
  }
  static bool IsHandleValid(Handle handle) {
    return handle != WSA_INVALID_EVENT;
  }
  static Handle NullHandle() { return WSA_INVALID_EVENT; }
};

// "Windows Sockets 2 event objects are system objects in Windows environments"
// so `base::win::VerifierTraits` verifier can be used.
// Source:
// https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsacreateevent
#if DCHECK_IS_ON()
using VerifierTraits = base::win::VerifierTraits;
#else
using VerifierTraits = base::win::DummyVerifierTraits;
#endif
using ScopedWSAEventHandle =
    base::win::GenericScopedHandle<WSAEventHandleTraits, VerifierTraits>;

}  // namespace

class TcpSocketIoCompletionPortWin::CoreImpl
    : public TCPSocketWin::Core,
      public base::win::ObjectWatcher::Delegate,
      public base::MessagePumpForIO::IOHandler {
 public:
  // Context for an overlapped I/O operation.
  struct IOContext : public base::MessagePumpForIO::IOContext {
    using CompletionMethod =
        int (TcpSocketIoCompletionPortWin::*)(DWORD bytes_transferred,
                                              DWORD error,
                                              scoped_refptr<IOBuffer> buffer,
                                              int buffer_length);

    explicit IOContext(scoped_refptr<CoreImpl> core);

    // Keeps the `CoreImpl` alive until the operation is complete. Required to
    // handle `base::MessagePumpForIO::IOHandler::OnIOCompleted`.
    const scoped_refptr<CoreImpl> core_keep_alive;

    // Buffer used for the operation.
    scoped_refptr<IOBuffer> buffer;
    int buffer_length = 0;

    // Method to call upon completion of the operation. The return value is
    // passed to `completion_callback`.
    CompletionMethod completion_method = nullptr;

    // External callback to invoke upon completion of the operation.
    CompletionOnceCallback completion_callback;
  };

  explicit CoreImpl(TcpSocketIoCompletionPortWin* socket);

  CoreImpl(const CoreImpl&) = delete;
  CoreImpl& operator=(const CoreImpl&) = delete;

  // TCPSocketWin::Core:
  void Detach() override;
  HANDLE GetConnectEvent() override;
  void WatchForConnect() override;

 private:
  ~CoreImpl() override;

  // base::win::ObjectWatcher::Delegate:
  void OnObjectSignaled(HANDLE object) override;

  // base::MessagePumpForIO::IOHandler:
  void OnIOCompleted(base::MessagePumpForIO::IOContext* context,
                     DWORD bytes_transferred,
                     DWORD error) override;

  // Stops watching and closes the connect event, if valid.
  void StopWatchingAndCloseConnectEvent();

  // Owning socket.
  raw_ptr<TcpSocketIoCompletionPortWin> socket_;

  // Event to watch for connect completion.
  ScopedWSAEventHandle connect_event_;

  // Watcher for `connect_event_`.
  base::win::ObjectWatcher connect_watcher_;
};

TcpSocketIoCompletionPortWin::TcpSocketIoCompletionPortWin(
    std::unique_ptr<SocketPerformanceWatcher> socket_performance_watcher,
    NetLog* net_log,
    const NetLogSource& source)
    : TCPSocketWin(std::move(socket_performance_watcher), net_log, source) {}

TcpSocketIoCompletionPortWin::TcpSocketIoCompletionPortWin(
    std::unique_ptr<SocketPerformanceWatcher> socket_performance_watcher,
    NetLogWithSource net_log_source)
    : TCPSocketWin(std::move(socket_performance_watcher), net_log_source) {}

TcpSocketIoCompletionPortWin::~TcpSocketIoCompletionPortWin() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  Close();
}

int TcpSocketIoCompletionPortWin::Read(IOBuffer* buf,
                                       int buf_len,
                                       CompletionOnceCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CHECK_NE(socket_, INVALID_SOCKET);

  EnsureRegisteredAsIOHandler();
  CoreImpl& core = GetCoreImpl();

  WSABUF read_buffer;
  read_buffer.len = buf_len;
  read_buffer.buf = buf->data();
  DWORD flags = 0;

  auto context = std::make_unique<CoreImpl::IOContext>(&core);
  context->buffer = buf;
  context->buffer_length = buf_len;
  context->completion_method = &TcpSocketIoCompletionPortWin::DidCompleteRead;
  context->completion_callback = std::move(callback);

  const auto rv =
      ::WSARecv(socket_, &read_buffer, /*dwBufferCount=*/1,
                /*lpNumberOfBytesRecvd=*/nullptr, &flags, &context->overlapped,
                /*lpCompletionRoutine=*/nullptr);

  if (rv != 0) {
    CHECK_EQ(rv, SOCKET_ERROR);
    int wsa_error = ::WSAGetLastError();
    if (wsa_error != WSA_IO_PENDING) {
      // "Any other error code [than WSA_IO_PENDING] indicates that [...] no
      // completion indication will occur", so `context` must be freed here.
      // https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsarecv
      context.reset();

      int net_error = MapSystemError(wsa_error);
      NetLogSocketError(net_log_, NetLogEventType::SOCKET_READ_ERROR, net_error,
                        wsa_error);
      return net_error;
    }
  }

  // When the operation completes immediately (return value 0) or is
  // successfully initiated (WSAGetLastError is WSA_IO_PENDING), a completion
  // packet is enqueued or will be enqueued later. Release `context` here to let
  // `OnIOCompleted` take ownership of it upon completion of the operation.
  // https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsarecv
  context.release();

  ++num_pending_reads_;

  return ERR_IO_PENDING;
}

int TcpSocketIoCompletionPortWin::ReadIfReady(IOBuffer* buf,
                                              int buf_len,
                                              CompletionOnceCallback callback) {
  return ERR_READ_IF_READY_NOT_IMPLEMENTED;
}

int TcpSocketIoCompletionPortWin::CancelReadIfReady() {
  NOTREACHED();
}

int TcpSocketIoCompletionPortWin::Write(
    IOBuffer* buf,
    int buf_len,
    CompletionOnceCallback callback,
    const NetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  EnsureRegisteredAsIOHandler();
  CoreImpl& core = GetCoreImpl();

  WSABUF write_buffer;
  write_buffer.len = buf_len;
  write_buffer.buf = buf->data();
  DWORD flags = 0;

  auto context = std::make_unique<CoreImpl::IOContext>(&core);
  context->buffer = buf;
  context->buffer_length = buf_len;
  context->completion_callback = std::move(callback);
  context->completion_method = &TcpSocketIoCompletionPortWin::DidCompleteWrite;

  int rv =
      ::WSASend(socket_, &write_buffer, /*dwBufferCount=*/1,
                /*lpNumberOfBytesSent=*/nullptr, flags, &context->overlapped,
                /*lpCompletionRoutine=*/nullptr);

  if (rv != 0) {
    CHECK_EQ(rv, SOCKET_ERROR);
    const int wsa_error = ::WSAGetLastError();
    if (wsa_error != WSA_IO_PENDING) {
      // "Any other error code [than WSA_IO_PENDING] indicates that [...] no
      // completion indication will occur", so `context` must be freed here.
      // https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsasend
      context.reset();

      int net_error = MapSystemError(wsa_error);
      NetLogSocketError(net_log_, NetLogEventType::SOCKET_WRITE_ERROR,
                        net_error, wsa_error);
      return net_error;
    }
  }

  // When the operation completes immediately (return value 0) or is
  // successfully initiated (WSAGetLastError is WSA_IO_PENDING), a completion
  // packet is enqueued or will be enqueued later. Release `context` here to let
  // `OnIOCompleted` take ownership of it upon completion of the operation.
  // https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsasend
  context.release();

  return ERR_IO_PENDING;
}

scoped_refptr<TCPSocketWin::Core> TcpSocketIoCompletionPortWin::CreateCore() {
  return base::MakeRefCounted<CoreImpl>(this);
}

bool TcpSocketIoCompletionPortWin::HasPendingRead() const {
  return num_pending_reads_ != 0;
}

void TcpSocketIoCompletionPortWin::OnClosed() {}

void TcpSocketIoCompletionPortWin::EnsureRegisteredAsIOHandler() {
  CHECK_NE(socket_, INVALID_SOCKET);
  if (!registered_as_io_handler_) {
    CoreImpl& core = GetCoreImpl();
    auto hresult = base::CurrentIOThread::Get()->RegisterIOHandler(
        reinterpret_cast<HANDLE>(socket_), &core);
    CHECK(SUCCEEDED(hresult));
    registered_as_io_handler_ = true;
  }
}

int TcpSocketIoCompletionPortWin::DidCompleteRead(
    DWORD bytes_transferred,
    DWORD error,
    scoped_refptr<IOBuffer> buffer,
    int buffer_length) {
  CHECK_GT(num_pending_reads_, 0);
  --num_pending_reads_;

  if (error == ERROR_SUCCESS) {
    // `bytes_transferred` should be <= `buffer_length` so cast should succeed.
    const int rv = base::checked_cast<int>(bytes_transferred);
    net_log_.AddByteTransferEvent(NetLogEventType::SOCKET_BYTES_RECEIVED, rv,
                                  buffer->data());
    return rv;
  }

  const int rv = MapSystemError(error);
  CHECK_NE(rv, ERR_IO_PENDING);
  NetLogSocketError(net_log_, NetLogEventType::SOCKET_READ_ERROR, rv, error);
  return rv;
}

int TcpSocketIoCompletionPortWin::DidCompleteWrite(
    DWORD bytes_transferred,
    DWORD error,
    scoped_refptr<IOBuffer> buffer,
    int buffer_length) {
  if (error == ERROR_SUCCESS) {
    // `bytes_transferred` should be <= `buffer_length` so cast should succeed.
    const int rv = base::checked_cast<int>(bytes_transferred);
    if (rv > buffer_length) {
      // It seems that some winsock interceptors report that more was written
      // than was available. Treat this as an error.  https://crbug.com/27870
      LOG(ERROR) << "Detected broken LSP: Asked to write " << buffer_length
                 << " bytes, but " << rv << " bytes reported.";
      return ERR_WINSOCK_UNEXPECTED_WRITTEN_BYTES;
    }

    net_log_.AddByteTransferEvent(NetLogEventType::SOCKET_BYTES_SENT, rv,
                                  buffer->data());
    return rv;
  }

  const int rv = MapSystemError(error);
  CHECK_NE(rv, ERR_IO_PENDING);
  NetLogSocketError(net_log_, NetLogEventType::SOCKET_WRITE_ERROR, rv, error);
  return rv;
}

TcpSocketIoCompletionPortWin::CoreImpl&
TcpSocketIoCompletionPortWin::GetCoreImpl() {
  return CHECK_DEREF(static_cast<CoreImpl*>(core_.get()));
}

TcpSocketIoCompletionPortWin::CoreImpl::IOContext::IOContext(
    scoped_refptr<CoreImpl> core)
    : core_keep_alive(std::move(core)) {}

TcpSocketIoCompletionPortWin::CoreImpl::CoreImpl(
    TcpSocketIoCompletionPortWin* socket)
    : base::MessagePumpForIO::IOHandler(FROM_HERE), socket_(socket) {}

void TcpSocketIoCompletionPortWin::CoreImpl::Detach() {
  StopWatchingAndCloseConnectEvent();

  // It is not possible to stop ongoing read or write operations. Clear
  // `socket_` so that the completion handler doesn't invoke completion methods.
  socket_ = nullptr;
}

HANDLE TcpSocketIoCompletionPortWin::CoreImpl::GetConnectEvent() {
  if (!connect_event_.IsValid()) {
    // Lazy-initialize the event.
    connect_event_.Set(::WSACreateEvent());
    ::WSAEventSelect(socket_->socket_, connect_event_.get(), FD_CONNECT);
  }
  return connect_event_.get();
}

void TcpSocketIoCompletionPortWin::CoreImpl::WatchForConnect() {
  CHECK(connect_event_.IsValid());
  connect_watcher_.StartWatchingOnce(connect_event_.get(), this);
}

TcpSocketIoCompletionPortWin::CoreImpl::~CoreImpl() {
  CHECK(!socket_);
}

void TcpSocketIoCompletionPortWin::CoreImpl::OnObjectSignaled(HANDLE object) {
  CHECK_EQ(object, connect_event_.get());
  CHECK(socket_);
  CHECK(!!socket_->connect_callback_);

  // Stop watching and close the event since it's no longer needed.
  StopWatchingAndCloseConnectEvent();

  socket_->DidCompleteConnect();
}

void TcpSocketIoCompletionPortWin::CoreImpl::OnIOCompleted(
    base::MessagePumpForIO::IOContext* context,
    DWORD bytes_transferred,
    DWORD error) {
  // Take ownership of `context`, which was released in `Read` or `Write`. The
  // cast is safe because all overlapped I/O operations handled by this are
  // issued with the OVERLAPPED member of an `IOContext` object.
  std::unique_ptr<IOContext> derived_context(static_cast<IOContext*>(context));

  if (socket_) {
    const int rv = std::invoke(
        derived_context->completion_method, socket_, bytes_transferred, error,
        std::move(derived_context->buffer), derived_context->buffer_length);
    std::move(derived_context->completion_callback).Run(rv);
  }
}

void TcpSocketIoCompletionPortWin::CoreImpl::
    StopWatchingAndCloseConnectEvent() {
  if (connect_event_.IsValid()) {
    connect_watcher_.StopWatching();
    connect_event_.Close();
  }
}

}  // namespace net
