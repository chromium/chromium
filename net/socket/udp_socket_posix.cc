// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "build/build_config.h"

#if BUILDFLAG(IS_APPLE)
// This must be defined before including <netinet/in.h>
// to use IPV6_DONTFRAG, one of the IPv6 Sockets option introduced by RFC 3542
#define __APPLE_USE_RFC_3542
#endif  // BUILDFLAG(IS_APPLE)

#include "net/socket/udp_socket_posix.h"

#include <errno.h>
#include <fcntl.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <memory>

#include "base/debug/alias.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/rand_util.h"
#include "base/task/current_thread.h"
#include "base/task/thread_pool.h"
#include "build/chromeos_buildflags.h"
#include "net/base/cronet_buildflags.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/network_activity_monitor.h"
#include "net/base/sockaddr_storage.h"
#include "net/base/trace_constants.h"
#include "net/base/tracing.h"
#include "net/log/net_log.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source.h"
#include "net/log/net_log_source_type.h"
#include "net/socket/socket_descriptor.h"
#include "net/socket/socket_options.h"
#include "net/socket/socket_tag.h"
#include "net/socket/udp_net_log_parameters.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/native_library.h"
#include "net/android/network_library.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_APPLE)
#include "net/base/apple/guarded_fd.h"
#endif  // BUILDFLAG(IS_APPLE)

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif  // BUILDFLAG(IS_MAC)

namespace net {

namespace {

constexpr int kBindRetries = 10;
constexpr int kPortStart = 1024;
constexpr int kPortEnd = 65535;

int GetSocketFDHash(int fd) {
  return fd ^ 1595649551;
}

}  // namespace

UDPSocketPosix::UDPSocketPosix(DatagramSocket::BindType bind_type,
                               net::NetLog* net_log,
                               const net::NetLogSource& source)
    : socket_(kInvalidSocket),
      bind_type_(bind_type),
      read_socket_watcher_(FROM_HERE),
      write_socket_watcher_(FROM_HERE),
      read_watcher_(this),
      write_watcher_(this),
      net_log_(NetLogWithSource::Make(net_log, NetLogSourceType::UDP_SOCKET)),
      bound_network_(handles::kInvalidNetworkHandle) {
  net_log_.BeginEventReferencingSource(NetLogEventType::SOCKET_ALIVE, source);
}

UDPSocketPosix::UDPSocketPosix(DatagramSocket::BindType bind_type,
                               NetLogWithSource source_net_log)
    : socket_(kInvalidSocket),
      bind_type_(bind_type),
      read_socket_watcher_(FROM_HERE),
      write_socket_watcher_(FROM_HERE),
      read_watcher_(this),
      write_watcher_(this),
      net_log_(source_net_log),
      bound_network_(handles::kInvalidNetworkHandle) {
  net_log_.BeginEventReferencingSource(NetLogEventType::SOCKET_ALIVE,
                                       net_log_.source());
}

UDPSocketPosix::~UDPSocketPosix() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  Close();
  net_log_.EndEvent(NetLogEventType::SOCKET_ALIVE);
}

int UDPSocketPosix::Open(AddressFamily address_family) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_EQ(socket_, kInvalidSocket);

  auto owned_socket_count = TryAcquireGlobalUDPSocketCount();
  if (owned_socket_count.empty())
    return ERR_INSUFFICIENT_RESOURCES;

  owned_socket_count_ = std::move(owned_socket_count);
  addr_family_ = ConvertAddressFamily(address_family);
  socket_ = CreatePlatformSocket(addr_family_, SOCK_DGRAM, 0);
  if (socket_ == kInvalidSocket) {
    owned_socket_count_.Reset();
    return MapSystemError(errno);
  }

  return ConfigureOpenedSocket();
}

int UDPSocketPosix::AdoptOpenedSocket(AddressFamily address_family,
                                      int socket) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_EQ(socket_, kInvalidSocket);
  auto owned_socket_count = TryAcquireGlobalUDPSocketCount();
  if (owned_socket_count.empty()) {
    return ERR_INSUFFICIENT_RESOURCES;
  }

  owned_socket_count_ = std::move(owned_socket_count);
  socket_ = socket;
  addr_family_ = ConvertAddressFamily(address_family);
  return ConfigureOpenedSocket();
}

int UDPSocketPosix::ConfigureOpenedSocket() {
#if BUILDFLAG(IS_APPLE) && !BUILDFLAG(CRONET_BUILD)
  // https://crbug.com/41271555: Guard against a file descriptor being closed
  // out from underneath the socket.
  guardid_t guardid = reinterpret_cast<guardid_t>(this);
  PCHECK(change_fdguard_np(socket_, nullptr, 0, &guardid,
                           GUARD_CLOSE | GUARD_DUP, nullptr) == 0);
#endif  // BUILDFLAG(IS_APPLE) && !BUILDFLAG(CRONET_BUILD)
  socket_hash_ = GetSocketFDHash(socket_);
  if (!base::SetNonBlocking(socket_)) {
    const int err = MapSystemError(errno);
    Close();
    return err;
  }
  if (tag_ != SocketTag())
    tag_.Apply(socket_);

  return OK;
}

