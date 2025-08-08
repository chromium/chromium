// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/socket_posix.h"

#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/socket.h>

#include <memory>
#include <utility>

#include "base/debug/alias.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/task/current_thread.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/sockaddr_storage.h"
#include "net/base/trace_constants.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

// TODO(crbug.com/40064248): Remove this once sufficient information is
// collected.
#include "base/debug/crash_logging.h"

#if BUILDFLAG(IS_FUCHSIA)
#include <poll.h>
#include <sys/ioctl.h>
#endif  // BUILDFLAG(IS_FUCHSIA)

#if defined(DEBUG_CRBUG_40064248_STATISTICS)

// TODO(crbug.com/40064248): Remove this once the crash is resolved.

#if BUILDFLAG(IS_APPLE)
#include <sys/syscall.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <unistd.h>
#endif  // BUILDFLAG(IS_APPLE)

#include "base/no_destructor.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"

#endif  // DEBUG_CRBUG_40064248_STATISTICS

namespace net {

namespace {

#if defined(DEBUG_CRBUG_40064248_STATISTICS)

// TODO(crbug.com/40064248): Remove this once the crash is resolved.

#if BUILDFLAG(IS_APPLE)

timespec TimespecSubtract(const timespec& minuend, const timespec& subtrahend) {
  constexpr int kNanosecondsPerSecond = 1e9;

  timespec difference{
      .tv_sec = minuend.tv_sec - subtrahend.tv_sec,
      .tv_nsec = minuend.tv_nsec - subtrahend.tv_nsec,
  };
  if (difference.tv_nsec < 0) {
    --difference.tv_sec;
    difference.tv_nsec += kNanosecondsPerSecond;
  }
  return difference;
}

// Returns the start time of the process, on the CLOCK_MONOTONIC_RAW time base.
timespec ProcessStartTimeMonotonic() {
  timespec now_real;
  if (clock_gettime(CLOCK_REALTIME, &now_real) != 0) {
    return {.tv_sec = -1, .tv_nsec = -1 - errno};
  }

  timespec now_monotonic;
  if (clock_gettime(CLOCK_MONOTONIC_RAW, &now_monotonic) != 0) {
    return {.tv_sec = -2, .tv_nsec = -1 - errno};
  }

  timespec monotonic_real_offset(TimespecSubtract(now_real, now_monotonic));
  if (monotonic_real_offset.tv_sec < 0) {
    return {.tv_sec = -3, .tv_nsec = -1};
  }

  kinfo_proc kern_proc_info;
  int mib[] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid()};
  size_t len = sizeof(kern_proc_info);
  if (sysctl(mib, std::size(mib), &kern_proc_info, &len, nullptr, 0) != 0) {
    return {.tv_sec = -4, .tv_nsec = -1 - errno};
  }

  timespec start_real;
  TIMEVAL_TO_TIMESPEC(&kern_proc_info.kp_proc.p_starttime, &start_real);

  return TimespecSubtract(start_real, monotonic_real_offset);
}

#endif  // BUILDFLAG(IS_APPLE)

// Returns a string representation of `time`, taken relative to `start_time`.
// The times must be on the same timebase, such as CLOCK_MONOTONIC_RAW.
std::string ProcessTimeString(const timespec& time,
                              const timespec& start_time) {
  if (time.tv_sec == -1 && time.tv_nsec < 0) {
    return base::StringPrintf("(time_errno=%d)", -1 - time.tv_nsec);
  }

  std::string s;
  timespec print_time;
  if (start_time.tv_sec <= -1 && start_time.tv_sec >= -4 &&
      start_time.tv_nsec < 0) {
    s.assign(base::StringPrintf("(start_time_errno=%d,%d)", -start_time.tv_sec,
                                -1 - start_time.tv_nsec));
    print_time = time;
  } else {
    print_time = TimespecSubtract(time, start_time);
  }

  base::StringAppendF(&s, "%d.%09d", print_time.tv_sec, print_time.tv_nsec);

  return s;
}

#endif  // DEBUG_CRBUG_40064248_STATISTICS

int MapAcceptError(int os_error) {
  switch (os_error) {
    // If the client aborts the connection before the server calls accept,
    // POSIX specifies accept should fail with ECONNABORTED. The server can
    // ignore the error and just call accept again, so we map the error to
    // ERR_IO_PENDING. See UNIX Network Programming, Vol. 1, 3rd Ed., Sec.
    // 5.11, "Connection Abort before accept Returns".
    case ECONNABORTED:
      return ERR_IO_PENDING;
    default:
      return MapSystemError(os_error);
  }
}

int MapConnectError(int os_error) {
  switch (os_error) {
    case EINPROGRESS:
      return ERR_IO_PENDING;
    case EACCES:
      return ERR_NETWORK_ACCESS_DENIED;
    case ETIMEDOUT:
      return ERR_CONNECTION_TIMED_OUT;
    default: {
      int net_error = MapSystemError(os_error);
      if (net_error == ERR_FAILED)
        return ERR_CONNECTION_FAILED;  // More specific than ERR_FAILED.
      return net_error;
    }
  }
}

}  // namespace

