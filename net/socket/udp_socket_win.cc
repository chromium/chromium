// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/udp_socket_win.h"

#include <mstcpip.h>

#include "base/bind.h"
#include "base/callback.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/task/post_task.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_address.h"
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
#include "net/socket/socket_descriptor.h"
#include "net/socket/socket_options.h"
#include "net/socket/socket_tag.h"
#include "net/socket/udp_net_log_parameters.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace {

const int kBindRetries = 10;
const int kPortStart = 1024;
const int kPortEnd = 65535;

}  // namespace

namespace net {

// This class encapsulates all the state that has to be preserved as long as
// there is a network IO operation in progress. If the owner UDPSocketWin
// is destroyed while an operation is in progress, the Core is detached and it
// lives until the operation completes and the OS doesn't reference any resource
// declared on this class anymore.
class UDPSocketWin::Core : public base::RefCounted<Core> {
 public:
  explicit Core(UDPSocketWin* socket);

  // Start watching for the end of a read or write operation.
  void WatchForRead();
  void WatchForWrite();

  // The UDPSocketWin is going away.
  void Detach() { socket_ = nullptr; }

  // The separate OVERLAPPED variables for asynchronous operation.
  OVERLAPPED read_overlapped_;
  OVERLAPPED write_overlapped_;

  // The buffers used in Read() and Write().
  scoped_refptr<IOBuffer> read_iobuffer_;
  scoped_refptr<IOBuffer> write_iobuffer_;

  // The address storage passed to WSARecvFrom().
  SockaddrStorage recv_addr_storage_;

 private:
  friend class base::RefCounted<Core>;

  class ReadDelegate : public base::win::ObjectWatcher::Delegate {
   public:
    explicit ReadDelegate(Core* core) : core_(core) {}
    ~ReadDelegate() override {}

    // base::ObjectWatcher::Delegate methods:
    void OnObjectSignaled(HANDLE object) override;

   private:
    Core* const core_;
  };

  class WriteDelegate : public base::win::ObjectWatcher::Delegate {
   public:
    explicit WriteDelegate(Core* core) : core_(core) {}
    ~WriteDelegate() override {}

    // base::ObjectWatcher::Delegate methods:
    void OnObjectSignaled(HANDLE object) override;

   private:
    Core* const core_;
  };

  ~Core();

  // The socket that created this object.
  UDPSocketWin* socket_;

  // |reader_| handles the signals from |read_watcher_|.
  ReadDelegate reader_;
  // |writer_| handles the signals from |write_watcher_|.
  WriteDelegate writer_;

  // |read_watcher_| watches for events from Read().
  base::win::ObjectWatcher read_watcher_;
  // |write_watcher_| watches for events from Write();
  base::win::ObjectWatcher write_watcher_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

UDPSocketWin::Core::Core(UDPSocketWin* socket)
    : socket_(socket),
      reader_(this),
      writer_(this) {
  memset(&read_overlapped_, 0, sizeof(read_overlapped_));
  memset(&write_overlapped_, 0, sizeof(write_overlapped_));

  read_overlapped_.hEvent = WSACreateEvent();
  write_overlapped_.hEvent = WSACreateEvent();
}

UDPSocketWin::Core::~Core() {
  // Make sure the message loop is not watching this object anymore.
  read_watcher_.StopWatching();
  write_watcher_.StopWatching();

  WSACloseEvent(read_overlapped_.hEvent);
  memset(&read_overlapped_, 0xaf, sizeof(read_overlapped_));
  WSACloseEvent(write_overlapped_.hEvent);
  memset(&write_overlapped_, 0xaf, sizeof(write_overlapped_));
}

void UDPSocketWin::Core::WatchForRead() {
  // We grab an extra reference because there is an IO operation in progress.
  // Balanced in ReadDelegate::OnObjectSignaled().
  AddRef();
  read_watcher_.StartWatchingOnce(read_overlapped_.hEvent, &reader_);
}

void UDPSocketWin::Core::WatchForWrite() {
  // We grab an extra reference because there is an IO operation in progress.
  // Balanced in WriteDelegate::OnObjectSignaled().
  AddRef();
  write_watcher_.StartWatchingOnce(write_overlapped_.hEvent, &writer_);
}

void UDPSocketWin::Core::ReadDelegate::OnObjectSignaled(HANDLE object) {
  DCHECK_EQ(object, core_->read_overlapped_.hEvent);
  if (core_->socket_)
    core_->socket_->DidCompleteRead();

  core_->Release();
}

void UDPSocketWin::Core::WriteDelegate::OnObjectSignaled(HANDLE object) {
  DCHECK_EQ(object, core_->write_overlapped_.hEvent);
  if (core_->socket_)
    core_->socket_->DidCompleteWrite();

  core_->Release();
}
//-----------------------------------------------------------------------------

QwaveApi::QwaveApi() : qwave_supported_(false) {
  HMODULE qwave = LoadLibrary(L"qwave.dll");
  if (!qwave)
    return;
  create_handle_func_ =
      (CreateHandleFn)GetProcAddress(qwave, "QOSCreateHandle");
  close_handle_func_ =
      (CloseHandleFn)GetProcAddress(qwave, "QOSCloseHandle");
  add_socket_to_flow_func_ =
      (AddSocketToFlowFn)GetProcAddress(qwave, "QOSAddSocketToFlow");
  remove_socket_from_flow_func_ =
      (RemoveSocketFromFlowFn)GetProcAddress(qwave, "QOSRemoveSocketFromFlow");
  set_flow_func_ = (SetFlowFn)GetProcAddress(qwave, "QOSSetFlow");

  if (create_handle_func_ && close_handle_func_ &&
      add_socket_to_flow_func_ && remove_socket_from_flow_func_ &&
      set_flow_func_) {
    qwave_supported_ = true;
  }
}

QwaveApi* QwaveApi::GetDefault() {
  static base::LazyInstance<QwaveApi>::Leaky lazy_qwave =
      LAZY_INSTANCE_INITIALIZER;
  return lazy_qwave.Pointer();
}

bool QwaveApi::qwave_supported() const {
  return qwave_supported_;
}

void QwaveApi::OnFatalError() {
  // Disable everything moving forward.
  qwave_supported_ = false;
}

BOOL QwaveApi::CreateHandle(PQOS_VERSION version, PHANDLE handle) {
  return create_handle_func_(version, handle);
}

BOOL QwaveApi::CloseHandle(HANDLE handle) {
  return close_handle_func_(handle);
}

BOOL QwaveApi::AddSocketToFlow(HANDLE handle,
                               SOCKET socket,
                               PSOCKADDR addr,
                               QOS_TRAFFIC_TYPE traffic_type,
                               DWORD flags,
                               PQOS_FLOWID flow_id) {
  return add_socket_to_flow_func_(handle, socket, addr, traffic_type, flags,
                                  flow_id);
}

BOOL QwaveApi::RemoveSocketFromFlow(HANDLE handle,
                                    SOCKET socket,
                                    QOS_FLOWID flow_id,
                                    DWORD reserved) {
  return remove_socket_from_flow_func_(handle, socket, flow_id, reserved);
}

BOOL QwaveApi::SetFlow(HANDLE handle,
                       QOS_FLOWID flow_id,
                       QOS_SET_FLOW op,
                       ULONG size,
                       PVOID data,
                       DWORD reserved,
                       LPOVERLAPPED overlapped) {
  return set_flow_func_(handle, flow_id, op, size, data, reserved, overlapped);
}

//-----------------------------------------------------------------------------

UDPSocketWin::UDPSocketWin(DatagramSocket::BindType bind_type,
                           net::NetLog* net_log,
                           const net::NetLogSource& source)
    : socket_(INVALID_SOCKET),
      addr_family_(0),
      is_connected_(false),
      socket_options_(SOCKET_OPTION_MULTICAST_LOOP),
      multicast_interface_(0),
      multicast_time_to_live_(1),
      bind_type_(bind_type),
      use_non_blocking_io_(false),
      read_iobuffer_len_(0),
      write_iobuffer_len_(0),
      recv_from_address_(nullptr),
      net_log_(NetLogWithSource::Make(net_log, NetLogSourceType::UDP_SOCKET)) {
  EnsureWinsockInit();
  net_log_.BeginEventReferencingSource(NetLogEventType::SOCKET_ALIVE, source);
}

UDPSocketWin::~UDPSocketWin() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  Close();
  net_log_.EndEvent(NetLogEventType::SOCKET_ALIVE);
}

int UDPSocketWin::Open(AddressFamily address_family) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_EQ(socket_, INVALID_SOCKET);

