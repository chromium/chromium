// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/tcp_socket.h"

#include <errno.h>
#include <netinet/tcp.h>
#include <sys/socket.h>

#include <algorithm>

#include "base/atomicops.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "net/base/address_list.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/network_activity_monitor.h"
#include "net/base/network_change_notifier.h"
#include "net/base/sockaddr_storage.h"
#include "net/http/http_util.h"
#include "net/log/net_log.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source.h"
#include "net/log/net_log_source_type.h"
#include "net/socket/socket_net_log_params.h"
#include "net/socket/socket_options.h"
#include "net/socket/socket_posix.h"
#include "net/socket/socket_tag.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

// If we don't have a definition for TCPI_OPT_SYN_DATA, create one.
#if !defined(TCPI_OPT_SYN_DATA)
#define TCPI_OPT_SYN_DATA 32
#endif

// Fuchsia defines TCP_INFO, but it's not implemented.
// TODO(crbug.com/758294): Enable TCP_INFO on Fuchsia once it's implemented
// there (see NET-160).
#if defined(TCP_INFO) && !defined(OS_FUCHSIA)
#define HAVE_TCP_INFO
#endif

namespace net {

namespace {

// True if TCP FastOpen connect-with-write has failed at least once.
bool g_tcp_fastopen_has_failed = false;

// SetTCPKeepAlive sets SO_KEEPALIVE.
bool SetTCPKeepAlive(int fd, bool enable, int delay) {
  // Enabling TCP keepalives is the same on all platforms.
  int on = enable ? 1 : 0;
  if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on))) {
    PLOG(ERROR) << "Failed to set SO_KEEPALIVE on fd: " << fd;
    return false;
  }

  // If we disabled TCP keep alive, our work is done here.
  if (!enable)
    return true;

#if defined(OS_LINUX) || defined(OS_ANDROID)
  // Setting the keepalive interval varies by platform.

  // Set seconds until first TCP keep alive.
  if (setsockopt(fd, SOL_TCP, TCP_KEEPIDLE, &delay, sizeof(delay))) {
    PLOG(ERROR) << "Failed to set TCP_KEEPIDLE on fd: " << fd;
    return false;
  }
  // Set seconds between TCP keep alives.
  if (setsockopt(fd, SOL_TCP, TCP_KEEPINTVL, &delay, sizeof(delay))) {
    PLOG(ERROR) << "Failed to set TCP_KEEPINTVL on fd: " << fd;
    return false;
  }
#elif defined(OS_MACOSX) || defined(OS_IOS)
  if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPALIVE, &delay, sizeof(delay))) {
    PLOG(ERROR) << "Failed to set TCP_KEEPALIVE on fd: " << fd;
    return false;
  }
#endif
  return true;
}

#if defined(OS_LINUX) || defined(OS_ANDROID)
// Probes if TCP FastOpen is supported, on another thread.
class FastOpenProbe {
 public:
  // Returns true if TCP FastOpen suport was detected. Returns false if it was
  // not detected, or the probe has not yet completed.
  bool IsTCPFastOpenSupported() const {
    return base::subtle::NoBarrier_Load(&tcp_fastopen_supported_) != 0;
  }

 private:
  friend struct base::LazyInstanceTraitsBase<FastOpenProbe>;