void UDPSocketPosix::Close() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  owned_socket_count_.Reset();

  if (socket_ == kInvalidSocket)
    return;

  // Zero out any pending read/write callback state.
  read_buf_.reset();
  read_buf_len_ = 0;
  read_callback_.Reset();
  recv_from_address_ = nullptr;
  write_buf_.reset();
  write_buf_len_ = 0;
  write_callback_.Reset();
  send_to_address_.reset();

  bool ok = read_socket_watcher_.StopWatchingFileDescriptor();
  DCHECK(ok);
  ok = write_socket_watcher_.StopWatchingFileDescriptor();
  DCHECK(ok);

  // Verify that |socket_| hasn't been corrupted. Needed to debug
  // https://crbug.com/41426706.
  CHECK_EQ(socket_hash_, GetSocketFDHash(socket_));
  TRACE_EVENT("base", perfetto::StaticString{"CloseSocketUDP"});

#if BUILDFLAG(IS_APPLE) && !BUILDFLAG(CRONET_BUILD)
  // Attempt to clear errors on the socket so that they are not returned by
  // close(). This seems to be effective at clearing some, but not all,
  // EPROTOTYPE errors. See https://crbug.com/40732798.
  int value = 0;
  socklen_t value_len = sizeof(value);
  HANDLE_EINTR(getsockopt(socket_, SOL_SOCKET, SO_ERROR, &value, &value_len));

  // https://crbug.com/41271555: Guard against a file descriptor being closed
  // out from underneath the socket.
  guardid_t guardid = reinterpret_cast<guardid_t>(this);
  if (IGNORE_EINTR(guarded_close_np(socket_, &guardid)) != 0) {
    // There is a bug in the Mac OS kernel that it can return an ENOTCONN or
    // EPROTOTYPE error. In this case we don't know whether the file descriptor
    // is still allocated or not. We cannot safely close the file descriptor
    // because it may have been reused by another thread in the meantime. We may
    // leak file handles here and cause a crash indirectly later. See
    // https://crbug.com/40732798.
    PCHECK(errno == ENOTCONN || errno == EPROTOTYPE);
  }
#else
  PCHECK(IGNORE_EINTR(close(socket_)) == 0);
#endif  // BUILDFLAG(IS_APPLE) && !BUILDFLAG(CRONET_BUILD)

  socket_ = kInvalidSocket;
  addr_family_ = 0;
  is_connected_ = false;
  tag_ = SocketTag();
}

int UDPSocketPosix::GetPeerAddress(IPEndPoint* address) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(address);
  if (!is_connected())
    return ERR_SOCKET_NOT_CONNECTED;

  if (!remote_address_.get()) {
    SockaddrStorage storage;
    if (getpeername(socket_, storage.addr, &storage.addr_len))
      return MapSystemError(errno);
    auto endpoint = std::make_unique<IPEndPoint>();
    if (!endpoint->FromSockAddr(storage.addr, storage.addr_len))
      return ERR_ADDRESS_INVALID;
    remote_address_ = std::move(endpoint);
  }

  *address = *remote_address_;
  return OK;
}

int UDPSocketPosix::GetLocalAddress(IPEndPoint* address) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(address);
  if (!is_connected())
    return ERR_SOCKET_NOT_CONNECTED;

  if (!local_address_.get()) {
    SockaddrStorage storage;
    if (getsockname(socket_, storage.addr, &storage.addr_len))
      return MapSystemError(errno);
    auto endpoint = std::make_unique<IPEndPoint>();
    if (!endpoint->FromSockAddr(storage.addr, storage.addr_len))
      return ERR_ADDRESS_INVALID;
    local_address_ = std::move(endpoint);
    net_log_.AddEvent(NetLogEventType::UDP_LOCAL_ADDRESS, [&] {
      return CreateNetLogUDPConnectParams(*local_address_, bound_network_);
    });
  }

  *address = *local_address_;
  return OK;
}

int UDPSocketPosix::Read(IOBuffer* buf,
                         int buf_len,
                         CompletionOnceCallback callback) {
  return RecvFrom(buf, buf_len, nullptr, std::move(callback));
}

int UDPSocketPosix::RecvFrom(IOBuffer* buf,
                             int buf_len,
                             IPEndPoint* address,
                             CompletionOnceCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_NE(kInvalidSocket, socket_);
  CHECK(read_callback_.is_null());
  DCHECK(!recv_from_address_);
  DCHECK(!callback.is_null());  // Synchronous operation not supported
  DCHECK_GT(buf_len, 0);

  int nread = InternalRecvFrom(buf, buf_len, address);
  if (nread != ERR_IO_PENDING)
    return nread;

  if (!base::CurrentIOThread::Get()->WatchFileDescriptor(
          socket_, true, base::MessagePumpForIO::WATCH_READ,
          &read_socket_watcher_, &read_watcher_)) {
    PLOG(ERROR) << "WatchFileDescriptor failed on read";
    int result = MapSystemError(errno);
    LogRead(result, nullptr, 0, nullptr);
    return result;
  }

  read_buf_ = buf;
  read_buf_len_ = buf_len;
  recv_from_address_ = address;
  read_callback_ = std::move(callback);
  return ERR_IO_PENDING;
}