SocketPosix::SocketPosix()
    : socket_fd_(kInvalidSocket),
      accept_socket_watcher_(FROM_HERE),
      read_socket_watcher_(FROM_HERE),
      write_socket_watcher_(FROM_HERE) {}

SocketPosix::~SocketPosix() {
  Close();
}

int SocketPosix::Open(int address_family) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(kInvalidSocket, socket_fd_);
  DCHECK(address_family == AF_INET ||
         address_family == AF_INET6 ||
         address_family == AF_UNIX);

  socket_fd_ = CreatePlatformSocket(
      address_family,
      SOCK_STREAM,
      address_family == AF_UNIX ? 0 : IPPROTO_TCP);
  if (socket_fd_ < 0) {
    PLOG(ERROR) << "CreatePlatformSocket() failed";
    return MapSystemError(errno);
  }

  if (!base::SetNonBlocking(socket_fd_)) {
    int rv = MapSystemError(errno);
    Close();
    return rv;
  }

  return OK;
}

int SocketPosix::AdoptConnectedSocket(SocketDescriptor socket,
                                      const SockaddrStorage& address) {
  int rv = AdoptUnconnectedSocket(socket);
  if (rv != OK)
    return rv;

  SetPeerAddress(address);
  return OK;
}

int SocketPosix::AdoptUnconnectedSocket(SocketDescriptor socket) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(kInvalidSocket, socket_fd_);

  socket_fd_ = socket;

  if (!base::SetNonBlocking(socket_fd_)) {
    int rv = MapSystemError(errno);
    Close();
    return rv;
  }

  return OK;
}

SocketDescriptor SocketPosix::ReleaseConnectedSocket() {
  // It's not safe to release a socket with a pending write.
  DCHECK(!write_buf_);

  StopWatchingAndCleanUp(false /* close_socket */);
  SocketDescriptor socket_fd = socket_fd_;
  socket_fd_ = kInvalidSocket;
  return socket_fd;
}

int SocketPosix::Bind(const SockaddrStorage& address) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_NE(kInvalidSocket, socket_fd_);

  int rv = bind(socket_fd_, address.addr(), address.addr_len);
  if (rv < 0) {
    PLOG(ERROR) << "bind() failed";
    return MapSystemError(errno);
  }

  return OK;
}

int SocketPosix::Listen(int backlog) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_NE(kInvalidSocket, socket_fd_);
  DCHECK_LT(0, backlog);

  int rv = listen(socket_fd_, backlog);
  if (rv < 0) {
    PLOG(ERROR) << "listen() failed";
    return MapSystemError(errno);
  }

  return OK;
}

int SocketPosix::Accept(std::unique_ptr<SocketPosix>* socket,
                        CompletionOnceCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_NE(kInvalidSocket, socket_fd_);
  DCHECK(accept_callback_.is_null());
  DCHECK(socket);
  DCHECK(!callback.is_null());

  int rv = DoAccept(socket);
  if (rv != ERR_IO_PENDING)
    return rv;

  if (!base::CurrentIOThread::Get()->WatchFileDescriptor(
          socket_fd_, true, base::MessagePumpForIO::WATCH_READ,
          &accept_socket_watcher_, this)) {
    PLOG(ERROR) << "WatchFileDescriptor failed on accept";
    return MapSystemError(errno);
  }

  accept_socket_ = socket;
  accept_callback_ = std::move(callback);
  return ERR_IO_PENDING;
}