  addr_family_ = ConvertAddressFamily(address_family);
  socket_ = CreatePlatformSocket(addr_family_, SOCK_DGRAM, IPPROTO_UDP);
  if (socket_ == INVALID_SOCKET)
    return MapSystemError(WSAGetLastError());
  if (!use_non_blocking_io_) {
    core_ = new Core(this);
  } else {
    read_write_event_.Set(WSACreateEvent());
    WSAEventSelect(socket_, read_write_event_.Get(), FD_READ | FD_WRITE);
  }
  return OK;
}

void UDPSocketWin::Close() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (socket_ == INVALID_SOCKET)
    return;

  // Remove socket_ from the QoS subsystem before we invalidate it.
  dscp_manager_ = nullptr;

  // Zero out any pending read/write callback state.
  read_callback_.Reset();
  recv_from_address_ = nullptr;
  write_callback_.Reset();

  base::TimeTicks start_time = base::TimeTicks::Now();
  closesocket(socket_);
  UMA_HISTOGRAM_TIMES("Net.UDPSocketWinClose",
                      base::TimeTicks::Now() - start_time);
  socket_ = INVALID_SOCKET;
  addr_family_ = 0;
  is_connected_ = false;

  // Release buffers to free up memory.
  read_iobuffer_ = nullptr;
  read_iobuffer_len_ = 0;
  write_iobuffer_ = nullptr;
  write_iobuffer_len_ = 0;

  read_write_watcher_.StopWatching();
  read_write_event_.Close();

  event_pending_.InvalidateWeakPtrs();

  if (core_) {
    core_->Detach();
    core_ = nullptr;
  }
}

int UDPSocketWin::GetPeerAddress(IPEndPoint* address) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(address);
  if (!is_connected())
    return ERR_SOCKET_NOT_CONNECTED;

  // TODO(szym): Simplify. http://crbug.com/126152
  if (!remote_address_.get()) {
    SockaddrStorage storage;
    if (getpeername(socket_, storage.addr, &storage.addr_len))
      return MapSystemError(WSAGetLastError());
    std::unique_ptr<IPEndPoint> remote_address(new IPEndPoint());
    if (!remote_address->FromSockAddr(storage.addr, storage.addr_len))
      return ERR_ADDRESS_INVALID;
    remote_address_ = std::move(remote_address);
  }

  *address = *remote_address_;
  return OK;
}

int UDPSocketWin::GetLocalAddress(IPEndPoint* address) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(address);
  if (!is_connected())
    return ERR_SOCKET_NOT_CONNECTED;

  // TODO(szym): Simplify. http://crbug.com/126152
  if (!local_address_.get()) {
    SockaddrStorage storage;
    if (getsockname(socket_, storage.addr, &storage.addr_len))
      return MapSystemError(WSAGetLastError());
    std::unique_ptr<IPEndPoint> local_address(new IPEndPoint());
    if (!local_address->FromSockAddr(storage.addr, storage.addr_len))
      return ERR_ADDRESS_INVALID;
    local_address_ = std::move(local_address);
    net_log_.AddEvent(NetLogEventType::UDP_LOCAL_ADDRESS, [&] {
      return CreateNetLogUDPConnectParams(
          *local_address_, NetworkChangeNotifier::kInvalidNetworkHandle);
    });
  }

  *address = *local_address_;
  return OK;
}

int UDPSocketWin::Read(IOBuffer* buf,
                       int buf_len,
                       CompletionOnceCallback callback) {
  return RecvFrom(buf, buf_len, nullptr, std::move(callback));
}