  FastOpenProbe() {
    base::PostTaskWithTraits(
        FROM_HERE,
        {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
        base::Bind(&FastOpenProbe::DetectTCPFastOpenSupport,
                   base::Unretained(this)));
  }

  ~FastOpenProbe() = default;

  // Checks if the kernel supports TCP FastOpen. Called only once, on startup.
  void DetectTCPFastOpenSupport() {
    // Since this method should only be called once, and is the only thing that
    // modifies |tcp_fastopen_supported_|, no need for this read to be atomic.
    DCHECK_EQ(tcp_fastopen_supported_, 0);

    const base::FilePath::CharType kTCPFastOpenProcFilePath[] =
        "/proc/sys/net/ipv4/tcp_fastopen";
    std::string system_supports_tcp_fastopen;
    if (!base::ReadFileToString(base::FilePath(kTCPFastOpenProcFilePath),
                                &system_supports_tcp_fastopen)) {
      return;
    }
    // The read value from /proc will be set in its least significant bit if
    // TCP FastOpen is enabled.
    int read_int = 0;
    base::StringToInt(
        HttpUtil::TrimLWS(base::StringPiece(system_supports_tcp_fastopen)),
        &read_int);
    if ((read_int & 0x1) != 1)
      return;
    base::subtle::NoBarrier_Store(&tcp_fastopen_supported_, 1);
  }

  base::subtle::Atomic32 tcp_fastopen_supported_ = 0;

  DISALLOW_COPY_AND_ASSIGN(FastOpenProbe);
};

base::LazyInstance<FastOpenProbe>::Leaky g_fast_open_probe =
    LAZY_INSTANCE_INITIALIZER;
#endif  // defined(OS_LINUX) || defined(OS_ANDROID)

#if defined(HAVE_TCP_INFO)
// Returns a zero value if the transport RTT is unavailable.
base::TimeDelta GetTransportRtt(SocketDescriptor fd) {
  // It is possible for the value returned by getsockopt(TCP_INFO) to be
  // legitimately zero due to the way the RTT is calculated where fractions are
  // rounded down. This is specially true for virtualized environments with
  // paravirtualized clocks.
  //
  // If getsockopt(TCP_INFO) succeeds and the tcpi_rtt is zero, this code
  // assumes that the RTT got rounded down to zero and rounds it back up to this
  // value so that callers can assume that no packets defy the laws of physics.
  constexpr uint32_t kMinValidRttMicros = 1;

  tcp_info info;
  // Reset |tcpi_rtt| to verify if getsockopt() actually updates |tcpi_rtt|.
  info.tcpi_rtt = 0;

  socklen_t info_len = sizeof(tcp_info);
  if (getsockopt(fd, IPPROTO_TCP, TCP_INFO, &info, &info_len) != 0)
    return base::TimeDelta();

  // Verify that |tcpi_rtt| in tcp_info struct was updated. Note that it's
  // possible that |info_len| is shorter than |sizeof(tcp_info)| which implies
  // that only a subset of values in |info| may have been updated by
  // getsockopt().
  if (info_len < static_cast<socklen_t>(offsetof(tcp_info, tcpi_rtt) +
                                        sizeof(info.tcpi_rtt))) {
    return base::TimeDelta();
  }

  return base::TimeDelta::FromMicroseconds(
      std::max(info.tcpi_rtt, kMinValidRttMicros));
}

// Returns true if getsockopt() call was successful. Sets
// |server_acked_syn_data| to true if SYN-ACK acked data in SYN sent or
// received.
bool GetServerAckedDataInSyn(SocketDescriptor fd, bool* server_acked_syn_data) {
  tcp_info info;
  // Reset |tcpi_options| to verify if getsockopt() actually updates
  // |tcpi_options|.
  info.tcpi_options = 0;

  socklen_t info_len = sizeof(tcp_info);
  if (getsockopt(fd, IPPROTO_TCP, TCP_INFO, &info, &info_len) != 0) {
    *server_acked_syn_data = false;
    return false;
  }

  // Verify that |tcpi_options| in tcp_info struct was updated. Note that it's
  // possible that |info_len| is shorter than |sizeof(tcp_info)| which implies
  // that only a subset of values in |info| may have been updated by
  // getsockopt().
  if (info_len < static_cast<socklen_t>(offsetof(tcp_info, tcpi_options) +
                                        sizeof(info.tcpi_options))) {
    *server_acked_syn_data = false;
    return false;
  }

  *server_acked_syn_data = (info.tcpi_options & TCPI_OPT_SYN_DATA);
  return true;
}

#endif  // defined(TCP_INFO)

}  // namespace

//-----------------------------------------------------------------------------

bool IsTCPFastOpenSupported() {
#if defined(OS_LINUX) || defined(OS_ANDROID)
  return g_fast_open_probe.Get().IsTCPFastOpenSupported();
#else
  return false;
#endif
}

TCPSocketPosix::TCPSocketPosix(
    std::unique_ptr<SocketPerformanceWatcher> socket_performance_watcher,
    NetLog* net_log,
    const NetLogSource& source)
    : socket_performance_watcher_(std::move(socket_performance_watcher)),
      use_tcp_fastopen_(false),
      tcp_fastopen_write_attempted_(false),
      tcp_fastopen_connected_(false),
      tcp_fastopen_status_(TCP_FASTOPEN_STATUS_UNKNOWN),
      logging_multiple_connect_attempts_(false),
      net_log_(NetLogWithSource::Make(net_log, NetLogSourceType::SOCKET)) {
  net_log_.BeginEvent(NetLogEventType::SOCKET_ALIVE,
                      source.ToEventParametersCallback());
}