int SocketPosix::Connect(const SockaddrStorage& address,
                         CompletionOnceCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_NE(kInvalidSocket, socket_fd_);
  DCHECK(!waiting_connect_);
  DCHECK(!callback.is_null());

  SetPeerAddress(address);

  int rv = DoConnect();
  if (rv != ERR_IO_PENDING)
    return rv;

  if (!base::CurrentIOThread::Get()->WatchFileDescriptor(
          socket_fd_, true, base::MessagePumpForIO::WATCH_WRITE,
          &write_socket_watcher_, this)) {
    PLOG(ERROR) << "WatchFileDescriptor failed on connect";
    return MapSystemError(errno);
  }

  // There is a race-condition in the above code if the kernel receive a RST
  // packet for the "connect" call before the registration of the socket file
  // descriptor to the message loop pump. On most platform it is benign as the
  // message loop pump is awakened for that socket in an error state, but on
  // iOS this does not happens. Check the status of the socket at this point
  // and if in error, consider the connection as failed.
  int os_error = 0;
  socklen_t len = sizeof(os_error);
  if (getsockopt(socket_fd_, SOL_SOCKET, SO_ERROR, &os_error, &len) == 0) {
    // TCPSocketPosix expects errno to be set.
    errno = os_error;
  }

  rv = MapConnectError(errno);
  if (rv != OK && rv != ERR_IO_PENDING) {
    write_socket_watcher_.StopWatchingFileDescriptor();
    return rv;
  }

  write_callback_ = std::move(callback);
  waiting_connect_ = true;
  return ERR_IO_PENDING;
}

bool SocketPosix::IsConnected() const {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (socket_fd_ == kInvalidSocket || waiting_connect_)
    return false;

  // Checks if connection is alive.
  char c;
  int rv = HANDLE_EINTR(recv(socket_fd_, &c, 1, MSG_PEEK));
  if (rv == 0)
    return false;
  if (rv == -1 && errno != EAGAIN && errno != EWOULDBLOCK)
    return false;

  return true;
}

bool SocketPosix::IsConnectedAndIdle() const {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (socket_fd_ == kInvalidSocket || waiting_connect_)
    return false;

  // Check if connection is alive and we haven't received any data
  // unexpectedly.
  char c;
  int rv = HANDLE_EINTR(recv(socket_fd_, &c, 1, MSG_PEEK));
  if (rv >= 0)
    return false;
  if (errno != EAGAIN && errno != EWOULDBLOCK)
    return false;

  return true;
}

int SocketPosix::Read(IOBuffer* buf,
                      int buf_len,
                      CompletionOnceCallback callback) {
  // Use base::Unretained() is safe here because OnFileCanReadWithoutBlocking()
  // won't be called if |this| is gone.
  int rv = ReadIfReady(
      buf, buf_len,
      base::BindOnce(&SocketPosix::RetryRead, base::Unretained(this)));
  if (rv == ERR_IO_PENDING) {
    read_buf_ = buf;
    read_buf_len_ = buf_len;
    read_callback_ = std::move(callback);
  }
  return rv;
}

int SocketPosix::ReadIfReady(IOBuffer* buf,
                             int buf_len,
                             CompletionOnceCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_NE(kInvalidSocket, socket_fd_);
  DCHECK(!waiting_connect_);
  CHECK(read_if_ready_callback_.is_null());
  DCHECK(!callback.is_null());
  DCHECK_LT(0, buf_len);

  int rv = DoRead(buf, buf_len);
  if (rv != ERR_IO_PENDING)
    return rv;

  if (!base::CurrentIOThread::Get()->WatchFileDescriptor(
          socket_fd_, true, base::MessagePumpForIO::WATCH_READ,
          &read_socket_watcher_, this)) {
    PLOG(ERROR) << "WatchFileDescriptor failed on read";
    return MapSystemError(errno);
  }

  read_if_ready_callback_ = std::move(callback);
  return ERR_IO_PENDING;
}

int SocketPosix::CancelReadIfReady() {
  DCHECK(read_if_ready_callback_);

  bool ok = read_socket_watcher_.StopWatchingFileDescriptor();
  DCHECK(ok);

  read_if_ready_callback_.Reset();
  return net::OK;
}