int UDPSocketWin::RecvFrom(IOBuffer* buf,
                           int buf_len,
                           IPEndPoint* address,
                           CompletionOnceCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_NE(INVALID_SOCKET, socket_);
  CHECK(read_callback_.is_null());
  DCHECK(!recv_from_address_);
  DCHECK(!callback.is_null());  // Synchronous operation not supported.
  DCHECK_GT(buf_len, 0);

  int nread = core_ ? InternalRecvFromOverlapped(buf, buf_len, address)
                    : InternalRecvFromNonBlocking(buf, buf_len, address);
  if (nread != ERR_IO_PENDING)
    return nread;

  read_callback_ = std::move(callback);
  recv_from_address_ = address;
  return ERR_IO_PENDING;
}

int UDPSocketWin::Write(
    IOBuffer* buf,
    int buf_len,
    CompletionOnceCallback callback,
    const NetworkTrafficAnnotationTag& /* traffic_annotation */) {
  return SendToOrWrite(buf, buf_len, remote_address_.get(),
                       std::move(callback));
}

int UDPSocketWin::SendTo(IOBuffer* buf,
                         int buf_len,
                         const IPEndPoint& address,
                         CompletionOnceCallback callback) {
  if (dscp_manager_) {
    // Alert DscpManager in case this is a new remote address.  Failure to
    // apply Dscp code is never fatal.
    int rv = dscp_manager_->PrepareForSend(address);
    if (rv != OK)
      net_log_.AddEventWithNetErrorCode(NetLogEventType::UDP_SEND_ERROR, rv);
  }
  return SendToOrWrite(buf, buf_len, &address, std::move(callback));
}

int UDPSocketWin::SendToOrWrite(IOBuffer* buf,
                                int buf_len,
                                const IPEndPoint* address,
                                CompletionOnceCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_NE(INVALID_SOCKET, socket_);
  CHECK(write_callback_.is_null());
  DCHECK(!callback.is_null());  // Synchronous operation not supported.
  DCHECK_GT(buf_len, 0);
  DCHECK(!send_to_address_.get());

  int nwrite = core_ ? InternalSendToOverlapped(buf, buf_len, address)
                     : InternalSendToNonBlocking(buf, buf_len, address);
  if (nwrite != ERR_IO_PENDING)
    return nwrite;

  if (address)
    send_to_address_.reset(new IPEndPoint(*address));
  write_callback_ = std::move(callback);
  return ERR_IO_PENDING;
}

int UDPSocketWin::Connect(const IPEndPoint& address) {
  DCHECK_NE(socket_, INVALID_SOCKET);
  net_log_.BeginEvent(NetLogEventType::UDP_CONNECT, [&] {
    return CreateNetLogUDPConnectParams(
        address, NetworkChangeNotifier::kInvalidNetworkHandle);
  });
  int rv = SetMulticastOptions();
  if (rv != OK)
    return rv;
  rv = InternalConnect(address);
  net_log_.EndEventWithNetErrorCode(NetLogEventType::UDP_CONNECT, rv);
  is_connected_ = (rv == OK);
  return rv;
}

int UDPSocketWin::InternalConnect(const IPEndPoint& address) {
  DCHECK(!is_connected());
  DCHECK(!remote_address_.get());

  int rv = 0;
  if (bind_type_ == DatagramSocket::RANDOM_BIND) {
    // Construct IPAddress of appropriate size (IPv4 or IPv6) of 0s,
    // representing INADDR_ANY or in6addr_any.
    size_t addr_size = (address.GetSockAddrFamily() == AF_INET)
                           ? IPAddress::kIPv4AddressSize
                           : IPAddress::kIPv6AddressSize;
    rv = RandomBind(IPAddress::AllZeros(addr_size));
  }
  // else connect() does the DatagramSocket::DEFAULT_BIND

  if (rv < 0) {
    base::UmaHistogramSparse("Net.UdpSocketRandomBindErrorCode", -rv);
    return rv;
  }

  SockaddrStorage storage;
  if (!address.ToSockAddr(storage.addr, &storage.addr_len))
    return ERR_ADDRESS_INVALID;

  rv = connect(socket_, storage.addr, storage.addr_len);
  if (rv < 0)
    return MapSystemError(WSAGetLastError());

  remote_address_.reset(new IPEndPoint(address));

  if (dscp_manager_)
    dscp_manager_->PrepareForSend(*remote_address_.get());

  return rv;
}

int UDPSocketWin::Bind(const IPEndPoint& address) {
  DCHECK_NE(socket_, INVALID_SOCKET);
  DCHECK(!is_connected());

  int rv = SetMulticastOptions();
  if (rv < 0)
    return rv;

  rv = DoBind(address);
  if (rv < 0)
    return rv;

  local_address_.reset();
  is_connected_ = true;
  return rv;
}

int UDPSocketWin::BindToNetwork(NetworkChangeNotifier::NetworkHandle network) {
  NOTIMPLEMENTED();
  return ERR_NOT_IMPLEMENTED;
}

int UDPSocketWin::SetReceiveBufferSize(int32_t size) {
  DCHECK_NE(socket_, INVALID_SOCKET);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  int rv = SetSocketReceiveBufferSize(socket_, size);

  if (rv != 0)
    return MapSystemError(WSAGetLastError());

  // According to documentation, setsockopt may succeed, but we need to check
  // the results via getsockopt to be sure it works on Windows.
  int32_t actual_size = 0;
  int option_size = sizeof(actual_size);
  rv = getsockopt(socket_, SOL_SOCKET, SO_RCVBUF,
                  reinterpret_cast<char*>(&actual_size), &option_size);
  if (rv != 0)
    return MapSystemError(WSAGetLastError());
  if (actual_size >= size)
    return OK;
  UMA_HISTOGRAM_CUSTOM_COUNTS("Net.SocketUnchangeableReceiveBuffer",
                              actual_size, 1000, 1000000, 50);
  return ERR_SOCKET_RECEIVE_BUFFER_SIZE_UNCHANGEABLE;
}

