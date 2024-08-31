// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/socket/udp_socket_win.h"

#include <winsock2.h>

#include <mstcpip.h>

#include <memory>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/lazy_instance.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "base/task/thread_pool.h"
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

namespace net {

// This class encapsulates all the state that has to be preserved as long as
// there is a network IO operation in progress. If the owner UDPSocketWin
// is destroyed while an operation is in progress, the Core is detached and it
// lives until the operation completes and the OS doesn't reference any resource
// declared on this class anymore.
class UDPSocketWin::Core : public base::RefCounted<Core> {
 public:
  explicit Core(UDPSocketWin* socket);

  Core(const Core&) = delete;
  Core& operator=(const Core&) = delete;

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
  // The struct for packet metadata passed to WSARecvMsg().
  std::unique_ptr<WSAMSG> read_message_ = nullptr;
  // Big enough for IP_ECN or IPV6_ECN, nothing more.
  char read_control_buffer_[WSA_CMSG_SPACE(sizeof(int))];

  // The address storage passed to WSARecvFrom().
  SockaddrStorage recv_addr_storage_;

 private:
  friend class base::RefCounted<Core>;

  class ReadDelegate : public base::win::ObjectWatcher::Delegate {
   public:
    explicit ReadDelegate(Core* core) : core_(core) {}
    ~ReadDelegate() override = default;

    // base::ObjectWatcher::Delegate methods:
    void OnObjectSignaled(HANDLE object) override;

   private:
    const raw_ptr<Core> core_;
  };

  class WriteDelegate : public base::win::ObjectWatcher::Delegate {
   public:
    explicit WriteDelegate(Core* core) : core_(core) {}
    ~WriteDelegate() override = default;

    // base::ObjectWatcher::Delegate methods:
    void OnObjectSignaled(HANDLE object) override;

   private:
    const raw_ptr<Core> core_;
  };

  ~Core();

  // The socket that created this object.
  raw_ptr<UDPSocketWin> socket_;

  // |reader_| handles the signals from |read_watcher_|.
  ReadDelegate reader_;
  // |writer_| handles the signals from |write_watcher_|.
  WriteDelegate writer_;

  // |read_watcher_| watches for events from Read().
  base::win::ObjectWatcher read_watcher_;
  // |write_watcher_| watches for events from Write();
  base::win::ObjectWatcher write_watcher_;
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

QwaveApi::QwaveApi() {
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
      socket_options_(SOCKET_OPTION_MULTICAST_LOOP),
      net_log_(NetLogWithSource::Make(net_log, NetLogSourceType::UDP_SOCKET)) {
  EnsureWinsockInit();
  net_log_.BeginEventReferencingSource(NetLogEventType::SOCKET_ALIVE, source);
}

UDPSocketWin::UDPSocketWin(DatagramSocket::BindType bind_type,
                           NetLogWithSource source_net_log)
    : socket_(INVALID_SOCKET),
      socket_options_(SOCKET_OPTION_MULTICAST_LOOP),
      net_log_(source_net_log) {
  EnsureWinsockInit();
  net_log_.BeginEventReferencingSource(NetLogEventType::SOCKET_ALIVE,
                                       net_log_.source());
}

UDPSocketWin::~UDPSocketWin() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  Close();
  net_log_.EndEvent(NetLogEventType::SOCKET_ALIVE);
}

int UDPSocketWin::Open(AddressFamily address_family) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_EQ(socket_, INVALID_SOCKET);

  auto owned_socket_count = TryAcquireGlobalUDPSocketCount();
  if (owned_socket_count.empty())
    return ERR_INSUFFICIENT_RESOURCES;

  owned_socket_count_ = std::move(owned_socket_count);
  addr_family_ = ConvertAddressFamily(address_family);
  socket_ = CreatePlatformSocket(addr_family_, SOCK_DGRAM, IPPROTO_UDP);
  if (socket_ == INVALID_SOCKET) {
    owned_socket_count_.Reset();
    return MapSystemError(WSAGetLastError());
  }
  ConfigureOpenedSocket();
  return OK;
}

int UDPSocketWin::AdoptOpenedSocket(AddressFamily address_family,
                                    SOCKET socket) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto owned_socket_count = TryAcquireGlobalUDPSocketCount();
  if (owned_socket_count.empty()) {
    return ERR_INSUFFICIENT_RESOURCES;
  }