int SocketPosix::Write(
    IOBuffer* buf,
    int buf_len,
    CompletionOnceCallback callback,
    const NetworkTrafficAnnotationTag& /* traffic_annotation */) {
  DCHECK(thread_checker_.CalledOnValidThread());
  CHECK_NE(kInvalidSocket, socket_fd_);
  CHECK(!waiting_connect_);
  CHECK(write_callback_.is_null());
  // Synchronous operation not supported
  CHECK(!callback.is_null());
  CHECK_LT(0, buf_len);

  int rv = DoWrite(buf, buf_len);
  if (rv == ERR_IO_PENDING)
    rv = WaitForWrite(buf, buf_len, std::move(callback));
  return rv;
}

int SocketPosix::WaitForWrite(IOBuffer* buf,
                              int buf_len,
                              CompletionOnceCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_NE(kInvalidSocket, socket_fd_);
  DCHECK(write_callback_.is_null());
  // Synchronous operation not supported
  DCHECK(!callback.is_null());
  DCHECK_LT(0, buf_len);

  if (!base::CurrentIOThread::Get()->WatchFileDescriptor(
          socket_fd_, true, base::MessagePumpForIO::WATCH_WRITE,
          &write_socket_watcher_, this)) {
    PLOG(ERROR) << "WatchFileDescriptor failed on write";
    return MapSystemError(errno);
  }

  write_buf_ = buf;
  write_buf_len_ = buf_len;
  write_callback_ = std::move(callback);
  return ERR_IO_PENDING;
}

int SocketPosix::GetLocalAddress(SockaddrStorage* address) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(address);

  if (getsockname(socket_fd_, address->addr(), &address->addr_len) < 0) {
    return MapSystemError(errno);
  }
  return OK;
}

int SocketPosix::GetPeerAddress(SockaddrStorage* address) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(address);

  if (!HasPeerAddress())
    return ERR_SOCKET_NOT_CONNECTED;

  *address = *peer_address_;
  return OK;
}

void SocketPosix::SetPeerAddress(const SockaddrStorage& address) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // |peer_address_| will be non-nullptr if Connect() has been called. Unless
  // Close() is called to reset the internal state, a second call to Connect()
  // is not allowed.
  // Please note that we don't allow a second Connect() even if the previous
  // Connect() has failed. Connecting the same |socket_| again after a
  // connection attempt failed results in unspecified behavior according to
  // POSIX.
  DCHECK(!peer_address_);
  peer_address_ = std::make_unique<SockaddrStorage>(address);
}

bool SocketPosix::HasPeerAddress() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return peer_address_ != nullptr;
}

void SocketPosix::Close() {
  DCHECK(thread_checker_.CalledOnValidThread());

  StopWatchingAndCleanUp(true /* close_socket */);
}

void SocketPosix::DetachFromThread() {
  thread_checker_.DetachFromThread();
}

void SocketPosix::OnFileCanReadWithoutBlocking(int fd) {
  TRACE_EVENT0(NetTracingCategory(),
               "SocketPosix::OnFileCanReadWithoutBlocking");
  if (!accept_callback_.is_null()) {
    AcceptCompleted();
  } else {
    DCHECK(!read_if_ready_callback_.is_null());
    ReadCompleted();
  }
}

void SocketPosix::OnFileCanWriteWithoutBlocking(int fd) {
  DCHECK(!write_callback_.is_null());
  if (waiting_connect_) {
    ConnectCompleted();
  } else {
    WriteCompleted();
  }
}

int SocketPosix::DoAccept(std::unique_ptr<SocketPosix>* socket) {
  SockaddrStorage new_peer_address;
  int new_socket = HANDLE_EINTR(
      accept(socket_fd_, new_peer_address.addr(), &new_peer_address.addr_len));
  if (new_socket < 0)
    return MapAcceptError(errno);

  auto accepted_socket = std::make_unique<SocketPosix>();
  int rv = accepted_socket->AdoptConnectedSocket(new_socket, new_peer_address);
  if (rv != OK)
    return rv;

  *socket = std::move(accepted_socket);
  return OK;
}

void SocketPosix::AcceptCompleted() {
  DCHECK(accept_socket_);
  int rv = DoAccept(accept_socket_);
  if (rv == ERR_IO_PENDING)
    return;

  bool ok = accept_socket_watcher_.StopWatchingFileDescriptor();
  DCHECK(ok);
  accept_socket_ = nullptr;
  std::move(accept_callback_).Run(rv);
}