int UDPSocketWin::SetSendBufferSize(int32_t size) {
  DCHECK_NE(socket_, INVALID_SOCKET);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  int rv = SetSocketSendBufferSize(socket_, size);
  if (rv != 0)
    return MapSystemError(WSAGetLastError());
  // According to documentation, setsockopt may succeed, but we need to check
  // the results via getsockopt to be sure it works on Windows.
  int32_t actual_size = 0;
  int option_size = sizeof(actual_size);
  rv = getsockopt(socket_, SOL_SOCKET, SO_SNDBUF,
                  reinterpret_cast<char*>(&actual_size), &option_size);
  if (rv != 0)
    return MapSystemError(WSAGetLastError());
  if (actual_size >= size)
    return OK;
  UMA_HISTOGRAM_CUSTOM_COUNTS("Net.SocketUnchangeableSendBuffer",
                              actual_size, 1000, 1000000, 50);
  return ERR_SOCKET_SEND_BUFFER_SIZE_UNCHANGEABLE;
}

int UDPSocketWin::SetDoNotFragment() {
  DCHECK_NE(socket_, INVALID_SOCKET);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (addr_family_ == AF_INET6)
    return OK;

  DWORD val = 1;
  int rv = setsockopt(socket_, IPPROTO_IP, IP_DONTFRAGMENT,
                      reinterpret_cast<const char*>(&val), sizeof(val));
  return rv == 0 ? OK : MapSystemError(WSAGetLastError());
}

void UDPSocketWin::SetMsgConfirm(bool confirm) {}

int UDPSocketWin::AllowAddressReuse() {
  DCHECK_NE(socket_, INVALID_SOCKET);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!is_connected());

  BOOL true_value = TRUE;
  int rv = setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR,
                      reinterpret_cast<const char*>(&true_value),
                      sizeof(true_value));
  return rv == 0 ? OK : MapSystemError(WSAGetLastError());
}

int UDPSocketWin::SetBroadcast(bool broadcast) {
  DCHECK_NE(socket_, INVALID_SOCKET);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  BOOL value = broadcast ? TRUE : FALSE;
  int rv = setsockopt(socket_, SOL_SOCKET, SO_BROADCAST,
                      reinterpret_cast<const char*>(&value), sizeof(value));
  return rv == 0 ? OK : MapSystemError(WSAGetLastError());
}

int UDPSocketWin::AllowAddressSharingForMulticast() {
  // When proper multicast groups are used, Windows further defines the address
  // resuse option (SO_REUSEADDR) to ensure all listening sockets can receive
  // all incoming messages for the multicast group.
  return AllowAddressReuse();
}

void UDPSocketWin::DoReadCallback(int rv) {
  DCHECK_NE(rv, ERR_IO_PENDING);
  DCHECK(!read_callback_.is_null());

  // since Run may result in Read being called, clear read_callback_ up front.
  std::move(read_callback_).Run(rv);
}

void UDPSocketWin::DoWriteCallback(int rv) {
  DCHECK_NE(rv, ERR_IO_PENDING);
  DCHECK(!write_callback_.is_null());

  // since Run may result in Write being called, clear write_callback_ up front.
  std::move(write_callback_).Run(rv);
}

void UDPSocketWin::DidCompleteRead() {
  DWORD num_bytes, flags;
  BOOL ok = WSAGetOverlappedResult(socket_, &core_->read_overlapped_,
                                   &num_bytes, FALSE, &flags);
  WSAResetEvent(core_->read_overlapped_.hEvent);
  int result = ok ? num_bytes : MapSystemError(WSAGetLastError());
  // Convert address.
  IPEndPoint address;
  IPEndPoint* address_to_log = nullptr;
  if (result >= 0) {
    if (address.FromSockAddr(core_->recv_addr_storage_.addr,
                             core_->recv_addr_storage_.addr_len)) {
      if (recv_from_address_)
        *recv_from_address_ = address;
      address_to_log = &address;
    } else {
      result = ERR_ADDRESS_INVALID;
    }
  }
  LogRead(result, core_->read_iobuffer_->data(), address_to_log);
  core_->read_iobuffer_ = nullptr;
  recv_from_address_ = nullptr;
  DoReadCallback(result);
}

void UDPSocketWin::DidCompleteWrite() {
  DWORD num_bytes, flags;
  BOOL ok = WSAGetOverlappedResult(socket_, &core_->write_overlapped_,
                                   &num_bytes, FALSE, &flags);
  WSAResetEvent(core_->write_overlapped_.hEvent);
  int result = ok ? num_bytes : MapSystemError(WSAGetLastError());
  LogWrite(result, core_->write_iobuffer_->data(), send_to_address_.get());

  send_to_address_.reset();
  core_->write_iobuffer_ = nullptr;
  DoWriteCallback(result);
}

void UDPSocketWin::OnObjectSignaled(HANDLE object) {
  DCHECK(object == read_write_event_.Get());
  WSANETWORKEVENTS network_events;
  int os_error = 0;
  int rv =
      WSAEnumNetworkEvents(socket_, read_write_event_.Get(), &network_events);
  // Protects against trying to call the write callback if the read callback
  // either closes or destroys |this|.
  base::WeakPtr<UDPSocketWin> event_pending = event_pending_.GetWeakPtr();
  if (rv == SOCKET_ERROR) {
    os_error = WSAGetLastError();
    rv = MapSystemError(os_error);

    if (read_iobuffer_) {
      read_iobuffer_ = nullptr;
      read_iobuffer_len_ = 0;
      recv_from_address_ = nullptr;
      DoReadCallback(rv);
    }

    // Socket may have been closed or destroyed here.
    if (event_pending && write_iobuffer_) {
      write_iobuffer_ = nullptr;
      write_iobuffer_len_ = 0;
      send_to_address_.reset();
      DoWriteCallback(rv);
    }
    return;
  }

  if ((network_events.lNetworkEvents & FD_READ) && read_iobuffer_)
    OnReadSignaled();
  if (!event_pending)
    return;

  if ((network_events.lNetworkEvents & FD_WRITE) && write_iobuffer_)
    OnWriteSignaled();
  if (!event_pending)
    return;

  // There's still pending read / write. Watch for further events.
  if (read_iobuffer_ || write_iobuffer_)
    WatchForReadWrite();
}