  owned_socket_count_ = std::move(owned_socket_count);
  addr_family_ = ConvertAddressFamily(address_family);
  socket_ = socket;
  ConfigureOpenedSocket();
  return OK;
}

void UDPSocketWin::ConfigureOpenedSocket() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!use_non_blocking_io_) {
    core_ = base::MakeRefCounted<Core>(this);
  } else {
    read_write_event_.Set(WSACreateEvent());
    WSAEventSelect(socket_, read_write_event_.Get(), FD_READ | FD_WRITE);
  }
}

void UDPSocketWin::Close() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  owned_socket_count_.Reset();

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
    auto remote_address = std::make_unique<IPEndPoint>();
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
    auto local_address = std::make_unique<IPEndPoint>();
    if (!local_address->FromSockAddr(storage.addr, storage.addr_len))
      return ERR_ADDRESS_INVALID;
    local_address_ = std::move(local_address);
    net_log_.AddEvent(NetLogEventType::UDP_LOCAL_ADDRESS, [&] {
      return CreateNetLogUDPConnectParams(*local_address_,
                                          handles::kInvalidNetworkHandle);
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
    send_to_address_ = std::make_unique<IPEndPoint>(*address);
  write_callback_ = std::move(callback);
  return ERR_IO_PENDING;
}

int UDPSocketWin::Connect(const IPEndPoint& address) {
  DCHECK_NE(socket_, INVALID_SOCKET);
  net_log_.BeginEvent(NetLogEventType::UDP_CONNECT, [&] {
    return CreateNetLogUDPConnectParams(address,
                                        handles::kInvalidNetworkHandle);
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

  // Always do a random bind.
  // Ignore failures, which may happen if the socket was already bound.
  DWORD randomize_port_value = 1;
  setsockopt(socket_, SOL_SOCKET, SO_RANDOMIZE_PORT,
             reinterpret_cast<const char*>(&randomize_port_value),
             sizeof(randomize_port_value));

  SockaddrStorage storage;
  if (!address.ToSockAddr(storage.addr, &storage.addr_len))
    return ERR_ADDRESS_INVALID;

  int rv = connect(socket_, storage.addr, storage.addr_len);
  if (rv < 0)
    return MapSystemError(WSAGetLastError());

  remote_address_ = std::make_unique<IPEndPoint>(address);

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

int UDPSocketWin::BindToNetwork(handles::NetworkHandle network) {
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

LPFN_WSARECVMSG UDPSocketWin::GetRecvMsgPointer() {
  LPFN_WSARECVMSG rv;
  GUID message_code = WSAID_WSARECVMSG;
  DWORD size;
  if (WSAIoctl(socket_, SIO_GET_EXTENSION_FUNCTION_POINTER, &message_code,
               sizeof(message_code), &rv, sizeof(rv), &size, NULL,
               NULL) == SOCKET_ERROR) {
    return nullptr;
  }
  return rv;
}

LPFN_WSASENDMSG UDPSocketWin::GetSendMsgPointer() {
  LPFN_WSASENDMSG rv;
  GUID message_code = WSAID_WSASENDMSG;
  DWORD size;
  if (WSAIoctl(socket_, SIO_GET_EXTENSION_FUNCTION_POINTER, &message_code,
               sizeof(message_code), &rv, sizeof(rv), &size, NULL,
               NULL) == SOCKET_ERROR) {
    return nullptr;
  }
  return rv;
}

int UDPSocketWin::LogAndReturnError() const {
  int result = MapSystemError(WSAGetLastError());
  LogRead(result, nullptr, nullptr);
  return result;
}

// Windows documentation recommends using WSASetRecvIPEcn(). However,
// this does not set the option for IPv4 packets on a dual-stack socket.
// It also returns an error when bound to an IPv4-mapped IPv6 address.
int UDPSocketWin::SetRecvTos() {
  DCHECK_NE(socket_, INVALID_SOCKET);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  IPEndPoint address;
  int rv = GetLocalAddress(&address);
  if (rv != OK) {
    return rv;
  }
  int v6_only = 0;
  int ecn = 1;
  if (addr_family_ == AF_INET6 && !address.address().IsIPv4MappedIPv6()) {
    rv = setsockopt(socket_, IPPROTO_IPV6, IPV6_RECVECN,
                    reinterpret_cast<const char*>(&ecn), sizeof(ecn));
    if (rv != 0) {
      return LogAndReturnError();
    }
    if (!address.address().IsZero()) {
      // If a socket is bound to an address besides IPV6_ANY, it won't receive
      // any v4 packets, and therefore is not truly dual-stack.
      v6_only = 1;
    } else {
      int option_size = sizeof(v6_only);
      rv = getsockopt(socket_, IPPROTO_IPV6, IPV6_V6ONLY,
                      reinterpret_cast<char*>(&v6_only), &option_size);
      if (rv != 0) {
        return LogAndReturnError();
      }
    }
  }
  if (v6_only == 0) {
    rv = setsockopt(socket_, IPPROTO_IP, IP_RECVECN,
                    reinterpret_cast<const char*>(&ecn), sizeof(ecn));
    if (rv != 0) {
      return LogAndReturnError();
    }
  }
  wsa_recv_msg_ = GetRecvMsgPointer();
  if (wsa_recv_msg_ == nullptr) {
    return LogAndReturnError();
  }
  report_ecn_ = true;
  return 0;
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
  // When proper multicast groups are used, Windows further defines the
  // address reuse option (SO_REUSEADDR) to ensure all listening sockets can
  // receive all incoming messages for the multicast group.
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

  // since Run may result in Write being called, clear write_callback_ up
  // front.
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
      if (recv_from_address_) {
        *recv_from_address_ = address;
      }
      address_to_log = &address;
    } else {
      result = ERR_ADDRESS_INVALID;
    }
    if (core_->read_message_ != nullptr) {
      SetLastTosFromWSAMSG(*core_->read_message_);
    }
  }
  LogRead(result, core_->read_iobuffer_->data(), address_to_log);
  core_->read_iobuffer_ = nullptr;
  core_->read_message_ = nullptr;
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

  if ((network_events.lNetworkEvents & FD_READ) && read_iobuffer_) {
    OnReadSignaled();
  }
  if (!event_pending) {
    return;
  }

  if ((network_events.lNetworkEvents & FD_WRITE) && write_iobuffer_) {
    OnWriteSignaled();
  }
  if (!event_pending) {
    return;
  }

  // There's still pending read / write. Watch for further events.
  if (read_iobuffer_ || write_iobuffer_) {
    WatchForReadWrite();
  }
}

void UDPSocketWin::OnReadSignaled() {
  int rv = InternalRecvFromNonBlocking(read_iobuffer_.get(), read_iobuffer_len_,
                                       recv_from_address_);
  if (rv == ERR_IO_PENDING) {
    return;
  }
  read_iobuffer_ = nullptr;
  read_iobuffer_len_ = 0;
  recv_from_address_ = nullptr;
  DoReadCallback(rv);
}

void UDPSocketWin::OnWriteSignaled() {
  int rv = InternalSendToNonBlocking(write_iobuffer_.get(), write_iobuffer_len_,
                                     send_to_address_.get());
  if (rv == ERR_IO_PENDING) {
    return;
  }
  write_iobuffer_ = nullptr;
  write_iobuffer_len_ = 0;
  send_to_address_.reset();
  DoWriteCallback(rv);
}

void UDPSocketWin::WatchForReadWrite() {
  if (read_write_watcher_.IsWatching()) {
    return;
  }
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

  activity_monitor::IncrementBytesReceived(result);
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
}

void UDPSocketWin::PopulateWSAMSG(WSAMSG& message,
                                  SockaddrStorage& storage,
                                  WSABUF* data_buffer,
                                  WSABUF& control_buffer,
                                  bool send) {
  bool is_ipv6;
  if (send && remote_address_.get() != nullptr) {
    is_ipv6 = (remote_address_->GetSockAddrFamily() == AF_INET6);
  } else {
    is_ipv6 = (addr_family_ == AF_INET6);
  }
  message.name = storage.addr;
  message.namelen = storage.addr_len;
  message.lpBuffers = data_buffer;
  message.dwBufferCount = 1;
  message.Control.buf = control_buffer.buf;
  message.dwFlags = 0;
  if (send) {
    message.Control.len = 0;
    WSACMSGHDR* cmsg;
    message.Control.len += WSA_CMSG_SPACE(sizeof(int));
    cmsg = WSA_CMSG_FIRSTHDR(&message);
    cmsg->cmsg_len = WSA_CMSG_LEN(sizeof(int));
    cmsg->cmsg_level = is_ipv6 ? IPPROTO_IPV6 : IPPROTO_IP;
    cmsg->cmsg_type = is_ipv6 ? IPV6_ECN : IP_ECN;
    *(int*)WSA_CMSG_DATA(cmsg) = static_cast<int>(send_ecn_);
  } else {
    message.Control.len = control_buffer.len;
  }
}

void UDPSocketWin::SetLastTosFromWSAMSG(WSAMSG& message) {
  int ecn = 0;
  for (WSACMSGHDR* cmsg = WSA_CMSG_FIRSTHDR(&message); cmsg != NULL;
       cmsg = WSA_CMSG_NXTHDR(&message, cmsg)) {
    if ((cmsg->cmsg_level == IPPROTO_IPV6 && cmsg->cmsg_type == IPV6_ECN) ||
        (cmsg->cmsg_level == IPPROTO_IP && cmsg->cmsg_type == IP_ECN)) {
      ecn = *(int*)WSA_CMSG_DATA(cmsg);
      break;
    }
  }
  last_tos_.ecn = static_cast<EcnCodePoint>(ecn);
}

int UDPSocketWin::InternalRecvFromOverlapped(IOBuffer* buf,
                                             int buf_len,
                                             IPEndPoint* address) {
  DCHECK(!core_->read_iobuffer_.get());
  DCHECK(!core_->read_message_.get());
  SockaddrStorage& storage = core_->recv_addr_storage_;
  storage.addr_len = sizeof(storage.addr_storage);

  WSABUF read_buffer;
  read_buffer.buf = buf->data();
  read_buffer.len = buf_len;

  DWORD flags = 0;
  DWORD num;
  CHECK_NE(INVALID_SOCKET, socket_);
  int rv;
  std::unique_ptr<WSAMSG> message;
  if (report_ecn_) {
    WSABUF control_buffer;
    control_buffer.buf = core_->read_control_buffer_;
    control_buffer.len = sizeof(core_->read_control_buffer_);
    message = std::make_unique<WSAMSG>();
    if (message == nullptr) {
      return WSA_NOT_ENOUGH_MEMORY;
    }
    PopulateWSAMSG(*message, storage, &read_buffer, control_buffer, false);
    rv = wsa_recv_msg_(socket_, message.get(), &num, &core_->read_overlapped_,
                       nullptr);
    if (rv == 0) {
      SetLastTosFromWSAMSG(*message);
    }
  } else {
    rv = WSARecvFrom(socket_, &read_buffer, 1, &num, &flags, storage.addr,
                     &storage.addr_len, &core_->read_overlapped_, nullptr);
  }
  if (rv == 0) {
    if (ResetEventIfSignaled(core_->read_overlapped_.hEvent)) {
      int result = num;
      // Convert address.
      IPEndPoint address_storage;
      IPEndPoint* address_to_log = nullptr;
      if (result >= 0) {
        if (address_storage.FromSockAddr(core_->recv_addr_storage_.addr,
                                         core_->recv_addr_storage_.addr_len)) {
          if (address) {
            *address = address_storage;
          }
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
  core_->read_message_ = std::move(message);
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
  int rv;
  if (send_ecn_ != ECN_NOT_ECT) {
    WSABUF control_buffer;
    char raw_control_buffer[WSA_CMSG_SPACE(sizeof(int))];
    control_buffer.buf = raw_control_buffer;
    control_buffer.len = sizeof(raw_control_buffer);
    WSAMSG message;
    bool temp_address = !remote_address_.get();
    if (temp_address) {
      remote_address_ = std::make_unique<IPEndPoint>(*address);
    }
    PopulateWSAMSG(message, storage, &write_buffer, control_buffer, true);
    if (temp_address) {
      remote_address_.reset();
    }
    rv = wsa_send_msg_(socket_, &message, flags, &num,
                       &core_->write_overlapped_, nullptr);
  } else {
    rv = WSASendTo(socket_, &write_buffer, 1, &num, flags, addr,
                   storage.addr_len, &core_->write_overlapped_, nullptr);
  }
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

  int rv;
  if (report_ecn_) {
    WSABUF read_buffer;
    read_buffer.buf = buf->data();
    read_buffer.len = buf_len;
    WSABUF control_buffer;
    char raw_control_buffer[WSA_CMSG_SPACE(sizeof(INT))];
    control_buffer.buf = raw_control_buffer;
    control_buffer.len = sizeof(raw_control_buffer);
    WSAMSG message;
    DWORD bytes_read;
    PopulateWSAMSG(message, storage, &read_buffer, control_buffer, false);
    rv = wsa_recv_msg_(socket_, &message, &bytes_read, nullptr, nullptr);
    SetLastTosFromWSAMSG(message);
    if (rv == 0) {
      rv = bytes_read;  // WSARecvMsg() returns zero on delivery, but recvfrom
                        // returns the number of bytes received.
    }
  } else {
    rv = recvfrom(socket_, buf->data(), buf_len, 0, storage.addr,
                  &storage.addr_len);
  }
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
      if (address) {
        *address = address_storage;
      }
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

  int rv;
  if (send_ecn_ != ECN_NOT_ECT) {
    char raw_control_buffer[WSA_CMSG_SPACE(sizeof(INT))];
    WSABUF write_buffer;
    write_buffer.buf = buf->data();
    write_buffer.len = buf_len;
    WSABUF control_buffer;
    control_buffer.buf = raw_control_buffer;
    control_buffer.len = sizeof(raw_control_buffer);
    WSAMSG message;
    DWORD bytes_read;
    PopulateWSAMSG(message, storage, &write_buffer, control_buffer, true);
    rv = wsa_send_msg_(socket_, &message, 0, &bytes_read, nullptr, nullptr);
    if (rv == 0) {
      rv = bytes_read;
    }
  } else {
    rv = sendto(socket_, buf->data(), buf_len, 0, addr, storage.addr_len);
  }
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
    int protocol_level = addr_family_ == AF_INET ? IPPROTO_IP : IPPROTO_IPV6;
    int option =
        addr_family_ == AF_INET ? IP_MULTICAST_LOOP : IPV6_MULTICAST_LOOP;
    int rv = setsockopt(socket_, protocol_level, option,
                        reinterpret_cast<const char*>(&loop), sizeof(loop));
    if (rv < 0) {
      return MapSystemError(WSAGetLastError());
    }
  }
  if (multicast_time_to_live_ != 1) {
    DWORD hops = multicast_time_to_live_;
    int protocol_level = addr_family_ == AF_INET ? IPPROTO_IP : IPPROTO_IPV6;
    int option =
        addr_family_ == AF_INET ? IP_MULTICAST_TTL : IPV6_MULTICAST_HOPS;
    int rv = setsockopt(socket_, protocol_level, option,
                        reinterpret_cast<const char*>(&hops), sizeof(hops));
    if (rv < 0) {
      return MapSystemError(WSAGetLastError());
    }
  }
  if (multicast_interface_ != 0) {
    switch (addr_family_) {
      case AF_INET: {
        in_addr address;
        address.s_addr = htonl(multicast_interface_);
        int rv = setsockopt(socket_, IPPROTO_IP, IP_MULTICAST_IF,
                            reinterpret_cast<const char*>(&address),
                            sizeof(address));
        if (rv) {
          return MapSystemError(WSAGetLastError());
        }
        break;
      }
      case AF_INET6: {
        uint32_t interface_index = multicast_interface_;
        int rv = setsockopt(socket_, IPPROTO_IPV6, IPV6_MULTICAST_IF,
                            reinterpret_cast<const char*>(&interface_index),
                            sizeof(interface_index));
        if (rv) {
          return MapSystemError(WSAGetLastError());
        }
        break;
      }
      default:
        NOTREACHED_IN_MIGRATION() << "Invalid address family";
        return ERR_ADDRESS_INVALID;
    }
  }
  return OK;
}

int UDPSocketWin::DoBind(const IPEndPoint& address) {
  SockaddrStorage storage;
  if (!address.ToSockAddr(storage.addr, &storage.addr_len)) {
    return ERR_ADDRESS_INVALID;
  }
  int rv = bind(socket_, storage.addr, storage.addr_len);
  if (rv == 0) {
    return OK;
  }
  int last_error = WSAGetLastError();
  // Map some codes that are special to bind() separately.
  // * WSAEACCES: If a port is already bound to a socket, WSAEACCES may be
  //   returned instead of WSAEADDRINUSE, depending on whether the socket
  //   option SO_REUSEADDR or SO_EXCLUSIVEADDRUSE is set and whether the
  //   conflicting socket is owned by a different user account. See the MSDN
  //   page "Using SO_REUSEADDR and SO_EXCLUSIVEADDRUSE" for the gory details.
  if (last_error == WSAEACCES || last_error == WSAEADDRNOTAVAIL) {
    return ERR_ADDRESS_IN_USE;
  }
  return MapSystemError(last_error);
}

QwaveApi* UDPSocketWin::GetQwaveApi() const {
  return QwaveApi::GetDefault();
}

int UDPSocketWin::JoinGroup(const IPAddress& group_address) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!is_connected()) {
    return ERR_SOCKET_NOT_CONNECTED;
  }

  switch (group_address.size()) {
    case IPAddress::kIPv4AddressSize: {
      if (addr_family_ != AF_INET) {
        return ERR_ADDRESS_INVALID;
      }
      ip_mreq mreq;
      mreq.imr_interface.s_addr = htonl(multicast_interface_);
      memcpy(&mreq.imr_multiaddr, group_address.bytes().data(),
             IPAddress::kIPv4AddressSize);
      int rv = setsockopt(socket_, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                          reinterpret_cast<const char*>(&mreq), sizeof(mreq));
      if (rv) {
        return MapSystemError(WSAGetLastError());
      }
      return OK;
    }
    case IPAddress::kIPv6AddressSize: {
      if (addr_family_ != AF_INET6) {
        return ERR_ADDRESS_INVALID;
      }
      ipv6_mreq mreq;
      mreq.ipv6mr_interface = multicast_interface_;
      memcpy(&mreq.ipv6mr_multiaddr, group_address.bytes().data(),
             IPAddress::kIPv6AddressSize);
      int rv = setsockopt(socket_, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP,
                          reinterpret_cast<const char*>(&mreq), sizeof(mreq));
      if (rv) {
        return MapSystemError(WSAGetLastError());
      }
      return OK;
    }
    default:
      NOTREACHED_IN_MIGRATION() << "Invalid address family";
      return ERR_ADDRESS_INVALID;
  }
}

int UDPSocketWin::LeaveGroup(const IPAddress& group_address) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!is_connected()) {
    return ERR_SOCKET_NOT_CONNECTED;
  }

  switch (group_address.size()) {
    case IPAddress::kIPv4AddressSize: {
      if (addr_family_ != AF_INET) {
        return ERR_ADDRESS_INVALID;
      }
      ip_mreq mreq;
      mreq.imr_interface.s_addr = htonl(multicast_interface_);
      memcpy(&mreq.imr_multiaddr, group_address.bytes().data(),
             IPAddress::kIPv4AddressSize);
      int rv = setsockopt(socket_, IPPROTO_IP, IP_DROP_MEMBERSHIP,
                          reinterpret_cast<const char*>(&mreq), sizeof(mreq));
      if (rv) {
        return MapSystemError(WSAGetLastError());
      }
      return OK;
    }
    case IPAddress::kIPv6AddressSize: {
      if (addr_family_ != AF_INET6) {
        return ERR_ADDRESS_INVALID;
      }
      ipv6_mreq mreq;
      mreq.ipv6mr_interface = multicast_interface_;
      memcpy(&mreq.ipv6mr_multiaddr, group_address.bytes().data(),
             IPAddress::kIPv6AddressSize);
      int rv = setsockopt(socket_, IPPROTO_IPV6, IP_DROP_MEMBERSHIP,
                          reinterpret_cast<const char*>(&mreq), sizeof(mreq));
      if (rv) {
        return MapSystemError(WSAGetLastError());
      }
      return OK;
    }
    default:
      NOTREACHED_IN_MIGRATION() << "Invalid address family";
      return ERR_ADDRESS_INVALID;
  }
}

int UDPSocketWin::SetMulticastInterface(uint32_t interface_index) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (is_connected()) {
    return ERR_SOCKET_IS_CONNECTED;
  }
  multicast_interface_ = interface_index;
  return OK;
}

int UDPSocketWin::SetMulticastTimeToLive(int time_to_live) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (is_connected()) {
    return ERR_SOCKET_IS_CONNECTED;
  }

  if (time_to_live < 0 || time_to_live > 255) {
    return ERR_INVALID_ARGUMENT;
  }
  multicast_time_to_live_ = time_to_live;
  return OK;
}

