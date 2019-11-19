// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/udp_socket_posix.h"

#include <errno.h>
#include <fcntl.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/ioctl.h>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/containers/stack_container.h"
#include "base/debug/alias.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_current.h"
#include "base/metrics/histogram_functions.h"
#include "base/posix/eintr_wrapper.h"
#include "base/rand_util.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/network_activity_monitor.h"
#include "net/base/sockaddr_storage.h"
#include "net/base/trace_constants.h"
#include "net/log/net_log.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source.h"
#include "net/log/net_log_source_type.h"
#include "net/socket/socket_descriptor.h"
#include "net/socket/socket_options.h"
#include "net/socket/socket_tag.h"
#include "net/socket/udp_net_log_parameters.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

#if defined(OS_ANDROID)
#include <dlfcn.h>
#include "base/android/build_info.h"
#include "base/native_library.h"
#include "base/strings/utf_string_conversions.h"
#endif  // defined(OS_ANDROID)

#if defined(OS_MACOSX) && !defined(OS_IOS)
// This was needed to debug crbug.com/640281.
// TODO(zhongyi): Remove once the bug is resolved.
#include <dlfcn.h>
#include <pthread.h>
#endif  // defined(OS_MACOSX) && !defined(OS_IOS)

namespace net {

namespace {

const int kBindRetries = 10;
const int kPortStart = 1024;
const int kPortEnd = 65535;
const int kActivityMonitorBytesThreshold = 65535;
const int kActivityMonitorMinimumSamplesForThroughputEstimate = 2;
const base::TimeDelta kActivityMonitorMsThreshold =
    base::TimeDelta::FromMilliseconds(100);

#if defined(OS_MACOSX)
// When enabling multicast using setsockopt(IP_MULTICAST_IF) MacOS
// requires passing IPv4 address instead of interface index. This function
// resolves IPv4 address by interface index. The |address| is returned in
// network order.
int GetIPv4AddressFromIndex(int socket, uint32_t index, uint32_t* address) {
  if (!index) {
    *address = htonl(INADDR_ANY);
    return OK;
  }

  sockaddr_in* result = nullptr;

  ifreq ifr;
  ifr.ifr_addr.sa_family = AF_INET;
  if (!if_indextoname(index, ifr.ifr_name))
    return MapSystemError(errno);
  int rv = ioctl(socket, SIOCGIFADDR, &ifr);
  if (rv == -1)
    return MapSystemError(errno);
  result = reinterpret_cast<sockaddr_in*>(&ifr.ifr_addr);

  if (!result)
    return ERR_ADDRESS_INVALID;

  *address = result->sin_addr.s_addr;
  return OK;
}

#endif  // OS_MACOSX

#if defined(OS_MACOSX) && !defined(OS_IOS)

// On OSX the file descriptor is guarded to detect the cause of
// crbug.com/640281. guarded API is supported only on newer versions of OSX,
// so the symbols need to be resolved dynamically.
// TODO(zhongyi): Removed this code once the bug is resolved.

typedef uint64_t guardid_t;

typedef int (*GuardedCloseNpFunction)(int fd, const guardid_t* guard);
typedef int (*ChangeFdguardNpFunction)(int fd,
                                       const guardid_t* guard,
                                       u_int flags,
                                       const guardid_t* nguard,
                                       u_int nflags,
                                       int* fdflagsp);

GuardedCloseNpFunction g_guarded_close_np = nullptr;
ChangeFdguardNpFunction g_change_fdguard_np = nullptr;

pthread_once_t g_guarded_functions_once = PTHREAD_ONCE_INIT;

void InitGuardedFunctions() {
  void* libsystem_handle =
      dlopen("/usr/lib/libSystem.dylib", RTLD_LAZY | RTLD_LOCAL | RTLD_NOLOAD);
  if (libsystem_handle) {
    g_guarded_close_np = reinterpret_cast<GuardedCloseNpFunction>(
        dlsym(libsystem_handle, "guarded_close_np"));
    g_change_fdguard_np = reinterpret_cast<ChangeFdguardNpFunction>(
        dlsym(libsystem_handle, "change_fdguard_np"));

    // If for any reason only one of the functions is found, set both of them to
    // nullptr.
    if (!g_guarded_close_np || !g_change_fdguard_np) {
      g_guarded_close_np = nullptr;
      g_change_fdguard_np = nullptr;
    }
  }
}

int change_fdguard_np(int fd,
                      const guardid_t* guard,
                      u_int flags,
                      const guardid_t* nguard,
                      u_int nflags,
                      int* fdflagsp) {
  CHECK_EQ(pthread_once(&g_guarded_functions_once, InitGuardedFunctions), 0);
  // Older version of OSX may not support guarded API.
  if (!g_change_fdguard_np)
    return 0;
  return g_change_fdguard_np(fd, guard, flags, nguard, nflags, fdflagsp);
}

int guarded_close_np(int fd, const guardid_t* guard) {
  // Older version of OSX may not support guarded API.
  if (!g_guarded_close_np)
    return close(fd);

  return g_guarded_close_np(fd, guard);
}

const unsigned int GUARD_CLOSE = 1u << 0;
const unsigned int GUARD_DUP = 1u << 1;

const guardid_t kSocketFdGuard = 0xD712BC0BC9A4EAD4;

#endif  // defined(OS_MACOSX) && !defined(OS_IOS)

int GetSocketFDHash(int fd) {
  return fd ^ 1595649551;
}

}  // namespace

UDPSocketPosix::UDPSocketPosix(DatagramSocket::BindType bind_type,
                               net::NetLog* net_log,
                               const net::NetLogSource& source)
    : write_async_watcher_(std::make_unique<WriteAsyncWatcher>(this)),
      sender_(new UDPSocketPosixSender()),
      socket_(kInvalidSocket),
      socket_hash_(0),
      addr_family_(0),
      is_connected_(false),
      socket_options_(SOCKET_OPTION_MULTICAST_LOOP),
      sendto_flags_(0),
      multicast_interface_(0),
      multicast_time_to_live_(1),
      bind_type_(bind_type),
      read_socket_watcher_(FROM_HERE),
      write_socket_watcher_(FROM_HERE),
      read_watcher_(this),
      write_watcher_(this),
      last_async_result_(0),
      write_async_timer_running_(false),
      write_async_outstanding_(0),
      read_buf_len_(0),
      recv_from_address_(NULL),
      write_buf_len_(0),
      net_log_(NetLogWithSource::Make(net_log, NetLogSourceType::UDP_SOCKET)),
      bound_network_(NetworkChangeNotifier::kInvalidNetworkHandle),
      experimental_recv_optimization_enabled_(false) {
  net_log_.BeginEventReferencingSource(NetLogEventType::SOCKET_ALIVE, source);
}

UDPSocketPosix::~UDPSocketPosix() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  Close();
  net_log_.EndEvent(NetLogEventType::SOCKET_ALIVE);
}