TCPSocketPosix::~TCPSocketPosix() {
  net_log_.EndEvent(NetLogEventType::SOCKET_ALIVE);
  Close();
}

int TCPSocketPosix::Open(AddressFamily family) {
  DCHECK(!socket_);
  socket_.reset(new SocketPosix);
  int rv = socket_->Open(ConvertAddressFamily(family));
  if (rv != OK)
    socket_.reset();
  if (rv == OK && tag_ != SocketTag())
    tag_.Apply(socket_->socket_fd());
  return rv;
}

int TCPSocketPosix::AdoptConnectedSocket(SocketDescriptor socket,
                                         const IPEndPoint& peer_address) {
  DCHECK(!socket_);

  SockaddrStorage storage;
  if (!peer_address.ToSockAddr(storage.addr, &storage.addr_len) &&
      // For backward compatibility, allows the empty address.
      !(peer_address == IPEndPoint())) {
    return ERR_ADDRESS_INVALID;
  }

  socket_.reset(new SocketPosix);
  int rv = socket_->AdoptConnectedSocket(socket, storage);
  if (rv != OK)
    socket_.reset();
  if (rv == OK && tag_ != SocketTag())
    tag_.Apply(socket_->socket_fd());
  return rv;
}

int TCPSocketPosix::AdoptUnconnectedSocket(SocketDescriptor socket) {
  DCHECK(!socket_);

  socket_.reset(new SocketPosix);
  int rv = socket_->AdoptUnconnectedSocket(socket);
  if (rv != OK)
    socket_.reset();
  if (rv == OK && tag_ != SocketTag())
    tag_.Apply(socket_->socket_fd());
  return rv;
}

int TCPSocketPosix::Bind(const IPEndPoint& address) {
  DCHECK(socket_);

  SockaddrStorage storage;
  if (!address.ToSockAddr(storage.addr, &storage.addr_len))
    return ERR_ADDRESS_INVALID;

  return socket_->Bind(storage);
}

int TCPSocketPosix::Listen(int backlog) {
  DCHECK(socket_);
  return socket_->Listen(backlog);
}

int TCPSocketPosix::Accept(std::unique_ptr<TCPSocketPosix>* tcp_socket,
                           IPEndPoint* address,
                           CompletionOnceCallback callback) {
  DCHECK(tcp_socket);
  DCHECK(!callback.is_null());
  DCHECK(socket_);
  DCHECK(!accept_socket_);

  net_log_.BeginEvent(NetLogEventType::TCP_ACCEPT);

  int rv = socket_->Accept(
      &accept_socket_,
      base::BindOnce(&TCPSocketPosix::AcceptCompleted, base::Unretained(this),
                     tcp_socket, address, std::move(callback)));
  if (rv != ERR_IO_PENDING)
    rv = HandleAcceptCompleted(tcp_socket, address, rv);
  return rv;
}

int TCPSocketPosix::Connect(const IPEndPoint& address,
                            CompletionOnceCallback callback) {
  DCHECK(socket_);

  if (!logging_multiple_connect_attempts_)
    LogConnectBegin(AddressList(address));

  net_log_.BeginEvent(NetLogEventType::TCP_CONNECT_ATTEMPT,
                      CreateNetLogIPEndPointCallback(&address));

  SockaddrStorage storage;
  if (!address.ToSockAddr(storage.addr, &storage.addr_len))
    return ERR_ADDRESS_INVALID;

  if (use_tcp_fastopen_) {
    // With TCP FastOpen, we pretend that the socket is connected.
    DCHECK(!tcp_fastopen_write_attempted_);
    socket_->SetPeerAddress(storage);
    return OK;
  }

  int rv = socket_->Connect(
      storage, base::BindOnce(&TCPSocketPosix::ConnectCompleted,
                              base::Unretained(this), std::move(callback)));
  if (rv != ERR_IO_PENDING)
    rv = HandleConnectCompleted(rv);
  return rv;
}

bool TCPSocketPosix::IsConnected() const {
  if (!socket_)
    return false;

  if (use_tcp_fastopen_ && !tcp_fastopen_write_attempted_ &&
      socket_->HasPeerAddress()) {
    // With TCP FastOpen, we pretend that the socket is connected.
    // This allows GetPeerAddress() to return peer_address_.
    return true;
  }

  return socket_->IsConnected();
}