int UDPSocketPosix::Write(
    IOBuffer* buf,
    int buf_len,
    CompletionOnceCallback callback,
    const NetworkTrafficAnnotationTag& traffic_annotation) {
  return SendToOrWrite(buf, buf_len, nullptr, std::move(callback));
}

int UDPSocketPosix::SendTo(IOBuffer* buf,
                           int buf_len,
                           const IPEndPoint& address,
                           CompletionOnceCallback callback) {
  return SendToOrWrite(buf, buf_len, &address, std::move(callback));
}

int UDPSocketPosix::SendToOrWrite(IOBuffer* buf,
                                  int buf_len,
                                  const IPEndPoint* address,
                                  CompletionOnceCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_NE(kInvalidSocket, socket_);
  CHECK(write_callback_.is_null());
  DCHECK(!callback.is_null());  // Synchronous operation not supported
  DCHECK_GT(buf_len, 0);

  if (int result = InternalSendTo(buf, buf_len, address);
      result != ERR_IO_PENDING) {
    return result;
  }

  if (!base::CurrentIOThread::Get()->WatchFileDescriptor(
          socket_, true, base::MessagePumpForIO::WATCH_WRITE,
          &write_socket_watcher_, &write_watcher_)) {
    DVPLOG(1) << "WatchFileDescriptor failed on write";
    int result = MapSystemError(errno);
    LogWrite(result, nullptr, nullptr);
    return result;
  }

  write_buf_ = buf;
  write_buf_len_ = buf_len;
  DCHECK(!send_to_address_.get());
  if (address) {
    send_to_address_ = std::make_unique<IPEndPoint>(*address);
  }
  write_callback_ = std::move(callback);
  return ERR_IO_PENDING;
}

int UDPSocketPosix::Connect(const IPEndPoint& address) {
  DCHECK_NE(socket_, kInvalidSocket);
  net_log_.BeginEvent(NetLogEventType::UDP_CONNECT, [&] {
    return CreateNetLogUDPConnectParams(address, bound_network_);
  });
  int rv = SetMulticastOptions();
  if (rv != OK)
    return rv;
  rv = InternalConnect(address);
  net_log_.EndEventWithNetErrorCode(NetLogEventType::UDP_CONNECT, rv);
  is_connected_ = (rv == OK);
  if (rv != OK)
    tag_ = SocketTag();
  return rv;
}

int UDPSocketPosix::InternalConnect(const IPEndPoint& address) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!is_connected());
  DCHECK(!remote_address_.get());

  int rv = 0;
  if (bind_type_ == DatagramSocket::RANDOM_BIND) {
    // Construct IPAddress of appropriate size (IPv4 or IPv6) of 0s,
    // representing INADDR_ANY or in6addr_any.
    size_t addr_size = address.GetSockAddrFamily() == AF_INET
                           ? IPAddress::kIPv4AddressSize
                           : IPAddress::kIPv6AddressSize;
    rv = RandomBind(IPAddress::AllZeros(addr_size));
  }
  // else connect() does the DatagramSocket::DEFAULT_BIND

  if (rv < 0) {
    return rv;
  }

  SockaddrStorage storage;
  if (!address.ToSockAddr(storage.addr, &storage.addr_len))
    return ERR_ADDRESS_INVALID;

  rv = HANDLE_EINTR(connect(socket_, storage.addr, storage.addr_len));
  if (rv < 0)
    return MapSystemError(errno);

  remote_address_ = std::make_unique<IPEndPoint>(address);
  return rv;
}

int UDPSocketPosix::Bind(const IPEndPoint& address) {
  DCHECK_NE(socket_, kInvalidSocket);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!is_connected());

  int rv = SetMulticastOptions();
  if (rv < 0)
    return rv;

  rv = DoBind(address);
  if (rv < 0)
    return rv;

  is_connected_ = true;
  local_address_.reset();
  return rv;
}

int UDPSocketPosix::BindToNetwork(handles::NetworkHandle network) {
  DCHECK_NE(socket_, kInvalidSocket);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!is_connected());
#if BUILDFLAG(IS_ANDROID)
  int rv = net::android::BindToNetwork(socket_, network);
  if (rv == OK)
    bound_network_ = network;
  return rv;
#else
  NOTIMPLEMENTED();
  return ERR_NOT_IMPLEMENTED;
#endif
}

int UDPSocketPosix::SetReceiveBufferSize(int32_t size) {
  DCHECK_NE(socket_, kInvalidSocket);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return SetSocketReceiveBufferSize(socket_, size);
}