int UDPSocketPosix::Open(AddressFamily address_family) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_EQ(socket_, kInvalidSocket);

  addr_family_ = ConvertAddressFamily(address_family);
  socket_ = CreatePlatformSocket(addr_family_, SOCK_DGRAM, 0);
  if (socket_ == kInvalidSocket)
    return MapSystemError(errno);
#if defined(OS_MACOSX) && !defined(OS_IOS)
  PCHECK(change_fdguard_np(socket_, NULL, 0, &kSocketFdGuard,
                           GUARD_CLOSE | GUARD_DUP, NULL) == 0);
#endif  // defined(OS_MACOSX) && !defined(OS_IOS)
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

void UDPSocketPosix::ActivityMonitor::Increment(uint32_t bytes) {
  if (!bytes)
    return;
  bool timer_running = timer_.IsRunning();
  bytes_ += bytes;
  increments_++;
  // Allow initial updates to make sure throughput estimator has
  // enough samples to generate a value. (low water mark)
  // Or once the bytes threshold has be met. (high water mark)
  if (increments_ < kActivityMonitorMinimumSamplesForThroughputEstimate ||
      bytes_ > kActivityMonitorBytesThreshold) {
    Update();
    if (timer_running)
      timer_.Reset();
  }
  if (!timer_running) {
    timer_.Start(FROM_HERE, kActivityMonitorMsThreshold, this,
                 &UDPSocketPosix::ActivityMonitor::OnTimerFired);
  }
}

void UDPSocketPosix::ActivityMonitor::Update() {
  if (!bytes_)
    return;
  NetworkActivityMonitorIncrement(bytes_);
  bytes_ = 0;
}

void UDPSocketPosix::ActivityMonitor::OnClose() {
  timer_.Stop();
  Update();
}