void UDPSocketWin::OnReadSignaled() {
  int rv = InternalRecvFromNonBlocking(read_iobuffer_.get(), read_iobuffer_len_,
                                       recv_from_address_);
  if (rv == ERR_IO_PENDING)
    return;
  read_iobuffer_ = nullptr;
  read_iobuffer_len_ = 0;
  recv_from_address_ = nullptr;
  DoReadCallback(rv);
}

void UDPSocketWin::OnWriteSignaled() {
  int rv = InternalSendToNonBlocking(write_iobuffer_.get(), write_iobuffer_len_,
                                     send_to_address_.get());
  if (rv == ERR_IO_PENDING)
    return;
  write_iobuffer_ = nullptr;
  write_iobuffer_len_ = 0;
  send_to_address_.reset();
  DoWriteCallback(rv);
}

void UDPSocketWin::WatchForReadWrite() {
  if (read_write_watcher_.IsWatching())
    return;
  bool watched =
      read_write_watcher_.StartWatchingOnce(read_write_event_.Get(), this);
  DCHECK(watched);
}

void UDPSocketWin::LogRead(int result,
                           const char* bytes,
                           const IPEndPoint* address) const {
  if (result < 0) {
    net_log_.AddEventWithNetErrorCode(NetLogEventType::UDP_RECEIVE_ERROR,
                                      result);
    return;
  }

  if (net_log_.IsCapturing()) {
    NetLogUDPDataTransfer(net_log_, NetLogEventType::UDP_BYTES_RECEIVED, result,
                          bytes, address);
  }

  NetworkActivityMonitor::GetInstance()->IncrementBytesReceived(result);
}

void UDPSocketWin::LogWrite(int result,
                            const char* bytes,
                            const IPEndPoint* address) const {
  if (result < 0) {
    net_log_.AddEventWithNetErrorCode(NetLogEventType::UDP_SEND_ERROR, result);
    return;
  }

  if (net_log_.IsCapturing()) {
    NetLogUDPDataTransfer(net_log_, NetLogEventType::UDP_BYTES_SENT, result,
                          bytes, address);
  }

  NetworkActivityMonitor::GetInstance()->IncrementBytesSent(result);
}

int UDPSocketWin::InternalRecvFromOverlapped(IOBuffer* buf,
                                             int buf_len,
                                             IPEndPoint* address) {
  DCHECK(!core_->read_iobuffer_.get());
  SockaddrStorage& storage = core_->recv_addr_storage_;
  storage.addr_len = sizeof(storage.addr_storage);

  WSABUF read_buffer;
  read_buffer.buf = buf->data();
  read_buffer.len = buf_len;

  DWORD flags = 0;
  DWORD num;
  CHECK_NE(INVALID_SOCKET, socket_);
  AssertEventNotSignaled(core_->read_overlapped_.hEvent);
  int rv = WSARecvFrom(socket_, &read_buffer, 1, &num, &flags, storage.addr,
                       &storage.addr_len, &core_->read_overlapped_, nullptr);
  if (rv == 0) {
    if (ResetEventIfSignaled(core_->read_overlapped_.hEvent)) {
      int result = num;
      // Convert address.
      IPEndPoint address_storage;
      IPEndPoint* address_to_log = nullptr;
      if (result >= 0) {
        if (address_storage.FromSockAddr(core_->recv_addr_storage_.addr,
                                         core_->recv_addr_storage_.addr_len)) {
          if (address)
            *address = address_storage;
          address_to_log = &address_storage;
        } else {
          result = ERR_ADDRESS_INVALID;
        }
      }
      LogRead(result, buf->data(), address_to_log);
      return result;
    }
  } else {
    int os_error = WSAGetLastError();
    if (os_error != WSA_IO_PENDING) {
      int result = MapSystemError(os_error);
      LogRead(result, nullptr, nullptr);
      return result;
    }
  }
  core_->WatchForRead();
  core_->read_iobuffer_ = buf;
  return ERR_IO_PENDING;
}

int UDPSocketWin::InternalSendToOverlapped(IOBuffer* buf,
                                           int buf_len,
                                           const IPEndPoint* address) {
  DCHECK(!core_->write_iobuffer_.get());
  SockaddrStorage storage;
  struct sockaddr* addr = storage.addr;
  // Convert address.
  if (!address) {
    addr = nullptr;
    storage.addr_len = 0;
  } else {
    if (!address->ToSockAddr(addr, &storage.addr_len)) {
      int result = ERR_ADDRESS_INVALID;
      LogWrite(result, nullptr, nullptr);
      return result;
    }
  }

  WSABUF write_buffer;
  write_buffer.buf = buf->data();
  write_buffer.len = buf_len;

  DWORD flags = 0;
  DWORD num;
  AssertEventNotSignaled(core_->write_overlapped_.hEvent);
  int rv = WSASendTo(socket_, &write_buffer, 1, &num, flags, addr,
                     storage.addr_len, &core_->write_overlapped_, nullptr);
  if (rv == 0) {
    if (ResetEventIfSignaled(core_->write_overlapped_.hEvent)) {
      int result = num;
      LogWrite(result, buf->data(), address);
      return result;
    }
  } else {
    int os_error = WSAGetLastError();
    if (os_error != WSA_IO_PENDING) {
      int result = MapSystemError(os_error);
      LogWrite(result, nullptr, nullptr);
      return result;
    }
  }

  core_->WatchForWrite();
  core_->write_iobuffer_ = buf;
  return ERR_IO_PENDING;
}