bool TCPSocketPosix::IsConnectedAndIdle() const {
  // TODO(wtc): should we also handle the TCP FastOpen case here,
  // as we do in IsConnected()?
  return socket_ && socket_->IsConnectedAndIdle();
}

int TCPSocketPosix::Read(IOBuffer* buf,
                         int buf_len,
                         CompletionOnceCallback callback) {
  DCHECK(socket_);
  DCHECK(!callback.is_null());

  int rv = socket_->Read(
      buf, buf_len,
      base::BindOnce(
          &TCPSocketPosix::ReadCompleted,
          // Grab a reference to |buf| so that ReadCompleted() can still
          // use it when Read() completes, as otherwise, this transfers
          // ownership of buf to socket.
          base::Unretained(this), base::WrapRefCounted(buf),
          std::move(callback)));
  if (rv != ERR_IO_PENDING)
    rv = HandleReadCompleted(buf, rv);
  return rv;
}

int TCPSocketPosix::ReadIfReady(IOBuffer* buf,
                                int buf_len,
                                CompletionOnceCallback callback) {
  DCHECK(socket_);
  DCHECK(!callback.is_null());

  int rv = socket_->ReadIfReady(
      buf, buf_len,
      base::BindOnce(&TCPSocketPosix::ReadIfReadyCompleted,
                     base::Unretained(this), std::move(callback)));
  if (rv != ERR_IO_PENDING)
    rv = HandleReadCompleted(buf, rv);
  return rv;
}

int TCPSocketPosix::CancelReadIfReady() {
  DCHECK(socket_);

  return socket_->CancelReadIfReady();
}

int TCPSocketPosix::Write(
    IOBuffer* buf,
    int buf_len,
    CompletionOnceCallback callback,
    const NetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK(socket_);
  DCHECK(!callback.is_null());

  CompletionOnceCallback write_callback = base::BindOnce(
      &TCPSocketPosix::WriteCompleted,
      // Grab a reference to |buf| so that WriteCompleted() can still
      // use it when Write() completes, as otherwise, this transfers
      // ownership of buf to socket.
      base::Unretained(this), base::WrapRefCounted(buf), std::move(callback));
  int rv;

  if (use_tcp_fastopen_ && !tcp_fastopen_write_attempted_) {
    rv = TcpFastOpenWrite(buf, buf_len, std::move(write_callback));
  } else {
    rv = socket_->Write(buf, buf_len, std::move(write_callback),
                        traffic_annotation);
  }

  if (rv != ERR_IO_PENDING)
    rv = HandleWriteCompleted(buf, rv);
  return rv;
}

int TCPSocketPosix::GetLocalAddress(IPEndPoint* address) const {
  DCHECK(address);

  if (!socket_)
    return ERR_SOCKET_NOT_CONNECTED;

  SockaddrStorage storage;
  int rv = socket_->GetLocalAddress(&storage);
  if (rv != OK)
    return rv;

  if (!address->FromSockAddr(storage.addr, storage.addr_len))
    return ERR_ADDRESS_INVALID;

  return OK;
}

int TCPSocketPosix::GetPeerAddress(IPEndPoint* address) const {
  DCHECK(address);

  if (!IsConnected())
    return ERR_SOCKET_NOT_CONNECTED;

  SockaddrStorage storage;
  int rv = socket_->GetPeerAddress(&storage);
  if (rv != OK)
    return rv;

  if (!address->FromSockAddr(storage.addr, storage.addr_len))
    return ERR_ADDRESS_INVALID;

  return OK;
}

int TCPSocketPosix::SetDefaultOptionsForServer() {
  DCHECK(socket_);
  return AllowAddressReuse();
}

void TCPSocketPosix::SetDefaultOptionsForClient() {
  DCHECK(socket_);

  // This mirrors the behaviour on Windows. See the comment in
  // tcp_socket_win.cc after searching for "NODELAY".
  // If SetTCPNoDelay fails, we don't care.
  SetTCPNoDelay(socket_->socket_fd(), true);

#if !defined(OS_ANDROID) && !defined(OS_IOS) && !defined(OS_FUCHSIA)
  // TCP keep alive wakes up the radio, which is expensive on mobile.
  // It's also not implemented on Fuchsia. Do not enable it there.
  // TODO(crbug.com/758706): Consider enabling keep-alive on Fuchsia.
  //
  // It's useful to prevent TCP middleboxes from timing out
  // connection mappings. Packets for timed out connection mappings at
  // middleboxes will either lead to:
  // a) Middleboxes sending TCP RSTs. It's up to higher layers to check for this
  // and retry. The HTTP network transaction code does this.
  // b) Middleboxes just drop the unrecognized TCP packet. This leads to the TCP
  // stack retransmitting packets per TCP stack retransmission timeouts, which
  // are very high (on the order of seconds). Given the number of
  // retransmissions required before killing the connection, this can lead to
  // tens of seconds or even minutes of delay, depending on OS.
  const int kTCPKeepAliveSeconds = 45;

  SetTCPKeepAlive(socket_->socket_fd(), true, kTCPKeepAliveSeconds);
#endif
}