void UDPSocketPosix::ActivityMonitor::OnTimerFired() {
  increments_ = 0;
  if (!bytes_) {
    // Can happen if the socket has been idle and have had no
    // increments since the timer previously fired.  Don't bother
    // keeping the timer running in this case.
    timer_.Stop();
    return;
  }
  Update();
}

void UDPSocketPosix::SentActivityMonitor::NetworkActivityMonitorIncrement(
    uint32_t bytes) {
  NetworkActivityMonitor::GetInstance()->IncrementBytesSent(bytes);
}

void UDPSocketPosix::ReceivedActivityMonitor::NetworkActivityMonitorIncrement(
    uint32_t bytes) {
  NetworkActivityMonitor::GetInstance()->IncrementBytesReceived(bytes);
}

void UDPSocketPosix::Close() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (socket_ == kInvalidSocket)
    return;

  // Zero out any pending read/write callback state.
  read_buf_.reset();
  read_buf_len_ = 0;
  read_callback_.Reset();
  recv_from_address_ = NULL;
  write_buf_.reset();
  write_buf_len_ = 0;
  write_callback_.Reset();
  send_to_address_.reset();

  bool ok = read_socket_watcher_.StopWatchingFileDescriptor();
  DCHECK(ok);
  ok = write_socket_watcher_.StopWatchingFileDescriptor();
  DCHECK(ok);

  // Verify that |socket_| hasn't been corrupted. Needed to debug
  // crbug.com/906005.
  CHECK_EQ(socket_hash_, GetSocketFDHash(socket_));
#if defined(OS_MACOSX) && !defined(OS_IOS)
  PCHECK(IGNORE_EINTR(guarded_close_np(socket_, &kSocketFdGuard)) == 0);
#else
  PCHECK(IGNORE_EINTR(close(socket_)) == 0);