int UDPSocketWin::SetMulticastLoopbackMode(bool loopback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (is_connected()) {
    return ERR_SOCKET_IS_CONNECTED;
  }

  if (loopback) {
    socket_options_ |= SOCKET_OPTION_MULTICAST_LOOP;
  } else {
    socket_options_ &= ~SOCKET_OPTION_MULTICAST_LOOP;
  }
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
      NOTREACHED_IN_MIGRATION();
      break;
  }
  return traffic_type;
}

int UDPSocketWin::SetDiffServCodePoint(DiffServCodePoint dscp) {
  return SetTos(dscp, ECN_NO_CHANGE);
}

int UDPSocketWin::SetTos(DiffServCodePoint dscp, EcnCodePoint ecn) {
  if (!is_connected()) {
    return ERR_SOCKET_NOT_CONNECTED;
  }

  if (dscp != DSCP_NO_CHANGE) {
    QwaveApi* api = GetQwaveApi();

    if (!api->qwave_supported()) {
      return ERR_NOT_IMPLEMENTED;
    }

    if (!dscp_manager_) {
      dscp_manager_ = std::make_unique<DscpManager>(api, socket_);
    }

    dscp_manager_->Set(dscp);
    if (remote_address_) {
      int rv = dscp_manager_->PrepareForSend(*remote_address_.get());
      if (rv != OK) {
        return rv;
      }
    }
  }
  if (ecn == ECN_NO_CHANGE) {
    return OK;
  }
  if (wsa_send_msg_ == nullptr) {
    wsa_send_msg_ = GetSendMsgPointer();
  }
  send_ecn_ = ecn;
  return OK;
}