int UDPSocketWin::InternalRecvFromNonBlocking(IOBuffer* buf,
                                              int buf_len,
                                              IPEndPoint* address) {
  DCHECK(!read_iobuffer_ || read_iobuffer_.get() == buf);
  SockaddrStorage storage;
  storage.addr_len = sizeof(storage.addr_storage);

  CHECK_NE(INVALID_SOCKET, socket_);
  int rv = recvfrom(socket_, buf->data(), buf_len, 0, storage.addr,
                    &storage.addr_len);
  if (rv == SOCKET_ERROR) {
    int os_error = WSAGetLastError();
    if (os_error == WSAEWOULDBLOCK) {
      read_iobuffer_ = buf;
      read_iobuffer_len_ = buf_len;
      WatchForReadWrite();
      return ERR_IO_PENDING;
    }
    rv = MapSystemError(os_error);
    LogRead(rv, nullptr, nullptr);
    return rv;
  }
  IPEndPoint address_storage;
  IPEndPoint* address_to_log = nullptr;
  if (rv >= 0) {
    if (address_storage.FromSockAddr(storage.addr, storage.addr_len)) {
      if (address)
        *address = address_storage;
      address_to_log = &address_storage;
    } else {
      rv = ERR_ADDRESS_INVALID;
    }
  }
  LogRead(rv, buf->data(), address_to_log);
  return rv;
}

int UDPSocketWin::InternalSendToNonBlocking(IOBuffer* buf,
                                            int buf_len,
                                            const IPEndPoint* address) {
  DCHECK(!write_iobuffer_ || write_iobuffer_.get() == buf);
  SockaddrStorage storage;
  struct sockaddr* addr = storage.addr;
  // Convert address.
  if (address) {
    if (!address->ToSockAddr(addr, &storage.addr_len)) {
      int result = ERR_ADDRESS_INVALID;
      LogWrite(result, nullptr, nullptr);
      return result;
    }
  } else {
    addr = nullptr;
    storage.addr_len = 0;
  }

  int rv = sendto(socket_, buf->data(), buf_len, 0, addr, storage.addr_len);
  if (rv == SOCKET_ERROR) {
    int os_error = WSAGetLastError();
    if (os_error == WSAEWOULDBLOCK) {
      write_iobuffer_ = buf;
      write_iobuffer_len_ = buf_len;
      WatchForReadWrite();
      return ERR_IO_PENDING;
    }
    rv = MapSystemError(os_error);
    LogWrite(rv, nullptr, nullptr);
    return rv;
  }
  LogWrite(rv, buf->data(), address);
  return rv;
}

int UDPSocketWin::SetMulticastOptions() {
  if (!(socket_options_ & SOCKET_OPTION_MULTICAST_LOOP)) {
    DWORD loop = 0;
    int protocol_level =
        addr_family_ == AF_INET ? IPPROTO_IP : IPPROTO_IPV6;
    int option =
        addr_family_ == AF_INET ? IP_MULTICAST_LOOP: IPV6_MULTICAST_LOOP;
    int rv = setsockopt(socket_, protocol_level, option,
                        reinterpret_cast<const char*>(&loop), sizeof(loop));
    if (rv < 0)
      return MapSystemError(WSAGetLastError());
  }
  if (multicast_time_to_live_ != 1) {
    DWORD hops = multicast_time_to_live_;
    int protocol_level =
        addr_family_ == AF_INET ? IPPROTO_IP : IPPROTO_IPV6;
    int option =
        addr_family_ == AF_INET ? IP_MULTICAST_TTL: IPV6_MULTICAST_HOPS;
    int rv = setsockopt(socket_, protocol_level, option,
                        reinterpret_cast<const char*>(&hops), sizeof(hops));
    if (rv < 0)
      return MapSystemError(WSAGetLastError());
  }
  if (multicast_interface_ != 0) {
    switch (addr_family_) {
      case AF_INET: {
        in_addr address;
        address.s_addr = htonl(multicast_interface_);
        int rv = setsockopt(socket_, IPPROTO_IP, IP_MULTICAST_IF,
                            reinterpret_cast<const char*>(&address),
                            sizeof(address));
        if (rv)
          return MapSystemError(WSAGetLastError());
        break;
      }
      case AF_INET6: {
        uint32_t interface_index = multicast_interface_;
        int rv = setsockopt(socket_, IPPROTO_IPV6, IPV6_MULTICAST_IF,
                            reinterpret_cast<const char*>(&interface_index),
                            sizeof(interface_index));
        if (rv)
          return MapSystemError(WSAGetLastError());
        break;
      }
      default:
        NOTREACHED() << "Invalid address family";
        return ERR_ADDRESS_INVALID;
    }
  }
  return OK;
}

int UDPSocketWin::DoBind(const IPEndPoint& address) {
  SockaddrStorage storage;
  if (!address.ToSockAddr(storage.addr, &storage.addr_len))
    return ERR_ADDRESS_INVALID;
  int rv = bind(socket_, storage.addr, storage.addr_len);
  if (rv == 0)
    return OK;
  int last_error = WSAGetLastError();
  // Map some codes that are special to bind() separately.
  // * WSAEACCES: If a port is already bound to a socket, WSAEACCES may be
  //   returned instead of WSAEADDRINUSE, depending on whether the socket
  //   option SO_REUSEADDR or SO_EXCLUSIVEADDRUSE is set and whether the
  //   conflicting socket is owned by a different user account. See the MSDN
  //   page "Using SO_REUSEADDR and SO_EXCLUSIVEADDRUSE" for the gory details.
  if (last_error == WSAEACCES || last_error == WSAEADDRNOTAVAIL)
    return ERR_ADDRESS_IN_USE;
  return MapSystemError(last_error);
}

int UDPSocketWin::RandomBind(const IPAddress& address) {
  DCHECK_EQ(bind_type_, DatagramSocket::RANDOM_BIND);

  for (int i = 0; i < kBindRetries; ++i) {
    int rv = DoBind(IPEndPoint(
        address, static_cast<uint16_t>(base::RandInt(kPortStart, kPortEnd))));
    if (rv != ERR_ADDRESS_IN_USE)
      return rv;
  }
  return DoBind(IPEndPoint(address, 0));
}

QwaveApi* UDPSocketWin::GetQwaveApi() const {
  return QwaveApi::GetDefault();
}