#endif  // defined(OS_MACOSX) && !defined(OS_IOS)

  socket_ = kInvalidSocket;
  addr_family_ = 0;
  is_connected_ = false;
  tag_ = SocketTag();

  write_async_timer_.Stop();
  sent_activity_monitor_.OnClose();
  received_activity_monitor_.OnClose();
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
    std::unique_ptr<IPEndPoint> address(new IPEndPoint());
    if (!address->FromSockAddr(storage.addr, storage.addr_len))
      return ERR_ADDRESS_INVALID;
    remote_address_ = std::move(address);
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
    std::unique_ptr<IPEndPoint> address(new IPEndPoint());
    if (!address->FromSockAddr(storage.addr, storage.addr_len))
      return ERR_ADDRESS_INVALID;
    local_address_ = std::move(address);
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
  return RecvFrom(buf, buf_len, NULL, std::move(callback));
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

  if (!base::MessageLoopCurrentForIO::Get()->WatchFileDescriptor(
          socket_, true, base::MessagePumpForIO::WATCH_READ,
          &read_socket_watcher_, &read_watcher_)) {
    PLOG(ERROR) << "WatchFileDescriptor failed on read";
    int result = MapSystemError(errno);
    LogRead(result, NULL, 0, NULL);
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
  return SendToOrWrite(buf, buf_len, NULL, std::move(callback));
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

  int result = InternalSendTo(buf, buf_len, address);
  if (result != ERR_IO_PENDING)
    return result;

  if (!base::MessageLoopCurrentForIO::Get()->WatchFileDescriptor(
          socket_, true, base::MessagePumpForIO::WATCH_WRITE,
          &write_socket_watcher_, &write_watcher_)) {
    DVPLOG(1) << "WatchFileDescriptor failed on write";
    int result = MapSystemError(errno);
    LogWrite(result, NULL, NULL);
    return result;
  }

  write_buf_ = buf;
  write_buf_len_ = buf_len;
  DCHECK(!send_to_address_.get());
  if (address) {
    send_to_address_.reset(new IPEndPoint(*address));
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
    base::UmaHistogramSparse("Net.UdpSocketRandomBindErrorCode", -rv);
    return rv;
  }

  SockaddrStorage storage;
  if (!address.ToSockAddr(storage.addr, &storage.addr_len))
    return ERR_ADDRESS_INVALID;

  rv = HANDLE_EINTR(connect(socket_, storage.addr, storage.addr_len));
  if (rv < 0)
    return MapSystemError(errno);

  remote_address_.reset(new IPEndPoint(address));
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

int UDPSocketPosix::BindToNetwork(
    NetworkChangeNotifier::NetworkHandle network) {
  DCHECK_NE(socket_, kInvalidSocket);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!is_connected());
  if (network == NetworkChangeNotifier::kInvalidNetworkHandle)
    return ERR_INVALID_ARGUMENT;
#if defined(OS_ANDROID)
  // Android prior to Lollipop didn't have support for binding sockets to
  // networks.
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SDK_VERSION_LOLLIPOP) {
    return ERR_NOT_IMPLEMENTED;
  }
  int rv;
  // On Android M and newer releases use supported NDK API. On Android L use
  // setNetworkForSocket from libnetd_client.so.
  if (base::android::BuildInfo::GetInstance()->sdk_int() >=
      base::android::SDK_VERSION_MARSHMALLOW) {
    // See declaration of android_setsocknetwork() here:
    // http://androidxref.com/6.0.0_r1/xref/development/ndk/platforms/android-M/include/android/multinetwork.h#65
    // Function cannot be called directly as it will cause app to fail to load
    // on pre-marshmallow devices.
    typedef int (*MarshmallowSetNetworkForSocket)(int64_t netId, int socketFd);
    static MarshmallowSetNetworkForSocket marshmallowSetNetworkForSocket;
    // This is racy, but all racers should come out with the same answer so it
    // shouldn't matter.
    if (!marshmallowSetNetworkForSocket) {
      base::FilePath file(base::GetNativeLibraryName("android"));
      void* dl = dlopen(file.value().c_str(), RTLD_NOW);
      marshmallowSetNetworkForSocket =
          reinterpret_cast<MarshmallowSetNetworkForSocket>(
              dlsym(dl, "android_setsocknetwork"));
    }
    if (!marshmallowSetNetworkForSocket)
      return ERR_NOT_IMPLEMENTED;
    rv = marshmallowSetNetworkForSocket(network, socket_);
    if (rv)
      rv = errno;
  } else {
    // NOTE(pauljensen): This does rely on Android implementation details, but
    // they won't change because Lollipop is already released.
    typedef int (*LollipopSetNetworkForSocket)(unsigned netId, int socketFd);
    static LollipopSetNetworkForSocket lollipopSetNetworkForSocket;
    // This is racy, but all racers should come out with the same answer so it
    // shouldn't matter.
    if (!lollipopSetNetworkForSocket) {
      // Android's netd client library should always be loaded in our address
      // space as it shims socket() which was used to create |socket_|.
      base::FilePath file(base::GetNativeLibraryName("netd_client"));
      // Use RTLD_NOW to match Android's prior loading of the library:
      // http://androidxref.com/6.0.0_r5/xref/bionic/libc/bionic/NetdClient.cpp#37
      // Use RTLD_NOLOAD to assert that the library is already loaded and
      // avoid doing any disk IO.
      void* dl = dlopen(file.value().c_str(), RTLD_NOW | RTLD_NOLOAD);
      lollipopSetNetworkForSocket =
          reinterpret_cast<LollipopSetNetworkForSocket>(
              dlsym(dl, "setNetworkForSocket"));
    }
    if (!lollipopSetNetworkForSocket)
      return ERR_NOT_IMPLEMENTED;
    rv = -lollipopSetNetworkForSocket(network, socket_);
  }
  // If |network| has since disconnected, |rv| will be ENONET.  Surface this as
  // ERR_NETWORK_CHANGED, rather than MapSystemError(ENONET) which gives back
  // the less descriptive ERR_FAILED.
  if (rv == ENONET)
    return ERR_NETWORK_CHANGED;
  if (rv == 0)
    bound_network_ = network;
  return MapSystemError(rv);
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

#if !defined(IP_PMTUDISC_DO)
  return ERR_NOT_IMPLEMENTED;
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