int UDPSocketWin::SetIPv6Only(bool ipv6_only) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (is_connected()) {
    return ERR_SOCKET_IS_CONNECTED;
  }
  return net::SetIPv6Only(socket_, ipv6_only);
}

void UDPSocketWin::DetachFromThread() {
  DETACH_FROM_THREAD(thread_checker_);
}

void UDPSocketWin::UseNonBlockingIO() {
  DCHECK(!core_);
  use_non_blocking_io_ = true;
}

void UDPSocketWin::ApplySocketTag(const SocketTag& tag) {
  // Windows does not support any specific SocketTags so fail if any
  // non-default tag is applied.
  CHECK(tag == SocketTag());
}

DscpManager::DscpManager(QwaveApi* api, SOCKET socket)
    : api_(api), socket_(socket) {
  RequestHandle();
}

DscpManager::~DscpManager() {
  if (!qos_handle_) {
    return;
  }

  if (flow_id_ != 0) {
    api_->RemoveSocketFromFlow(qos_handle_, NULL, flow_id_, 0);
  }

  api_->CloseHandle(qos_handle_);
}

void DscpManager::Set(DiffServCodePoint dscp) {
  if (dscp == DSCP_NO_CHANGE || dscp == dscp_value_) {
    return;
  }

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

  if (!api_->qwave_supported()) {
    return ERR_NOT_IMPLEMENTED;
  }

  if (!qos_handle_) {
    return ERR_INVALID_HANDLE;  // The closest net error to try again later.
  }

  if (configured_.find(remote_address) != configured_.end()) {
    return OK;
  }

  SockaddrStorage storage;
  if (!remote_address.ToSockAddr(storage.addr, &storage.addr_len)) {
    return ERR_ADDRESS_INVALID;
  }

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
      // completes asynchronously the next PrepareForSend call will
      // re-register the address with the new QoS Handle.  In the meantime,
      // sends will continue without DSCP.
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
  if (handle_is_initializing_) {
    return;
  }

  if (qos_handle_) {
    api_->CloseHandle(qos_handle_);
    qos_handle_ = nullptr;
  }

  handle_is_initializing_ = true;
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
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
  if (!handle) {
    api->OnFatalError();
  }

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