int TCPSocketPosix::AllowAddressReuse() {
  DCHECK(socket_);

  return SetReuseAddr(socket_->socket_fd(), true);
}

int TCPSocketPosix::SetReceiveBufferSize(int32_t size) {
  DCHECK(socket_);

  return SetSocketReceiveBufferSize(socket_->socket_fd(), size);
}

int TCPSocketPosix::SetSendBufferSize(int32_t size) {
  DCHECK(socket_);

  return SetSocketSendBufferSize(socket_->socket_fd(), size);
}

bool TCPSocketPosix::SetKeepAlive(bool enable, int delay) {
  DCHECK(socket_);

  return SetTCPKeepAlive(socket_->socket_fd(), enable, delay);
}

bool TCPSocketPosix::SetNoDelay(bool no_delay) {
  DCHECK(socket_);

  return SetTCPNoDelay(socket_->socket_fd(), no_delay) == OK;
}

void TCPSocketPosix::Close() {
  socket_.reset();

  // Record and reset TCP FastOpen state.
  if (tcp_fastopen_write_attempted_ ||
      tcp_fastopen_status_ == TCP_FASTOPEN_PREVIOUSLY_FAILED) {
    UMA_HISTOGRAM_ENUMERATION("Net.TcpFastOpenSocketConnection",
                              tcp_fastopen_status_, TCP_FASTOPEN_MAX_VALUE);
  }
  use_tcp_fastopen_ = false;
  tcp_fastopen_connected_ = false;
  tcp_fastopen_write_attempted_ = false;
  tcp_fastopen_status_ = TCP_FASTOPEN_STATUS_UNKNOWN;
  tag_ = SocketTag();
}

void TCPSocketPosix::EnableTCPFastOpenIfSupported() {
  if (!IsTCPFastOpenSupported())
    return;

  // Do not enable TCP FastOpen if it had previously failed.
  // This check conservatively avoids middleboxes that may blackhole
  // TCP FastOpen SYN+Data packets; on such a failure, subsequent sockets
  // should not use TCP FastOpen.
  if (!g_tcp_fastopen_has_failed)
    use_tcp_fastopen_ = true;
  else
    tcp_fastopen_status_ = TCP_FASTOPEN_PREVIOUSLY_FAILED;
}

bool TCPSocketPosix::IsValid() const {
  return socket_ != NULL && socket_->socket_fd() != kInvalidSocket;
}

void TCPSocketPosix::DetachFromThread() {
  socket_->DetachFromThread();
}

void TCPSocketPosix::StartLoggingMultipleConnectAttempts(
    const AddressList& addresses) {
  if (!logging_multiple_connect_attempts_) {
    logging_multiple_connect_attempts_ = true;
    LogConnectBegin(addresses);
  } else {
    NOTREACHED();
  }
}

void TCPSocketPosix::EndLoggingMultipleConnectAttempts(int net_error) {
  if (logging_multiple_connect_attempts_) {
    LogConnectEnd(net_error);
    logging_multiple_connect_attempts_ = false;
  } else {
    NOTREACHED();
  }
}

SocketDescriptor TCPSocketPosix::ReleaseSocketDescriptorForTesting() {
  SocketDescriptor socket_descriptor = socket_->ReleaseConnectedSocket();
  socket_.reset();
  return socket_descriptor;
}

SocketDescriptor TCPSocketPosix::SocketDescriptorForTesting() const {
  return socket_->socket_fd();
}

void TCPSocketPosix::ApplySocketTag(const SocketTag& tag) {
  if (IsValid() && tag != tag_) {
    tag.Apply(socket_->socket_fd());
  }
  tag_ = tag;
}

void TCPSocketPosix::AcceptCompleted(
    std::unique_ptr<TCPSocketPosix>* tcp_socket,
    IPEndPoint* address,
    CompletionOnceCallback callback,
    int rv) {
  DCHECK_NE(ERR_IO_PENDING, rv);
  std::move(callback).Run(HandleAcceptCompleted(tcp_socket, address, rv));
}