void UDPSocketPosix::SetMsgConfirm(bool confirm) {
#if !defined(OS_MACOSX) && !defined(OS_IOS)
  if (confirm) {
    sendto_flags_ |= MSG_CONFIRM;
  } else {
    sendto_flags_ &= ~MSG_CONFIRM;
  }
#endif  // !defined(OS_MACOSX) && !defined(OS_IOS)
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
#if defined(OS_MACOSX)
  // SO_REUSEPORT on OSX permits multiple processes to each receive
  // UDP multicast or broadcast datagrams destined for the bound
  // port.
  // This is only being set on OSX because its behavior is platform dependent
  // and we are playing it safe by only setting it on platforms where things
  // break.
  rv = setsockopt(socket_, SOL_SOCKET, SO_REUSEPORT, &value, sizeof(value));
  if (rv != 0)
    return MapSystemError(errno);
#endif  // defined(OS_MACOSX)
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
  TRACE_EVENT0(NetTracingCategory(),
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
    recv_from_address_ = NULL;
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

  received_activity_monitor_.Increment(result);
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

  sent_activity_monitor_.Increment(result);
}

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
  int bytes_transferred;
  bytes_transferred = HANDLE_EINTR(read(socket_, buf->data(), buf_len));
  int result;

  if (bytes_transferred < 0) {
    result = MapSystemError(errno);
  } else if (bytes_transferred == buf_len) {
    result = ERR_MSG_TOO_BIG;
  } else {
    result = bytes_transferred;
    if (address)
      *address = *remote_address_.get();
  }

  if (result != ERR_IO_PENDING) {
    SockaddrStorage sock_addr;
    bool success =
        remote_address_->ToSockAddr(sock_addr.addr, &sock_addr.addr_len);
    DCHECK(success);
    LogRead(result, buf->data(), sock_addr.addr_len, sock_addr.addr);
  }
  return result;
}

int UDPSocketPosix::InternalRecvFromNonConnectedSocket(IOBuffer* buf,
                                                       int buf_len,
                                                       IPEndPoint* address) {
  int bytes_transferred;

  struct iovec iov = {};
  iov.iov_base = buf->data();
  iov.iov_len = buf_len;

  struct msghdr msg = {};
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;

  SockaddrStorage storage;
  msg.msg_name = storage.addr;
  msg.msg_namelen = storage.addr_len;

  bytes_transferred = HANDLE_EINTR(recvmsg(socket_, &msg, 0));
  storage.addr_len = msg.msg_namelen;
  int result;
  if (bytes_transferred >= 0) {
    if (msg.msg_flags & MSG_TRUNC) {
      result = ERR_MSG_TOO_BIG;
    } else {
      result = bytes_transferred;
      if (address && !address->FromSockAddr(storage.addr, storage.addr_len))
        result = ERR_ADDRESS_INVALID;
    }
  } else {
    result = MapSystemError(errno);
  }
  if (result != ERR_IO_PENDING)
    LogRead(result, buf->data(), storage.addr_len, storage.addr);
  return result;
}