int SocketPosix::DoConnect() {
  int rv = HANDLE_EINTR(
      connect(socket_fd_, peer_address_->addr(), peer_address_->addr_len));
  DCHECK_GE(0, rv);
  return rv == 0 ? OK : MapConnectError(errno);
}

void SocketPosix::ConnectCompleted() {
  // Get the error that connect() completed with.
  int os_error = 0;
  socklen_t len = sizeof(os_error);
  if (getsockopt(socket_fd_, SOL_SOCKET, SO_ERROR, &os_error, &len) == 0) {
    // TCPSocketPosix expects errno to be set.
    errno = os_error;
  }

  int rv = MapConnectError(errno);
  if (rv == ERR_IO_PENDING)
    return;

  bool ok = write_socket_watcher_.StopWatchingFileDescriptor();
  DCHECK(ok);
  waiting_connect_ = false;
  std::move(write_callback_).Run(rv);
}

int SocketPosix::DoRead(IOBuffer* buf, int buf_len) {
  int rv = HANDLE_EINTR(read(socket_fd_, buf->data(), buf_len));
  return rv >= 0 ? rv : MapSystemError(errno);
}

void SocketPosix::RetryRead(int rv) {
  DCHECK(read_callback_);
  DCHECK(read_buf_);
  DCHECK_LT(0, read_buf_len_);

  if (rv == OK) {
    rv = ReadIfReady(
        read_buf_.get(), read_buf_len_,
        base::BindOnce(&SocketPosix::RetryRead, base::Unretained(this)));
    if (rv == ERR_IO_PENDING)
      return;
  }
  read_buf_ = nullptr;
  read_buf_len_ = 0;
  std::move(read_callback_).Run(rv);
}

void SocketPosix::ReadCompleted() {
  DCHECK(read_if_ready_callback_);

  bool ok = read_socket_watcher_.StopWatchingFileDescriptor();
  DCHECK(ok);
  std::move(read_if_ready_callback_).Run(OK);
}

int SocketPosix::DoWrite(IOBuffer* buf, int buf_len) {
  const char* data = buf->data();
  const int flags = MSG_NOSIGNAL;

  // TODO(crbug.com/40064248): Remove this once the crash is resolved.
  char debug3[128];
  snprintf(debug3, sizeof(debug3),
           "socket_fd_=%d,data=%p,buf_len=%d,flags=0x%x", socket_fd_, data,
           buf_len, flags);
  base::debug::Alias(debug3);

  ssize_t send_rv = HANDLE_EINTR(send(socket_fd_, buf->data(), buf_len, flags));
  int send_errno = errno;

#if defined(DEBUG_CRBUG_40064248_STATISTICS)
  // TODO(crbug.com/40064248): Remove this once the crash is resolved.

  // Do this once, as close to process start as is reasonably practical,
  // because the OS records the start time on the CLOCK_REALTIME timebase,
  // which can only drift (or even move backwards) relative to the
  // CLOCK_MONOTONIC_RAW timebase as time elapses.
  static timespec start_time(ProcessStartTimeMonotonic());

  timespec now;
  if (clock_gettime(CLOCK_MONOTONIC_RAW, &now) != 0) {
    now.tv_sec = -1;
    now.tv_nsec = -errno;
  }

  static Statistics g_statistics;

  // A single SocketPosix shouldn’t have DoWrite called on more than one thread,
  // checked by its thread_checker_. Thus, access to its own statistics_ need
  // not be thread-safe. It’s less certain that access to the global
  // g_statistics can afford to not be thread-safe, because it’s conceivable
  // (although unlikely) that different SocketPosix objects operate on different
  // threads. Use a mutex to protect g_statistics and, incidentally, all
  // SocketPosix::statistics_ objects. The mutex is not likely to be contended,
  // and in any case, the critical section executes quickly.
  static base::NoDestructor<base::Lock> statistics_lock;
  base::ReleasableAutoLock statistics_auto_lock(statistics_lock.get());

  statistics_.Update(socket_fd_, send_rv, send_errno, now);
  g_statistics.Update(socket_fd_, send_rv, send_errno, now);
#endif  // DEBUG_CRBUG_40064248_STATISTICS

  if (send_rv >= 0) {
    if (send_rv > buf_len) {
      // TODO(crbug.com/40064248): Remove this once the crash is resolved.
      char debug4[64];
      snprintf(debug4, sizeof(debug4), "send_rv=%zd,send_errno=%d", send_rv,
               send_rv < 0 ? send_errno : 0);
      base::debug::Alias(debug4);

#if defined(DEBUG_CRBUG_40064248_STATISTICS)
      char debug5[4096];
      snprintf(debug5, sizeof(debug5), "statistics_={%s},g_statistics={%s}",
               statistics_.DebugInfo(start_time).c_str(),
               g_statistics.DebugInfo(start_time).c_str());
      base::debug::Alias(debug5);
      statistics_auto_lock.Release();
#endif  // DEBUG_CRBUG_40064248_STATISTICS

      // This duplicates the CHECK_LE below. Keep it here so that the aliased
      // debug buffers are in scope when the process crashes.
      CHECK_LE(send_rv, buf_len);
    }

    CHECK_LE(send_rv, buf_len);
  }

  return send_rv >= 0 ? send_rv : MapSystemError(send_errno);
}