int UDPSocketPosix::SetSendBufferSize(int32_t size) {
  DCHECK_NE(socket_, kInvalidSocket);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return SetSocketSendBufferSize(socket_, size);
}

int UDPSocketPosix::SetDoNotFragment() {
  DCHECK_NE(socket_, kInvalidSocket);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

#if !defined(IP_PMTUDISC_DO) && !BUILDFLAG(IS_MAC)
  return ERR_NOT_IMPLEMENTED;

#elif BUILDFLAG(IS_MAC)
  int val = 1;
  if (addr_family_ == AF_INET6) {
    int rv =
        setsockopt(socket_, IPPROTO_IPV6, IPV6_DONTFRAG, &val, sizeof(val));
    // IP_DONTFRAG is not supported on v4mapped addresses.
    return rv == 0 ? OK : MapSystemError(errno);
  }
  int rv = setsockopt(socket_, IPPROTO_IP, IP_DONTFRAG, &val, sizeof(val));
  return rv == 0 ? OK : MapSystemError(errno);

#else
  if (addr_family_ == AF_INET6) {
    int val = IPV6_PMTUDISC_DO;
    if (setsockopt(socket_, IPPROTO_IPV6, IPV6_MTU_DISCOVER, &val,
                   sizeof(val)) != 0) {
      return MapSystemError(errno);
    }

    int v6_only = false;
    socklen_t v6_only_len = sizeof(v6_only);
    if (getsockopt(socket_, IPPROTO_IPV6, IPV6_V6ONLY, &v6_only,
                   &v6_only_len) != 0) {
      return MapSystemError(errno);
    }

    if (v6_only)
      return OK;
  }

  int val = IP_PMTUDISC_DO;
  int rv = setsockopt(socket_, IPPROTO_IP, IP_MTU_DISCOVER, &val, sizeof(val));
  return rv == 0 ? OK : MapSystemError(errno);
#endif
}

int UDPSocketPosix::SetRecvTos() {
  DCHECK_NE(socket_, kInvalidSocket);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  unsigned int ecn = 1;
  if (addr_family_ == AF_INET6) {
    if (setsockopt(socket_, IPPROTO_IPV6, IPV6_RECVTCLASS, &ecn, sizeof(ecn)) !=
        0) {
      return MapSystemError(errno);
    }
#if BUILDFLAG(IS_APPLE)
    // Linux requires dual-stack sockets to have the sockopt set on both levels.
    // Apple does not, and in fact returns an error if it is.
    return OK;
#else
    int v6_only = false;
    socklen_t v6_only_len = sizeof(v6_only);
    if (getsockopt(socket_, IPPROTO_IPV6, IPV6_V6ONLY, &v6_only,
                   &v6_only_len) != 0) {
      return MapSystemError(errno);
    }
    if (v6_only) {
      return OK;
    }
#endif  // BUILDFLAG(IS_APPLE)
  }

  int rv = setsockopt(socket_, IPPROTO_IP, IP_RECVTOS, &ecn, sizeof(ecn));
  return rv == 0 ? OK : MapSystemError(errno);
}

void UDPSocketPosix::SetMsgConfirm(bool confirm) {
#if !BUILDFLAG(IS_APPLE)
  if (confirm) {
    sendto_flags_ |= MSG_CONFIRM;
  } else {
    sendto_flags_ &= ~MSG_CONFIRM;
  }
#endif  // !BUILDFLAG(IS_APPLE)
}

int UDPSocketPosix::AllowAddressReuse() {
  DCHECK_NE(socket_, kInvalidSocket);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!is_connected());
  return SetReuseAddr(socket_, true);
}

int UDPSocketPosix::SetBroadcast(bool broadcast) {
  DCHECK_NE(socket_, kInvalidSocket);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  int value = broadcast ? 1 : 0;
  int rv;
#if BUILDFLAG(IS_APPLE)
  // SO_REUSEPORT on OSX permits multiple processes to each receive
  // UDP multicast or broadcast datagrams destined for the bound
  // port.
  // This is only being set on OSX because its behavior is platform dependent
  // and we are playing it safe by only setting it on platforms where things
  // break.
  rv = setsockopt(socket_, SOL_SOCKET, SO_REUSEPORT, &value, sizeof(value));
  if (rv != 0)
    return MapSystemError(errno);
#endif  // BUILDFLAG(IS_APPLE)
  rv = setsockopt(socket_, SOL_SOCKET, SO_BROADCAST, &value, sizeof(value));

  return rv == 0 ? OK : MapSystemError(errno);
}

int UDPSocketPosix::AllowAddressSharingForMulticast() {
  DCHECK_NE(socket_, kInvalidSocket);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!is_connected());

  int rv = AllowAddressReuse();
  if (rv != OK)
    return rv;