int UDPSocketWin::JoinGroup(const IPAddress& group_address) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!is_connected())
    return ERR_SOCKET_NOT_CONNECTED;

  switch (group_address.size()) {
    case IPAddress::kIPv4AddressSize: {
      if (addr_family_ != AF_INET)
        return ERR_ADDRESS_INVALID;
      ip_mreq mreq;
      mreq.imr_interface.s_addr = htonl(multicast_interface_);
      memcpy(&mreq.imr_multiaddr, group_address.bytes().data(),
             IPAddress::kIPv4AddressSize);
      int rv = setsockopt(socket_, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                          reinterpret_cast<const char*>(&mreq),
                          sizeof(mreq));
      if (rv)
        return MapSystemError(WSAGetLastError());
      return OK;
    }
    case IPAddress::kIPv6AddressSize: {
      if (addr_family_ != AF_INET6)
        return ERR_ADDRESS_INVALID;
      ipv6_mreq mreq;
      mreq.ipv6mr_interface = multicast_interface_;
      memcpy(&mreq.ipv6mr_multiaddr, group_address.bytes().data(),
             IPAddress::kIPv6AddressSize);
      int rv = setsockopt(socket_, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP,
                          reinterpret_cast<const char*>(&mreq),
                          sizeof(mreq));
      if (rv)
        return MapSystemError(WSAGetLastError());
      return OK;
    }
    default:
      NOTREACHED() << "Invalid address family";
      return ERR_ADDRESS_INVALID;
  }
}

int UDPSocketWin::LeaveGroup(const IPAddress& group_address) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!is_connected())
    return ERR_SOCKET_NOT_CONNECTED;

  switch (group_address.size()) {
    case IPAddress::kIPv4AddressSize: {
      if (addr_family_ != AF_INET)
        return ERR_ADDRESS_INVALID;
      ip_mreq mreq;
      mreq.imr_interface.s_addr = htonl(multicast_interface_);
      memcpy(&mreq.imr_multiaddr, group_address.bytes().data(),
             IPAddress::kIPv4AddressSize);
      int rv = setsockopt(socket_, IPPROTO_IP, IP_DROP_MEMBERSHIP,
                          reinterpret_cast<const char*>(&mreq), sizeof(mreq));
      if (rv)
        return MapSystemError(WSAGetLastError());
      return OK;
    }
    case IPAddress::kIPv6AddressSize: {
      if (addr_family_ != AF_INET6)
        return ERR_ADDRESS_INVALID;
      ipv6_mreq mreq;
      mreq.ipv6mr_interface = multicast_interface_;
      memcpy(&mreq.ipv6mr_multiaddr, group_address.bytes().data(),
             IPAddress::kIPv6AddressSize);
      int rv = setsockopt(socket_, IPPROTO_IPV6, IP_DROP_MEMBERSHIP,
                          reinterpret_cast<const char*>(&mreq), sizeof(mreq));
      if (rv)
        return MapSystemError(WSAGetLastError());
      return OK;
    }
    default:
      NOTREACHED() << "Invalid address family";
      return ERR_ADDRESS_INVALID;
  }
}

int UDPSocketWin::SetMulticastInterface(uint32_t interface_index) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (is_connected())
    return ERR_SOCKET_IS_CONNECTED;
  multicast_interface_ = interface_index;
  return OK;
}

int UDPSocketWin::SetMulticastTimeToLive(int time_to_live) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (is_connected())
    return ERR_SOCKET_IS_CONNECTED;

  if (time_to_live < 0 || time_to_live > 255)
    return ERR_INVALID_ARGUMENT;
  multicast_time_to_live_ = time_to_live;
  return OK;
}

int UDPSocketWin::SetMulticastLoopbackMode(bool loopback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (is_connected())
    return ERR_SOCKET_IS_CONNECTED;

  if (loopback)
    socket_options_ |= SOCKET_OPTION_MULTICAST_LOOP;
  else
    socket_options_ &= ~SOCKET_OPTION_MULTICAST_LOOP;
  return OK;
}

QOS_TRAFFIC_TYPE DscpToTrafficType(DiffServCodePoint dscp) {
  QOS_TRAFFIC_TYPE traffic_type = QOSTrafficTypeBestEffort;
  switch (dscp) {
    case DSCP_CS0:
      traffic_type = QOSTrafficTypeBestEffort;
      break;
    case DSCP_CS1:
      traffic_type = QOSTrafficTypeBackground;
      break;
    case DSCP_AF11:
    case DSCP_AF12:
    case DSCP_AF13:
    case DSCP_CS2:
    case DSCP_AF21:
    case DSCP_AF22:
    case DSCP_AF23:
    case DSCP_CS3:
    case DSCP_AF31:
    case DSCP_AF32:
    case DSCP_AF33:
    case DSCP_CS4:
      traffic_type = QOSTrafficTypeExcellentEffort;
      break;
    case DSCP_AF41:
    case DSCP_AF42:
    case DSCP_AF43:
    case DSCP_CS5:
      traffic_type = QOSTrafficTypeAudioVideo;
      break;
    case DSCP_EF:
    case DSCP_CS6:
      traffic_type = QOSTrafficTypeVoice;
      break;
    case DSCP_CS7:
      traffic_type = QOSTrafficTypeControl;
      break;
    case DSCP_NO_CHANGE:
      NOTREACHED();
      break;
  }
  return traffic_type;
}

int UDPSocketWin::SetDiffServCodePoint(DiffServCodePoint dscp) {
  if (dscp == DSCP_NO_CHANGE)
    return OK;

  if (!is_connected())
    return ERR_SOCKET_NOT_CONNECTED;

  QwaveApi* api = GetQwaveApi();

  if (!api->qwave_supported())
    return ERR_NOT_IMPLEMENTED;

  if (!dscp_manager_)
    dscp_manager_ = std::make_unique<DscpManager>(api, socket_);

  dscp_manager_->Set(dscp);
  if (remote_address_)
    return dscp_manager_->PrepareForSend(*remote_address_.get());

  return OK;
}