void SocketPosix::WriteCompleted() {
  int rv = DoWrite(write_buf_.get(), write_buf_len_);
  if (rv == ERR_IO_PENDING)
    return;

  bool ok = write_socket_watcher_.StopWatchingFileDescriptor();
  DCHECK(ok);
  write_buf_.reset();
  write_buf_len_ = 0;
  std::move(write_callback_).Run(rv);
}

void SocketPosix::StopWatchingAndCleanUp(bool close_socket) {
  bool ok = accept_socket_watcher_.StopWatchingFileDescriptor();
  DCHECK(ok);
  ok = read_socket_watcher_.StopWatchingFileDescriptor();
  DCHECK(ok);
  ok = write_socket_watcher_.StopWatchingFileDescriptor();
  DCHECK(ok);

  // These needs to be done after the StopWatchingFileDescriptor() calls, but
  // before deleting the write buffer.
  if (close_socket) {
    if (socket_fd_ != kInvalidSocket) {
      if (IGNORE_EINTR(close(socket_fd_)) < 0)
        DPLOG(ERROR) << "close() failed";
      socket_fd_ = kInvalidSocket;
    }
  }

  if (!accept_callback_.is_null()) {
    accept_socket_ = nullptr;
    accept_callback_.Reset();
  }

  if (!read_callback_.is_null()) {
    read_buf_.reset();
    read_buf_len_ = 0;
    read_callback_.Reset();
  }

  read_if_ready_callback_.Reset();

  if (!write_callback_.is_null()) {
    write_buf_.reset();
    write_buf_len_ = 0;
    write_callback_.Reset();
  }

  waiting_connect_ = false;
  peer_address_.reset();
}

#if defined(DEBUG_CRBUG_40064248_STATISTICS)

// TODO(crbug.com/40064248): Remove this once the crash is resolved.

SocketPosix::Statistics::Statistics() = default;