#ifdef SO_REUSEPORT
  // Attempt to set SO_REUSEPORT if available. On some platforms, this is
  // necessary to allow the address to be fully shared between separate sockets.
  // On platforms where the option does not exist, SO_REUSEADDR should be
  // sufficient to share multicast packets if such sharing is at all possible.
  int value = 1;
  rv = setsockopt(socket_, SOL_SOCKET, SO_REUSEPORT, &value, sizeof(value));
  // Ignore errors that the option does not exist.
  if (rv != 0 && errno != ENOPROTOOPT)
    return MapSystemError(errno);
#endif  // SO_REUSEPORT

  return OK;
}

void UDPSocketPosix::ReadWatcher::OnFileCanReadWithoutBlocking(int) {
  TRACE_EVENT(NetTracingCategory(),
              "UDPSocketPosix::ReadWatcher::OnFileCanReadWithoutBlocking");
  if (!socket_->read_callback_.is_null())
    socket_->DidCompleteRead();
}

void UDPSocketPosix::WriteWatcher::OnFileCanWriteWithoutBlocking(int) {
  if (!socket_->write_callback_.is_null())
    socket_->DidCompleteWrite();
}

void UDPSocketPosix::DoReadCallback(int rv) {
  DCHECK_NE(rv, ERR_IO_PENDING);
  DCHECK(!read_callback_.is_null());

  // Since Run() may result in Read() being called,
  // clear |read_callback_| up front.
  std::move(read_callback_).Run(rv);
}

void UDPSocketPosix::DoWriteCallback(int rv) {
  DCHECK_NE(rv, ERR_IO_PENDING);
  DCHECK(!write_callback_.is_null());

  // Since Run() may result in Write() being called,
  // clear |write_callback_| up front.
  std::move(write_callback_).Run(rv);
}

void UDPSocketPosix::DidCompleteRead() {
  int result =
      InternalRecvFrom(read_buf_.get(), read_buf_len_, recv_from_address_);
  if (result != ERR_IO_PENDING) {
    read_buf_.reset();
    read_buf_len_ = 0;
    recv_from_address_ = nullptr;
    bool ok = read_socket_watcher_.StopWatchingFileDescriptor();
    DCHECK(ok);
    DoReadCallback(result);
  }
}

void UDPSocketPosix::LogRead(int result,
                             const char* bytes,
                             socklen_t addr_len,
                             const sockaddr* addr) {
  if (result < 0) {
    net_log_.AddEventWithNetErrorCode(NetLogEventType::UDP_RECEIVE_ERROR,
                                      result);
    return;
  }

  if (net_log_.IsCapturing()) {
    DCHECK(addr_len > 0);
    DCHECK(addr);

    IPEndPoint address;
    bool is_address_valid = address.FromSockAddr(addr, addr_len);
    NetLogUDPDataTransfer(net_log_, NetLogEventType::UDP_BYTES_RECEIVED, result,
                          bytes, is_address_valid ? &address : nullptr);
  }

  activity_monitor::IncrementBytesReceived(result);
}

void UDPSocketPosix::DidCompleteWrite() {
  int result =
      InternalSendTo(write_buf_.get(), write_buf_len_, send_to_address_.get());

  if (result != ERR_IO_PENDING) {
    write_buf_.reset();
    write_buf_len_ = 0;
    send_to_address_.reset();
    write_socket_watcher_.StopWatchingFileDescriptor();
    DoWriteCallback(result);
  }
}

void UDPSocketPosix::LogWrite(int result,
                              const char* bytes,
                              const IPEndPoint* address) {
  if (result < 0) {
    net_log_.AddEventWithNetErrorCode(NetLogEventType::UDP_SEND_ERROR, result);
    return;
  }

  if (net_log_.IsCapturing()) {
    NetLogUDPDataTransfer(net_log_, NetLogEventType::UDP_BYTES_SENT, result,
                          bytes, address);
  }
}

// TODO(crbug.com/40285166): Because InternalRecvFromConnectedSocket() uses
// recvfrom() instead of recvmsg(), it cannot report received ECN marks for
// QUIC ACK-ECN frames. It might be time to deprecate
// experimental_recv_optimization_enabled_ if that experiment has run its
// course.
int UDPSocketPosix::InternalRecvFrom(IOBuffer* buf,
                                     int buf_len,
                                     IPEndPoint* address) {
  // If the socket is connected and the remote address is known
  // use the more efficient method that uses read() instead of recvmsg().
  if (experimental_recv_optimization_enabled_ && is_connected_ &&
      remote_address_) {
    return InternalRecvFromConnectedSocket(buf, buf_len, address);
  }
  return InternalRecvFromNonConnectedSocket(buf, buf_len, address);
}