int TCPSocketPosix::HandleAcceptCompleted(
    std::unique_ptr<TCPSocketPosix>* tcp_socket,
    IPEndPoint* address,
    int rv) {
  if (rv == OK)
    rv = BuildTcpSocketPosix(tcp_socket, address);

  if (rv == OK) {
    net_log_.EndEvent(NetLogEventType::TCP_ACCEPT,
                      CreateNetLogIPEndPointCallback(address));
  } else {
    net_log_.EndEventWithNetErrorCode(NetLogEventType::TCP_ACCEPT, rv);
  }

  return rv;
}

int TCPSocketPosix::BuildTcpSocketPosix(
    std::unique_ptr<TCPSocketPosix>* tcp_socket,
    IPEndPoint* address) {
  DCHECK(accept_socket_);

  SockaddrStorage storage;
  if (accept_socket_->GetPeerAddress(&storage) != OK ||
      !address->FromSockAddr(storage.addr, storage.addr_len)) {
    accept_socket_.reset();
    return ERR_ADDRESS_INVALID;
  }

  tcp_socket->reset(
      new TCPSocketPosix(nullptr, net_log_.net_log(), net_log_.source()));
  (*tcp_socket)->socket_ = std::move(accept_socket_);
  return OK;
}

void TCPSocketPosix::ConnectCompleted(CompletionOnceCallback callback, int rv) {
  DCHECK_NE(ERR_IO_PENDING, rv);
  std::move(callback).Run(HandleConnectCompleted(rv));
}

int TCPSocketPosix::HandleConnectCompleted(int rv) {
  // Log the end of this attempt (and any OS error it threw).
  if (rv != OK) {
    net_log_.EndEvent(NetLogEventType::TCP_CONNECT_ATTEMPT,
                      NetLog::IntCallback("os_error", errno));
    tag_ = SocketTag();
  } else {
    net_log_.EndEvent(NetLogEventType::TCP_CONNECT_ATTEMPT);
    NotifySocketPerformanceWatcher();
  }

  // Give a more specific error when the user is offline.
  if (rv == ERR_ADDRESS_UNREACHABLE && NetworkChangeNotifier::IsOffline())
    rv = ERR_INTERNET_DISCONNECTED;

  if (!logging_multiple_connect_attempts_)
    LogConnectEnd(rv);

  return rv;
}

void TCPSocketPosix::LogConnectBegin(const AddressList& addresses) const {
  net_log_.BeginEvent(NetLogEventType::TCP_CONNECT,
                      addresses.CreateNetLogCallback());
}

void TCPSocketPosix::LogConnectEnd(int net_error) const {
  if (net_error != OK) {
    net_log_.EndEventWithNetErrorCode(NetLogEventType::TCP_CONNECT, net_error);
    return;
  }

  SockaddrStorage storage;
  int rv = socket_->GetLocalAddress(&storage);
  if (rv != OK) {
    PLOG(ERROR) << "GetLocalAddress() [rv: " << rv << "] error: ";
    NOTREACHED();
    net_log_.EndEventWithNetErrorCode(NetLogEventType::TCP_CONNECT, rv);
    return;
  }

  net_log_.EndEvent(
      NetLogEventType::TCP_CONNECT,
      CreateNetLogSourceAddressCallback(storage.addr, storage.addr_len));
}

void TCPSocketPosix::ReadCompleted(const scoped_refptr<IOBuffer>& buf,
                                   CompletionOnceCallback callback,
                                   int rv) {
  DCHECK_NE(ERR_IO_PENDING, rv);

  std::move(callback).Run(HandleReadCompleted(buf.get(), rv));
}

void TCPSocketPosix::ReadIfReadyCompleted(CompletionOnceCallback callback,
                                          int rv) {
  DCHECK_NE(ERR_IO_PENDING, rv);
  DCHECK_GE(OK, rv);

  HandleReadCompletedHelper(rv);
  std::move(callback).Run(rv);
}

int TCPSocketPosix::HandleReadCompleted(IOBuffer* buf, int rv) {
  HandleReadCompletedHelper(rv);

  if (rv < 0)
    return rv;

  // Notify the watcher only if at least 1 byte was read.
  if (rv > 0)
    NotifySocketPerformanceWatcher();

  net_log_.AddByteTransferEvent(NetLogEventType::SOCKET_BYTES_RECEIVED, rv,
                                buf->data());
  NetworkActivityMonitor::GetInstance()->IncrementBytesReceived(rv);

  return rv;
}