void SocketPosix::Statistics::Update(const int socket_fd,
                                     const ssize_t send_rv,
                                     const int send_errno,
                                     const timespec& now) {
  if (send_rv < 0) {
    if (sends_error_++ == 0) {
      first_send_errno_ = send_errno;
      first_send_error_time_ = now;
    }
    last_send_errno_ = send_errno;
    last_send_error_time_ = now;
    if (sends_error_consecutive_++ == 0) {
      first_send_error_consecutive_time_ = now;
    }

    sends_suspicious_consecutive_ = 0;
    send_bytes_suspicious_consecutive_ = 0;
    first_send_suspicious_consecutive_time_ = {};
  } else {
    // Suspicious return values for `send` are those that are produced when the
    // `sendto` system call returns from the kernel to user space without
    // setting the return value register, leaving whatever contents had
    // previously been there on the call into the kernel. The return value
    // register is x0 on arm64 and rax on x86-64. When the `sendto` system call
    // is made, user space places the first argument, the file descriptor
    // number, in r0 (thus x0) on arm64, and the UNIX system call class (2) and
    // `sendto` system call number (133) in eax (thus rax) on x86_64.
    //
    // A suspicious return isn’t necessarily a problem: it’s possible that only
    // that many bytes were sent (or consumed from the buffer and queued to be
    // sent). But it could also be an indication of https://crbug.com/40064248
    // occurring before it’s unambiguously detectable as a bug.
    //
    // Suspicious returns become unambiguously detectable when they exceed the
    // size of the buffer passed to `send`.
    //
    // This classifies returns into suspicious and not-suspicious, so that the
    // extent of the bug can be understood better.
#if BUILDFLAG(IS_APPLE)
#if defined(ARCH_CPU_ARM64)
    const ssize_t suspicious_rv = socket_fd;
#elif defined(ARCH_CPU_X86_FAMILY)
    // Definitions from xnu osfmk/mach/i386/syscall_sw.h, as used in its
    // SYSCALL_CONSTRUCT_UNIX.
    constexpr uint32_t SYSCALL_CLASS_SHIFT = 24;
    constexpr uint32_t SYSCALL_CLASS_MASK = 0xff << SYSCALL_CLASS_SHIFT;
    constexpr uint32_t SYSCALL_NUMBER_MASK = ~SYSCALL_CLASS_MASK;
    constexpr uint32_t SYSCALL_CLASS_UNIX = 2;

    const ssize_t suspicious_rv =
        ((SYSCALL_CLASS_UNIX << SYSCALL_CLASS_SHIFT) & SYSCALL_CLASS_MASK) |
        (SYS_sendto & SYSCALL_NUMBER_MASK);  // ((2 << 24) | 133) = 0x2000085
#endif
#endif  // BUILDFLAG(IS_APPLE)
    const bool suspicious = send_rv == suspicious_rv;

    if (!suspicious) {
      if (sends_ok_++ == 0) {
        first_send_ok_time_ = now;
      }
      send_bytes_ok_ += send_rv;
      last_send_ok_time_ = now;

      sends_suspicious_consecutive_ = 0;
      send_bytes_suspicious_consecutive_ = 0;
      first_send_suspicious_consecutive_time_ = {};
    } else {
      if (sends_suspicious_++ == 0) {
        first_send_suspicious_time_ = now;
      }
      send_bytes_suspicious_ += send_rv;
      last_send_suspicious_time_ = now;

      if (sends_suspicious_consecutive_++ == 0) {
        first_send_suspicious_consecutive_time_ = now;
      }
      send_bytes_suspicious_consecutive_ += send_rv;
    }

    sends_error_consecutive_ = 0;
    first_send_error_consecutive_time_ = {};
  }
}

std::string SocketPosix::Statistics::DebugInfo(
    const timespec& start_time) const {
  std::string s(base::StringPrintf(
      "sends_ok_=%u,sends_suspicious_=%u,sends_suspicious_consecutive_=%u,"
      "sends_error_=%u,sends_error_consecutive_=%u,send_bytes_ok_=%u,"
      "send_bytes_suspicious_=%u,send_bytes_suspicious_consecutive_=%u",
      sends_ok_, sends_suspicious_, sends_suspicious_consecutive_, sends_error_,
      sends_error_consecutive_, send_bytes_ok_, send_bytes_suspicious_,
      send_bytes_suspicious_consecutive_));

  if (sends_ok_) {
    base::StringAppendF(&s, ",first_send_ok_time_=%s,last_send_ok_time_=%s",
                        ProcessTimeString(first_send_ok_time_, start_time),
                        ProcessTimeString(last_send_ok_time_, start_time));
  }

  if (sends_suspicious_) {
    base::StringAppendF(
        &s, ",first_send_suspicious_time_=%s,last_send_suspicious_time_=%s",
        ProcessTimeString(first_send_suspicious_time_, start_time),
        ProcessTimeString(last_send_suspicious_time_, start_time));

    if (sends_suspicious_consecutive_) {
      base::StringAppendF(
          &s, ",first_send_suspicious_consecutive_time_=%s",
          ProcessTimeString(first_send_suspicious_consecutive_time_,
                            start_time));
    }
  }

  if (sends_error_) {
    base::StringAppendF(
        &s,
        ",first_send_errno_=%d,last_send_errno_=%d,first_send_error_time_=%s,"
        "last_send_error_time_=%s",
        first_send_errno_, last_send_errno_,
        ProcessTimeString(first_send_error_time_, start_time),
        ProcessTimeString(last_send_error_time_, start_time));

    if (sends_error_consecutive_) {
      base::StringAppendF(
          &s, ",first_send_error_consecutive_time_=%s",
          ProcessTimeString(first_send_error_consecutive_time_, start_time));
    }
  }

  return s;
}

#endif  // DEBUG_CRBUG_40064248_STATISTICS

}  // namespace net