int UDPSocketPosix::InternalRecvFromConnectedSocket(IOBuffer* buf,
                                                    int buf_len,
                                                    IPEndPoint* address) {
  DCHECK(is_connected_);
  DCHECK(remote_address_);
  int result;
  int bytes_transferred = HANDLE_EINTR(read(socket_, buf->data(), buf_len));
  if (bytes_transferred < 0) {
    result = MapSystemError(errno);
    if (result == ERR_IO_PENDING) {
      return result;
    }
  } else if (bytes_transferred == buf_len) {
    // NB: recv(..., MSG_TRUNC) would be a more reliable way to do this on
    // Linux, but isn't supported by POSIX.
    result = ERR_MSG_TOO_BIG;
  } else {
    result = bytes_transferred;
    if (address) {
      *address = *remote_address_.get();
    }
  }

  SockaddrStorage sock_addr;
  bool success =
        remote_address_->ToSockAddr(sock_addr.addr, &sock_addr.addr_len);
    DCHECK(success);
    LogRead(result, buf->data(), sock_addr.addr_len, sock_addr.addr);
  return result;
}

int UDPSocketPosix::InternalRecvFromNonConnectedSocket(IOBuffer* buf,
                                                       int buf_len,
                                                       IPEndPoint* address) {
  SockaddrStorage storage;
  struct iovec iov = {
      .iov_base = buf->data(),
      .iov_len = static_cast<size_t>(buf_len),
  };
  // control_buffer needs to be big enough to accommodate the maximum
  // conceivable number of CMSGs. Other (proprietary) Google QUIC code uses
  // 512 Bytes, re-used here.
  char control_buffer[512];
  struct msghdr msg = {
      .msg_name = storage.addr,
      .msg_namelen = storage.addr_len,
      .msg_iov = &iov,
      .msg_iovlen = 1,
      .msg_control = control_buffer,
      .msg_controllen = ABSL_ARRAYSIZE(control_buffer),
  };
  int result;
  int bytes_transferred = HANDLE_EINTR(recvmsg(socket_, &msg, 0));
  if (bytes_transferred < 0) {
    result = MapSystemError(errno);
    if (result == ERR_IO_PENDING) {
      return result;
    }
  } else {
    storage.addr_len = msg.msg_namelen;
    if (msg.msg_flags & MSG_TRUNC) {
      // NB: recvfrom(..., MSG_TRUNC, ...) would be a simpler way to do this on
      // Linux, but isn't supported by POSIX.
      result = ERR_MSG_TOO_BIG;
    } else if (address &&
               !address->FromSockAddr(storage.addr, storage.addr_len)) {
      result = ERR_ADDRESS_INVALID;
    } else {
      result = bytes_transferred;
    }
    last_tos_ = 0;
    if (bytes_transferred > 0 && msg.msg_controllen > 0) {
      for (struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg); cmsg != nullptr;
           cmsg = CMSG_NXTHDR(&msg, cmsg)) {
#if BUILDFLAG(IS_APPLE)
        if ((cmsg->cmsg_level == IPPROTO_IP && cmsg->cmsg_type == IP_RECVTOS) ||
            (cmsg->cmsg_level == IPPROTO_IPV6 &&
             cmsg->cmsg_type == IPV6_TCLASS)) {
#else
        if ((cmsg->cmsg_level == IPPROTO_IP && cmsg->cmsg_type == IP_TOS) ||
            (cmsg->cmsg_level == IPPROTO_IPV6 &&
             cmsg->cmsg_type == IPV6_TCLASS)) {
#endif  // BUILDFLAG(IS_APPLE)
          last_tos_ = *(reinterpret_cast<uint8_t*>(CMSG_DATA(cmsg)));
        }
      }
    }
  }

  LogRead(result, buf->data(), storage.addr_len, storage.addr);
  return result;
}

int UDPSocketPosix::InternalSendTo(IOBuffer* buf,
                                   int buf_len,
                                   const IPEndPoint* address) {
  SockaddrStorage storage;
  struct sockaddr* addr = storage.addr;
  if (!address) {
    addr = nullptr;
    storage.addr_len = 0;
  } else {
    if (!address->ToSockAddr(storage.addr, &storage.addr_len)) {
      int result = ERR_ADDRESS_INVALID;
      LogWrite(result, nullptr, nullptr);
      return result;
    }
  }

  int result = HANDLE_EINTR(sendto(socket_, buf->data(), buf_len, sendto_flags_,
                                   addr, storage.addr_len));
  if (result < 0)
    result = MapSystemError(errno);
  if (result != ERR_IO_PENDING)
    LogWrite(result, buf->data(), address);
  return result;
}