void TCPSocketPosix::HandleReadCompletedHelper(int rv) {
  if (tcp_fastopen_write_attempted_ && !tcp_fastopen_connected_) {
    // A TCP FastOpen connect-with-write was attempted. This read was a
    // subsequent read, which either succeeded or failed. If the read
    // succeeded, the socket is considered connected via TCP FastOpen.
    // If the read failed, TCP FastOpen is (conservatively) turned off for all
    // subsequent connections. TCP FastOpen status is recorded in both cases.
    // TODO (jri): This currently results in conservative behavior, where TCP
    // FastOpen is turned off on _any_ error. Implement optimizations,
    // such as turning off TCP FastOpen on more specific errors, and
    // re-attempting TCP FastOpen after a certain amount of time has passed.
    if (rv >= 0)
      tcp_fastopen_connected_ = true;
    else
      g_tcp_fastopen_has_failed = true;
    UpdateTCPFastOpenStatusAfterRead();
  }

  if (rv < 0) {
    net_log_.AddEvent(NetLogEventType::SOCKET_READ_ERROR,
                      CreateNetLogSocketErrorCallback(rv, errno));
  }
}

void TCPSocketPosix::WriteCompleted(const scoped_refptr<IOBuffer>& buf,
                                    CompletionOnceCallback callback,
                                    int rv) {
  DCHECK_NE(ERR_IO_PENDING, rv);
  std::move(callback).Run(HandleWriteCompleted(buf.get(), rv));
}

int TCPSocketPosix::HandleWriteCompleted(IOBuffer* buf, int rv) {
  if (rv < 0) {
    if (tcp_fastopen_write_attempted_ && !tcp_fastopen_connected_) {
      // TCP FastOpen connect-with-write was attempted, and the write failed
      // for unknown reasons. Record status and (conservatively) turn off
      // TCP FastOpen for all subsequent connections.
      // TODO (jri): This currently results in conservative behavior, where TCP
      // FastOpen is turned off on _any_ error. Implement optimizations,
      // such as turning off TCP FastOpen on more specific errors, and
      // re-attempting TCP FastOpen after a certain amount of time has passed.
      tcp_fastopen_status_ = TCP_FASTOPEN_ERROR;
      g_tcp_fastopen_has_failed = true;
    }
    net_log_.AddEvent(NetLogEventType::SOCKET_WRITE_ERROR,
                      CreateNetLogSocketErrorCallback(rv, errno));
    return rv;
  }

  // Notify the watcher only if at least 1 byte was written.
  if (rv > 0)
    NotifySocketPerformanceWatcher();

  net_log_.AddByteTransferEvent(NetLogEventType::SOCKET_BYTES_SENT, rv,
                                buf->data());
  NetworkActivityMonitor::GetInstance()->IncrementBytesSent(rv);
  return rv;
}

int TCPSocketPosix::TcpFastOpenWrite(IOBuffer* buf,
                                     int buf_len,
                                     CompletionOnceCallback callback) {
  SockaddrStorage storage;
  int rv = socket_->GetPeerAddress(&storage);
  if (rv != OK)
    return rv;

  int flags = 0x20000000;  // Magic flag to enable TCP_FASTOPEN.
#if defined(OS_LINUX) || defined(OS_ANDROID)
  // sendto() will fail with EPIPE when the system doesn't implement TCP
  // FastOpen, and with EOPNOTSUPP when the system implements TCP FastOpen
  // but it is disabled. Theoretically these shouldn't happen
  // since the caller should check for system support on startup, but
  // users may dynamically disable TCP FastOpen via sysctl.
  flags |= MSG_NOSIGNAL;
#endif // defined(OS_LINUX) || defined(OS_ANDROID)
  rv = HANDLE_EINTR(sendto(socket_->socket_fd(),
                           buf->data(),
                           buf_len,
                           flags,
                           storage.addr,
                           storage.addr_len));
  tcp_fastopen_write_attempted_ = true;

  if (rv >= 0) {
    tcp_fastopen_status_ = TCP_FASTOPEN_FAST_CONNECT_RETURN;
    return rv;
  }

  DCHECK_NE(EPIPE, errno);

  // If errno == EINPROGRESS, that means the kernel didn't have a cookie
  // and would block. The kernel is internally doing a connect() though.
  // Remap EINPROGRESS to EAGAIN so we treat this the same as our other
  // asynchronous cases. Note that the user buffer has not been copied to
  // kernel space.
  if (errno == EINPROGRESS) {
    rv = ERR_IO_PENDING;
  } else {
    rv = MapSystemError(errno);
  }

  if (rv != ERR_IO_PENDING) {
    // TCP FastOpen connect-with-write was attempted, and the write failed
    // since TCP FastOpen was not implemented or disabled in the OS.
    // Record status and turn off TCP FastOpen for all subsequent connections.
    // TODO (jri): This is almost certainly too conservative, since it blanket
    // turns off TCP FastOpen on any write error. Two things need to be done
    // here: (i) record a histogram of write errors; in particular, record
    // occurrences of EOPNOTSUPP and EPIPE, and (ii) afterwards, consider
    // turning off TCP FastOpen on more specific errors.
    tcp_fastopen_status_ = TCP_FASTOPEN_ERROR;
    g_tcp_fastopen_has_failed = true;
    return rv;
  }

  tcp_fastopen_status_ = TCP_FASTOPEN_SLOW_CONNECT_RETURN;
  return socket_->WaitForWrite(buf, buf_len, std::move(callback));
}