int UDPSocketPosix::InternalSendTo(IOBuffer* buf,
                                   int buf_len,
                                   const IPEndPoint* address) {
  SockaddrStorage storage;
  struct sockaddr* addr = storage.addr;
  if (!address) {
    addr = NULL;
    storage.addr_len = 0;
  } else {
    if (!address->ToSockAddr(storage.addr, &storage.addr_len)) {
      int result = ERR_ADDRESS_INVALID;
      LogWrite(result, NULL, NULL);
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
#if defined(OS_MACOSX)
        ip_mreq mreq = {};
        int error = GetIPv4AddressFromIndex(socket_, multicast_interface_,
                                            &mreq.imr_interface.s_addr);
        if (error != OK)
          return error;
#else   //  defined(OS_MACOSX)
        ip_mreqn mreq = {};
        mreq.imr_ifindex = multicast_interface_;
        mreq.imr_address.s_addr = htonl(INADDR_ANY);
#endif  //  !defined(OS_MACOSX)
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
        NOTREACHED() << "Invalid address family";
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
#if defined(OS_CHROMEOS)
  if (last_error == EINVAL)
    return ERR_ADDRESS_IN_USE;
#elif defined(OS_MACOSX)
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

#if defined(OS_MACOSX)
      ip_mreq mreq = {};
      int error = GetIPv4AddressFromIndex(socket_, multicast_interface_,
                                          &mreq.imr_interface.s_addr);
      if (error != OK)
        return error;
#else
      ip_mreqn mreq = {};
      mreq.imr_ifindex = multicast_interface_;
      mreq.imr_address.s_addr = htonl(INADDR_ANY);
#endif
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
      NOTREACHED() << "Invalid address family";
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
#if defined(OS_FUCHSIA)
      mreq.ipv6mr_interface = multicast_interface_;
#else   // defined(OS_FUCHSIA)
      mreq.ipv6mr_interface = 0;  // 0 indicates default multicast interface.
#endif  // !defined(OS_FUCHSIA)
      memcpy(&mreq.ipv6mr_multiaddr, group_address.bytes().data(),
             IPAddress::kIPv6AddressSize);
      int rv = setsockopt(socket_, IPPROTO_IPV6, IPV6_LEAVE_GROUP,
                          &mreq, sizeof(mreq));
      if (rv < 0)
        return MapSystemError(errno);
      return OK;
    }
    default:
      NOTREACHED() << "Invalid address family";
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
  if (dscp == DSCP_NO_CHANGE) {
    return OK;
  }

  int dscp_and_ecn = dscp << 2;
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

UDPSocketPosixSender::UDPSocketPosixSender() : sendmmsg_enabled_(false) {}
UDPSocketPosixSender::~UDPSocketPosixSender() {}

SendResult::SendResult() : rv(0), write_count(0) {}
SendResult::~SendResult() {}
SendResult::SendResult(int _rv, int _write_count, DatagramBuffers _buffers)
    : rv(_rv), write_count(_write_count), buffers(std::move(_buffers)) {}
SendResult::SendResult(SendResult&& other) = default;

SendResult UDPSocketPosixSender::InternalSendBuffers(
    int fd,
    DatagramBuffers buffers) const {
  int rv = 0;
  int write_count = 0;
  for (auto& buffer : buffers) {
    int result = HANDLE_EINTR(Send(fd, buffer->data(), buffer->length(), 0));
    if (result < 0) {
      rv = MapSystemError(errno);
      break;
    }
    write_count++;
  }
  return SendResult(rv, write_count, std::move(buffers));
}

#if HAVE_SENDMMSG
SendResult UDPSocketPosixSender::InternalSendmmsgBuffers(
    int fd,
    DatagramBuffers buffers) const {
  base::StackVector<struct iovec, kWriteAsyncMaxBuffersThreshold + 1> msg_iov;
  base::StackVector<struct mmsghdr, kWriteAsyncMaxBuffersThreshold + 1> msgvec;
  msg_iov->reserve(buffers.size());
  for (auto& buffer : buffers)
    msg_iov->push_back({const_cast<char*>(buffer->data()), buffer->length()});
  msgvec->reserve(buffers.size());
  for (size_t j = 0; j < buffers.size(); j++)
    msgvec->push_back({{nullptr, 0, &msg_iov[j], 1, nullptr, 0, 0}, 0});
  int result = HANDLE_EINTR(Sendmmsg(fd, &msgvec[0], buffers.size(), 0));
  SendResult send_result(0, 0, std::move(buffers));
  if (result < 0) {
    send_result.rv = MapSystemError(errno);
  } else {
    send_result.write_count = result;
  }
  return send_result;
}
#endif

SendResult UDPSocketPosixSender::SendBuffers(int fd, DatagramBuffers buffers) {
#if HAVE_SENDMMSG
  if (sendmmsg_enabled_) {
    auto result = InternalSendmmsgBuffers(fd, std::move(buffers));
    if (LIKELY(result.rv != ERR_NOT_IMPLEMENTED)) {
      return result;
    }
    DLOG(WARNING) << "senddmsg() not implemented, falling back to send()";
    sendmmsg_enabled_ = false;
    buffers = std::move(result.buffers);
  }
#endif
  return InternalSendBuffers(fd, std::move(buffers));
}

ssize_t UDPSocketPosixSender::Send(int sockfd,
                                   const void* buf,
                                   size_t len,
                                   int flags) const {
  return send(sockfd, buf, len, flags);
}

#if HAVE_SENDMMSG
int UDPSocketPosixSender::Sendmmsg(int sockfd,
                                   struct mmsghdr* msgvec,
                                   unsigned int vlen,
                                   unsigned int flags) const {
  return sendmmsg(sockfd, msgvec, vlen, flags);
}
#endif

int UDPSocketPosix::WriteAsync(
    const char* buffer,
    size_t buf_len,
    CompletionOnceCallback callback,
    const NetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK(datagram_buffer_pool_ != nullptr);
  IncreaseWriteAsyncOutstanding(1);
  datagram_buffer_pool_->Enqueue(buffer, buf_len, &pending_writes_);
  return InternalWriteAsync(std::move(callback), traffic_annotation);
}

int UDPSocketPosix::WriteAsync(
    DatagramBuffers buffers,
    CompletionOnceCallback callback,
    const NetworkTrafficAnnotationTag& traffic_annotation) {
  IncreaseWriteAsyncOutstanding(buffers.size());
  pending_writes_.splice(pending_writes_.end(), std::move(buffers));
  return InternalWriteAsync(std::move(callback), traffic_annotation);
}

int UDPSocketPosix::InternalWriteAsync(
    CompletionOnceCallback callback,
    const NetworkTrafficAnnotationTag& traffic_annotation) {
  CHECK(write_callback_.is_null());

  // Surface error immediately if one is pending.
  if (last_async_result_ < 0) {
    return ResetLastAsyncResult();
  }

  size_t flush_threshold =
      write_batching_active_ ? kWriteAsyncPostBuffersThreshold : 1;
  if (pending_writes_.size() >= flush_threshold) {
    FlushPending();
    // Surface error immediately if one is pending.
    if (last_async_result_ < 0) {
      return ResetLastAsyncResult();
    }
  }

  if (!write_async_timer_running_) {
    write_async_timer_running_ = true;
    write_async_timer_.Start(FROM_HERE, kWriteAsyncMsThreshold, this,
                             &UDPSocketPosix::OnWriteAsyncTimerFired);
  }

  int blocking_threshold =
      write_batching_active_ ? kWriteAsyncMaxBuffersThreshold : 1;
  if (write_async_outstanding_ >= blocking_threshold) {
    write_callback_ = std::move(callback);
    return ERR_IO_PENDING;
  }

  DVLOG(2) << __func__ << " pending " << pending_writes_.size()
           << " outstanding " << write_async_outstanding_;
  return ResetWrittenBytes();
}

DatagramBuffers UDPSocketPosix::GetUnwrittenBuffers() {
  write_async_outstanding_ -= pending_writes_.size();
  return std::move(pending_writes_);
}

void UDPSocketPosix::FlushPending() {
  // Nothing to do if socket is blocked.
  if (write_async_watcher_->watching())
    return;

  if (pending_writes_.empty())
    return;

  if (write_async_timer_running_)
    write_async_timer_.Reset();

  int num_pending_writes = static_cast<int>(pending_writes_.size());
  if (!write_multi_core_enabled_ ||
      // Don't bother with post if not enough buffers
      (num_pending_writes <= kWriteAsyncMinBuffersThreshold &&
       // but not if there is a previous post
       // outstanding, to prevent out of order transmission.
       (num_pending_writes == write_async_outstanding_))) {
    LocalSendBuffers();
  } else {
    PostSendBuffers();
  }
}

// TODO(ckrasic) Sad face.  Do this lazily because many tests exploded
// otherwise.  |threading_and_tasks.md| advises to instantiate a
// |base::test::TaskEnvironment| in the test, implementing that
// for all tests that might exercise QUIC is too daunting.  Also, in
// some tests it seemed like following the advice just broke in other
// ways.
base::SequencedTaskRunner* UDPSocketPosix::GetTaskRunner() {
  if (task_runner_ == nullptr) {
    task_runner_ =
        CreateSequencedTaskRunner(base::TaskTraits(base::ThreadPool()));
  }
  return task_runner_.get();
}

void UDPSocketPosix::OnWriteAsyncTimerFired() {
  DVLOG(2) << __func__ << " pending writes " << pending_writes_.size();
  if (pending_writes_.empty()) {
    write_async_timer_.Stop();
    write_async_timer_running_ = false;
    return;
  }
  if (last_async_result_ < 0) {
    DVLOG(1) << __func__ << " socket not writeable";
    return;
  }
  FlushPending();
}

void UDPSocketPosix::LocalSendBuffers() {
  DVLOG(1) << __func__ << " queue " << pending_writes_.size() << " out of "
           << write_async_outstanding_ << " total";
  DidSendBuffers(sender_->SendBuffers(socket_, std::move(pending_writes_)));
}

void UDPSocketPosix::PostSendBuffers() {
  DVLOG(1) << __func__ << " queue " << pending_writes_.size() << " out of "
           << write_async_outstanding_ << " total";
  base::PostTaskAndReplyWithResult(
      GetTaskRunner(), FROM_HERE,
      base::BindOnce(&UDPSocketPosixSender::SendBuffers, sender_, socket_,
                     std::move(pending_writes_)),
      base::BindOnce(&UDPSocketPosix::DidSendBuffers,
                     weak_factory_.GetWeakPtr()));
}

void UDPSocketPosix::DidSendBuffers(SendResult send_result) {
  DVLOG(3) << __func__;
  int write_count = send_result.write_count;
  DatagramBuffers& buffers = send_result.buffers;

  DCHECK(!buffers.empty());
  int num_buffers = buffers.size();

  // Dequeue buffers that have been written.
  if (write_count > 0) {
    write_async_outstanding_ -= write_count;

    DatagramBuffers::const_iterator it;
    // Generate logs for written buffers
    it = buffers.cbegin();
    for (int i = 0; i < write_count; i++, it++) {
      auto& buffer = *it;
      LogWrite(buffer->length(), buffer->data(), NULL);
      written_bytes_ += buffer->length();
    }
    // Return written buffers to pool
    DatagramBuffers written_buffers;
    if (write_count == num_buffers) {
      it = buffers.cend();
    } else {
      it = buffers.cbegin();
      for (int i = 0; i < write_count; i++) {
        it++;
      }
    }
    written_buffers.splice(written_buffers.cend(), buffers, buffers.cbegin(),
                           it);
    DCHECK(datagram_buffer_pool_ != nullptr);
    datagram_buffer_pool_->Dequeue(&written_buffers);
  }

  // Requeue left-over (unwritten) buffers.
  if (!buffers.empty()) {
    DVLOG(2) << __func__ << " requeue " << buffers.size() << " buffers";
    pending_writes_.splice(pending_writes_.begin(), std::move(buffers));
  }

  last_async_result_ = send_result.rv;
  if (last_async_result_ == ERR_IO_PENDING) {
    DVLOG(2) << __func__ << " WatchFileDescriptor start";
    if (!WatchFileDescriptor()) {
      DVPLOG(1) << "WatchFileDescriptor failed on write";
      last_async_result_ = MapSystemError(errno);
      LogWrite(last_async_result_, NULL, NULL);
    } else {
      last_async_result_ = 0;
    }
  } else if (last_async_result_ < 0 || pending_writes_.empty()) {
    DVLOG(2) << __func__ << " WatchFileDescriptor stop: result "
             << ErrorToShortString(last_async_result_) << " pending_writes "
             << pending_writes_.size();
    StopWatchingFileDescriptor();
  }
  DCHECK(last_async_result_ != ERR_IO_PENDING);

  if (write_callback_.is_null())
    return;

  if (last_async_result_ < 0) {
    DVLOG(1) << last_async_result_;
    // Update the writer with the latest result.
    DoWriteCallback(ResetLastAsyncResult());
  } else if (write_async_outstanding_ < kWriteAsyncCallbackBuffersThreshold) {
    DVLOG(1) << write_async_outstanding_ << " < "
             << kWriteAsyncCallbackBuffersThreshold;
    DoWriteCallback(ResetWrittenBytes());
  }
}

void UDPSocketPosix::WriteAsyncWatcher::OnFileCanWriteWithoutBlocking(int) {
  DVLOG(1) << __func__ << " queue " << socket_->pending_writes_.size()
           << " out of " << socket_->write_async_outstanding_ << " total";
  socket_->StopWatchingFileDescriptor();
  socket_->FlushPending();
}

bool UDPSocketPosix::WatchFileDescriptor() {
  if (write_async_watcher_->watching())
    return true;
  bool result = InternalWatchFileDescriptor();
  if (result) {
    write_async_watcher_->set_watching(true);
  }
  return result;
}

void UDPSocketPosix::StopWatchingFileDescriptor() {
  if (!write_async_watcher_->watching())
    return;
  InternalStopWatchingFileDescriptor();
  write_async_watcher_->set_watching(false);
}

bool UDPSocketPosix::InternalWatchFileDescriptor() {
  return base::MessageLoopCurrentForIO::Get()->WatchFileDescriptor(
      socket_, true, base::MessagePumpForIO::WATCH_WRITE,
      &write_socket_watcher_, write_async_watcher_.get());
}

void UDPSocketPosix::InternalStopWatchingFileDescriptor() {
  bool ok = write_socket_watcher_.StopWatchingFileDescriptor();
  DCHECK(ok);
}

void UDPSocketPosix::SetMaxPacketSize(size_t max_packet_size) {
  datagram_buffer_pool_ = std::make_unique<DatagramBufferPool>(max_packet_size);
}

int UDPSocketPosix::ResetLastAsyncResult() {
  int result = last_async_result_;
  last_async_result_ = 0;
  return result;
}

int UDPSocketPosix::ResetWrittenBytes() {
  int bytes = written_bytes_;
  written_bytes_ = 0;
  return bytes;
}

}  // namespace net