int UDPSocketPosix::SetMulticastOptions() {
  if (!(socket_options_ & SOCKET_OPTION_MULTICAST_LOOP)) {
    int rv;
    if (addr_family_ == AF_INET) {
      u_char loop = 0;
      rv = setsockopt(socket_, IPPROTO_IP, IP_MULTICAST_LOOP,
                      &loop, sizeof(loop));
    } else {
      u_int loop = 0;
      rv = setsockopt(socket_, IPPROTO_IPV6, IPV6_MULTICAST_LOOP,
                      &loop, sizeof(loop));
    }
    if (rv < 0)
      return MapSystemError(errno);
  }
  if (multicast_time_to_live_ != IP_DEFAULT_MULTICAST_TTL) {
    int rv;
    if (addr_family_ == AF_INET) {
      u_char ttl = multicast_time_to_live_;
      rv = setsockopt(socket_, IPPROTO_IP, IP_MULTICAST_TTL,
                      &ttl, sizeof(ttl));
    } else {
      // Signed integer. -1 to use route default.
      int ttl = multicast_time_to_live_;
      rv = setsockopt(socket_, IPPROTO_IPV6, IPV6_MULTICAST_HOPS,
                      &ttl, sizeof(ttl));
    }
    if (rv < 0)
      return MapSystemError(errno);
  }
  if (multicast_interface_ != 0) {
    switch (addr_family_) {
      case AF_INET: {
        ip_mreqn mreq = {};
        mreq.imr_ifindex = multicast_interface_;
        mreq.imr_address.s_addr = htonl(INADDR_ANY);
        int rv = setsockopt(socket_, IPPROTO_IP, IP_MULTICAST_IF,
                            reinterpret_cast<const char*>(&mreq), sizeof(mreq));
        if (rv)
          return MapSystemError(errno);
        break;
      }
      case AF_INET6: {
        uint32_t interface_index = multicast_interface_;
        int rv = setsockopt(socket_, IPPROTO_IPV6, IPV6_MULTICAST_IF,
                            reinterpret_cast<const char*>(&interface_index),
                            sizeof(interface_index));
        if (rv)
          return MapSystemError(errno);
        break;
      }
      default:
        NOTREACHED_IN_MIGRATION() << "Invalid address family";
        return ERR_ADDRESS_INVALID;
    }
  }
  return OK;
}

int UDPSocketPosix::DoBind(const IPEndPoint& address) {
  SockaddrStorage storage;
  if (!address.ToSockAddr(storage.addr, &storage.addr_len))
    return ERR_ADDRESS_INVALID;
  int rv = bind(socket_, storage.addr, storage.addr_len);
  if (rv == 0)
    return OK;
  int last_error = errno;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (last_error == EINVAL)
    return ERR_ADDRESS_IN_USE;
#elif BUILDFLAG(IS_APPLE)
  if (last_error == EADDRNOTAVAIL)
    return ERR_ADDRESS_IN_USE;
#endif
  return MapSystemError(last_error);
}

int UDPSocketPosix::RandomBind(const IPAddress& address) {
  DCHECK_EQ(bind_type_, DatagramSocket::RANDOM_BIND);

  for (int i = 0; i < kBindRetries; ++i) {
    int rv = DoBind(IPEndPoint(address, base::RandInt(kPortStart, kPortEnd)));
    if (rv != ERR_ADDRESS_IN_USE)
      return rv;
  }

  return DoBind(IPEndPoint(address, 0));
}

int UDPSocketPosix::JoinGroup(const IPAddress& group_address) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!is_connected())
    return ERR_SOCKET_NOT_CONNECTED;

  switch (group_address.size()) {
    case IPAddress::kIPv4AddressSize: {
      if (addr_family_ != AF_INET)
        return ERR_ADDRESS_INVALID;
      ip_mreqn mreq = {};
      mreq.imr_ifindex = multicast_interface_;
      mreq.imr_address.s_addr = htonl(INADDR_ANY);
      memcpy(&mreq.imr_multiaddr, group_address.bytes().data(),
             IPAddress::kIPv4AddressSize);
      int rv = setsockopt(socket_, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                          &mreq, sizeof(mreq));
      if (rv < 0)
        return MapSystemError(errno);
      return OK;
    }
    case IPAddress::kIPv6AddressSize: {
      if (addr_family_ != AF_INET6)
        return ERR_ADDRESS_INVALID;
      ipv6_mreq mreq;
      mreq.ipv6mr_interface = multicast_interface_;
      memcpy(&mreq.ipv6mr_multiaddr, group_address.bytes().data(),
             IPAddress::kIPv6AddressSize);
      int rv = setsockopt(socket_, IPPROTO_IPV6, IPV6_JOIN_GROUP,
                          &mreq, sizeof(mreq));
      if (rv < 0)
        return MapSystemError(errno);
      return OK;
    }
    default:
      NOTREACHED_IN_MIGRATION() << "Invalid address family";
      return ERR_ADDRESS_INVALID;
  }
}