void UDPSocketWin::DetachFromThread() {
  DETACH_FROM_THREAD(thread_checker_);
}

void UDPSocketWin::UseNonBlockingIO() {
  DCHECK(!core_);
  use_non_blocking_io_ = true;
}

void UDPSocketWin::ApplySocketTag(const SocketTag& tag) {
  // Windows does not support any specific SocketTags so fail if any non-default
  // tag is applied.
  CHECK(tag == SocketTag());
}

void UDPSocketWin::SetWriteAsyncEnabled(bool enabled) {}
bool UDPSocketWin::WriteAsyncEnabled() {
  return false;
}
void UDPSocketWin::SetMaxPacketSize(size_t max_packet_size) {}
void UDPSocketWin::SetWriteMultiCoreEnabled(bool enabled) {}
void UDPSocketWin::SetSendmmsgEnabled(bool enabled) {}
void UDPSocketWin::SetWriteBatchingActive(bool active) {}

int UDPSocketWin::WriteAsync(
    DatagramBuffers buffers,
    CompletionOnceCallback callback,
    const NetworkTrafficAnnotationTag& traffic_annotation) {
  NOTIMPLEMENTED();
  return ERR_NOT_IMPLEMENTED;
}

int UDPSocketWin::WriteAsync(
    const char* buffer,
    size_t buf_len,
    CompletionOnceCallback callback,
    const NetworkTrafficAnnotationTag& traffic_annotation) {
  NOTIMPLEMENTED();
  return ERR_NOT_IMPLEMENTED;
}

DatagramBuffers UDPSocketWin::GetUnwrittenBuffers() {
  DatagramBuffers result;
  NOTIMPLEMENTED();
  return result;
}
DscpManager::DscpManager(QwaveApi* api, SOCKET socket)
    : api_(api), socket_(socket) {
  RequestHandle();
}

DscpManager::~DscpManager() {
  if (!qos_handle_)
    return;

  if (flow_id_ != 0)
    api_->RemoveSocketFromFlow(qos_handle_, NULL, flow_id_, 0);

  api_->CloseHandle(qos_handle_);
}

void DscpManager::Set(DiffServCodePoint dscp) {
  if (dscp == DSCP_NO_CHANGE || dscp == dscp_value_)
    return;

  dscp_value_ = dscp;

  // TODO(zstein): We could reuse the flow when the value changes
  // by calling QOSSetFlow with the new traffic type and dscp value.
  if (flow_id_ != 0 && qos_handle_) {
    api_->RemoveSocketFromFlow(qos_handle_, NULL, flow_id_, 0);
    configured_.clear();
    flow_id_ = 0;
  }
}

int DscpManager::PrepareForSend(const IPEndPoint& remote_address) {
  if (dscp_value_ == DSCP_NO_CHANGE) {
    // No DSCP value has been set.
    return OK;
  }

  if (!api_->qwave_supported())
    return ERR_NOT_IMPLEMENTED;

  if (!qos_handle_)
    return ERR_INVALID_HANDLE;  // The closest net error to try again later.

  if (configured_.find(remote_address) != configured_.end())
    return OK;

  SockaddrStorage storage;
  if (!remote_address.ToSockAddr(storage.addr, &storage.addr_len))
    return ERR_ADDRESS_INVALID;

  // We won't try this address again if we get an error.
  configured_.emplace(remote_address);

  // We don't need to call SetFlow if we already have a qos flow.
  bool new_flow = flow_id_ == 0;

  const QOS_TRAFFIC_TYPE traffic_type = DscpToTrafficType(dscp_value_);

  if (!api_->AddSocketToFlow(qos_handle_, socket_, storage.addr, traffic_type,
                             QOS_NON_ADAPTIVE_FLOW, &flow_id_)) {
    DWORD err = ::GetLastError();
    if (err == ERROR_DEVICE_REINITIALIZATION_NEEDED) {
      // Reset. PrepareForSend is called for every packet.  Once RequestHandle
      // completes asynchronously the next PrepareForSend call will re-register
      // the address with the new QoS Handle.  In the meantime, sends will
      // continue without DSCP.
      RequestHandle();
      configured_.clear();
      flow_id_ = 0;
      return ERR_INVALID_HANDLE;
    }
    return MapSystemError(err);
  }

  if (new_flow) {
    DWORD buf = dscp_value_;
    // This requires admin rights, and may fail, if so we ignore it
    // as AddSocketToFlow should still do *approximately* the right thing.
    api_->SetFlow(qos_handle_, flow_id_, QOSSetOutgoingDSCPValue, sizeof(buf),
                  &buf, 0, nullptr);
  }

  return OK;
}

void DscpManager::RequestHandle() {
  if (handle_is_initializing_)
    return;

  if (qos_handle_) {
    api_->CloseHandle(qos_handle_);
    qos_handle_ = nullptr;
  }

  handle_is_initializing_ = true;
  base::PostTaskAndReplyWithResult(
      FROM_HERE, {base::ThreadPool(), base::MayBlock()},
      base::BindOnce(&DscpManager::DoCreateHandle, api_),
      base::BindOnce(&DscpManager::OnHandleCreated, api_,
                     weak_ptr_factory_.GetWeakPtr()));
}

HANDLE DscpManager::DoCreateHandle(QwaveApi* api) {
  QOS_VERSION version;
  version.MajorVersion = 1;
  version.MinorVersion = 0;

  HANDLE handle = nullptr;

  // No access to net_log_ so swallow any errors here.
  api->CreateHandle(&version, &handle);
  return handle;
}

void DscpManager::OnHandleCreated(QwaveApi* api,
                                  base::WeakPtr<DscpManager> dscp_manager,
                                  HANDLE handle) {
  if (!handle)
    api->OnFatalError();

  if (!dscp_manager) {
    api->CloseHandle(handle);
    return;
  }

  DCHECK(dscp_manager->handle_is_initializing_);
  DCHECK(!dscp_manager->qos_handle_);

  dscp_manager->qos_handle_ = handle;
  dscp_manager->handle_is_initializing_ = false;
}

}  // namespace net