void TCPSocketPosix::NotifySocketPerformanceWatcher() {
#if defined(HAVE_TCP_INFO)
  // Check if |socket_performance_watcher_| is interested in receiving a RTT
  // update notification.
  if (!socket_performance_watcher_ ||
      !socket_performance_watcher_->ShouldNotifyUpdatedRTT()) {
    return;
  }

  base::TimeDelta rtt = GetTransportRtt(socket_->socket_fd());
  if (rtt.is_zero())
    return;

  socket_performance_watcher_->OnUpdatedRTTAvailable(rtt);
#endif  // defined(TCP_INFO)
}

void TCPSocketPosix::UpdateTCPFastOpenStatusAfterRead() {
  DCHECK(tcp_fastopen_status_ == TCP_FASTOPEN_FAST_CONNECT_RETURN ||
         tcp_fastopen_status_ == TCP_FASTOPEN_SLOW_CONNECT_RETURN);

  if (tcp_fastopen_write_attempted_ && !tcp_fastopen_connected_) {
    // TCP FastOpen connect-with-write was attempted, and failed.
    tcp_fastopen_status_ =
        (tcp_fastopen_status_ == TCP_FASTOPEN_FAST_CONNECT_RETURN ?
            TCP_FASTOPEN_FAST_CONNECT_READ_FAILED :
            TCP_FASTOPEN_SLOW_CONNECT_READ_FAILED);
    return;
  }

  bool getsockopt_success = false;
  bool server_acked_syn_data = false;
#if defined(HAVE_TCP_INFO)
  // Probe to see the if the socket used TCP FastOpen.
  getsockopt_success =
      GetServerAckedDataInSyn(socket_->socket_fd(), &server_acked_syn_data);
#endif  // defined(TCP_INFO)

  if (getsockopt_success) {
    if (tcp_fastopen_status_ == TCP_FASTOPEN_FAST_CONNECT_RETURN) {
      tcp_fastopen_status_ =
          (server_acked_syn_data ? TCP_FASTOPEN_SYN_DATA_ACK
                                 : TCP_FASTOPEN_SYN_DATA_NACK);
    } else {
      tcp_fastopen_status_ =
          (server_acked_syn_data ? TCP_FASTOPEN_NO_SYN_DATA_ACK
                                 : TCP_FASTOPEN_NO_SYN_DATA_NACK);
    }
  } else {
    tcp_fastopen_status_ =
        (tcp_fastopen_status_ == TCP_FASTOPEN_FAST_CONNECT_RETURN ?
         TCP_FASTOPEN_SYN_DATA_GETSOCKOPT_FAILED :
         TCP_FASTOPEN_NO_SYN_DATA_GETSOCKOPT_FAILED);
  }
}

bool TCPSocketPosix::GetEstimatedRoundTripTime(base::TimeDelta* out_rtt) const {
  DCHECK(out_rtt);
  if (!socket_)
    return false;

#if defined(HAVE_TCP_INFO)
  base::TimeDelta rtt = GetTransportRtt(socket_->socket_fd());
  if (rtt.is_zero())
    return false;
  *out_rtt = rtt;
  return true;
#endif  // defined(TCP_INFO)
  return false;
}

}  // namespace net