int UDPSocketPosix::LeaveGroup(const IPAddress& group_address) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!is_connected())
    return ERR_SOCKET_NOT_CONNECTED;

  switch (group_address.size()) {
    case IPAddress::kIPv4AddressSize: {
      if (addr_family_ != AF_INET)
        return ERR_ADDRESS_INVALID;
      ip_mreqn mreq = {};
      mreq.imr_ifindex = multicast_interface_;
      mreq.imr_address.s_addr = INADDR_ANY;
      memcpy(&mreq.imr_multiaddr, group_address.bytes().data(),
             IPAddress::kIPv4AddressSize);
      int rv = setsockopt(socket_, IPPROTO_IP, IP_DROP_MEMBERSHIP,
                          &mreq, sizeof(mreq));
      if (rv < 0)
        return MapSystemError(errno);
      return OK;
    }
    case IPAddress::kIPv6AddressSize: {
      if (addr_family_ != AF_INET6)
        return ERR_ADDRESS_INVALID;
      ipv6_mreq mreq;
#if BUILDFLAG(IS_FUCHSIA)
      mreq.ipv6mr_interface = multicast_interface_;
#else   // BUILDFLAG(IS_FUCHSIA)
      mreq.ipv6mr_interface = 0;  // 0 indicates default multicast interface.
#endif  // !BUILDFLAG(IS_FUCHSIA)
      memcpy(&mreq.ipv6mr_multiaddr, group_address.bytes().data(),
             IPAddress::kIPv6AddressSize);
      int rv = setsockopt(socket_, IPPROTO_IPV6, IPV6_LEAVE_GROUP,
                          &mreq, sizeof(mreq));
      if (rv < 0)
        return MapSystemError(errno);
      return OK;
    }
    default:
      NOTREACHED_IN_MIGRATION() << "Invalid address family";
      return ERR_ADDRESS_INVALID;
  }
}

int UDPSocketPosix::SetMulticastInterface(uint32_t interface_index) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (is_connected())
    return ERR_SOCKET_IS_CONNECTED;
  multicast_interface_ = interface_index;
  return OK;
}

int UDPSocketPosix::SetMulticastTimeToLive(int time_to_live) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (is_connected())
    return ERR_SOCKET_IS_CONNECTED;

  if (time_to_live < 0 || time_to_live > 255)
    return ERR_INVALID_ARGUMENT;
  multicast_time_to_live_ = time_to_live;
  return OK;
}

int UDPSocketPosix::SetMulticastLoopbackMode(bool loopback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (is_connected())
    return ERR_SOCKET_IS_CONNECTED;

  if (loopback)
    socket_options_ |= SOCKET_OPTION_MULTICAST_LOOP;
  else
    socket_options_ &= ~SOCKET_OPTION_MULTICAST_LOOP;
  return OK;
}

int UDPSocketPosix::SetDiffServCodePoint(DiffServCodePoint dscp) {
  return SetTos(dscp, ECN_NO_CHANGE);
}

int UDPSocketPosix::SetTos(DiffServCodePoint dscp, EcnCodePoint ecn) {
  if (dscp == DSCP_NO_CHANGE && ecn == ECN_NO_CHANGE) {
    return OK;
  }
  int dscp_and_ecn = (dscp << 2) | ecn;
  socklen_t size = sizeof(dscp_and_ecn);
  if (dscp == DSCP_NO_CHANGE || ecn == ECN_NO_CHANGE) {
    int rv;
    if (addr_family_ == AF_INET) {
      rv = getsockopt(socket_, IPPROTO_IP, IP_TOS, &dscp_and_ecn, &size);
    } else {
      rv = getsockopt(socket_, IPPROTO_IPV6, IPV6_TCLASS, &dscp_and_ecn, &size);
    }
    if (rv < 0) {
      return MapSystemError(errno);
    }
    if (dscp == DSCP_NO_CHANGE) {
      dscp_and_ecn &= ~ECN_LAST;
      dscp_and_ecn |= ecn;
    } else {
      dscp_and_ecn &= ECN_LAST;
      dscp_and_ecn |= (dscp << 2);
    }
  }
  // Set the IPv4 option in all cases to support dual-stack sockets.
  int rv = setsockopt(socket_, IPPROTO_IP, IP_TOS, &dscp_and_ecn,
                      sizeof(dscp_and_ecn));
  if (addr_family_ == AF_INET6) {
    // In the IPv6 case, the previous socksetopt may fail because of a lack of
    // dual-stack support. Therefore ignore the previous return value.
    rv = setsockopt(socket_, IPPROTO_IPV6, IPV6_TCLASS,
                    &dscp_and_ecn, sizeof(dscp_and_ecn));
  }
  if (rv < 0)
    return MapSystemError(errno);
  return OK;
}

int UDPSocketPosix::SetIPv6Only(bool ipv6_only) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (is_connected()) {
    return ERR_SOCKET_IS_CONNECTED;
  }
  return net::SetIPv6Only(socket_, ipv6_only);
}

void UDPSocketPosix::DetachFromThread() {
  DETACH_FROM_THREAD(thread_checker_);
}

void UDPSocketPosix::ApplySocketTag(const SocketTag& tag) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (socket_ != kInvalidSocket && tag != tag_) {
    tag.Apply(socket_);
  }
  tag_ = tag;
}

int UDPSocketPosix::SetIOSNetworkServiceType(int ios_network_service_type) {
  if (ios_network_service_type == 0) {
    return OK;
  }
#if BUILDFLAG(IS_IOS)
  if (setsockopt(socket_, SOL_SOCKET, SO_NET_SERVICE_TYPE,
                 &ios_network_service_type, sizeof(ios_network_service_type))) {
    return MapSystemError(errno);
  }
#endif  // BUILDFLAG(IS_IOS)
  return OK;
}

}  // namespace net
