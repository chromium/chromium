// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_SOCKET_POSIX_H_
#define NET_SOCKET_SOCKET_POSIX_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/message_loop/message_pump_for_io.h"
#include "base/threading/thread_checker.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_export.h"
#include "net/socket/socket_descriptor.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

#if BUILDFLAG(IS_MAC)
#define DEBUG_CRBUG_40064248_STATISTICS 1
#include <stdint.h>
#include <sys/types.h>
#include <time.h>

#include <string>
#endif  // BUILDFLAG(IS_MAC)

namespace net {

class IOBuffer;
struct SockaddrStorage;

// Socket class to provide asynchronous read/write operations on top of the
// posix socket api. It supports AF_INET, AF_INET6, and AF_UNIX addresses.
class NET_EXPORT_PRIVATE SocketPosix
    : public base::MessagePumpForIO::FdWatcher {
 public:
  SocketPosix();

  SocketPosix(const SocketPosix&) = delete;
  SocketPosix& operator=(const SocketPosix&) = delete;

  ~SocketPosix() override;

  // Opens a socket and returns net::OK if |address_family| is AF_INET, AF_INET6
  // or AF_UNIX. Otherwise, it does DCHECK() and returns a net error.
  int Open(int address_family);

  // Takes ownership of |socket|, which is known to already be connected to the
  // given peer address.
  int AdoptConnectedSocket(SocketDescriptor socket,
                           const SockaddrStorage& peer_address);
  // Takes ownership of |socket|, which may or may not be open, bound, or
  // listening. The caller must determine the state of the socket based on its
  // provenance and act accordingly. The socket may have connections waiting
  // to be accepted, but must not be actually connected.
  int AdoptUnconnectedSocket(SocketDescriptor socket);

  // Releases ownership of |socket_fd_| to caller. There must be no pending
  // write.
  SocketDescriptor ReleaseConnectedSocket();

  int Bind(const SockaddrStorage& address);

  int Listen(int backlog);
  int Accept(std::unique_ptr<SocketPosix>* socket,
             CompletionOnceCallback callback);

  // Connects socket. On non-ERR_IO_PENDING error, sets errno and returns a net
  // error code. On ERR_IO_PENDING, |callback| is called with a net error code,
  // not errno, though errno is set if connect event happens with error.
  // TODO(byungchul): Need more robust way to pass system errno.
  int Connect(const SockaddrStorage& address, CompletionOnceCallback callback);
  bool IsConnected() const;
  bool IsConnectedAndIdle() const;

  // Multiple outstanding requests of the same type are not supported.
  // Full duplex mode (reading and writing at the same time) is supported.
  // On error which is not ERR_IO_PENDING, sets errno and returns a net error
  // code. On ERR_IO_PENDING, |callback| is called with a net error code, not
  // errno, though errno is set if read or write events happen with error.
  // TODO(byungchul): Need more robust way to pass system errno.
  int Read(IOBuffer* buf, int buf_len, CompletionOnceCallback callback);

  // Reads up to |buf_len| bytes into |buf| without blocking. If read is to
  // be retried later, |callback| will be invoked when data is ready for
  // reading. This method doesn't hold on to |buf|.
  // See socket.h for more information.
  int ReadIfReady(IOBuffer* buf, int buf_len, CompletionOnceCallback callback);
  int CancelReadIfReady();
  int Write(IOBuffer* buf,
            int buf_len,
            CompletionOnceCallback callback,
            const NetworkTrafficAnnotationTag& traffic_annotation);

  // Waits for next write event. This is called by TCPSocketPosix for TCP
  // fastopen after sending first data. Returns ERR_IO_PENDING if it starts
  // waiting for write event successfully. Otherwise, returns a net error code.
  // It must not be called after Write() because Write() calls it internally.
  int WaitForWrite(IOBuffer* buf, int buf_len, CompletionOnceCallback callback);

  int GetLocalAddress(SockaddrStorage* address) const;
  int GetPeerAddress(SockaddrStorage* address) const;
  void SetPeerAddress(const SockaddrStorage& address);
  // Returns true if peer address has been set regardless of socket state.
  bool HasPeerAddress() const;

  void Close();

  // Detachs from the current thread, to allow the socket to be transferred to
  // a new thread. Should only be called when the object is no longer used by
  // the old thread.
  void DetachFromThread();

  SocketDescriptor socket_fd() const { return socket_fd_; }

 private:
#if defined(DEBUG_CRBUG_40064248_STATISTICS)
  // TODO(crbug.com/40064248): Remove this once the crash is resolved.
  class Statistics {
   public:
    Statistics();

    // Update statistics with the result of a send.
    void Update(int socket_fd,
                ssize_t send_rv,
                int send_errno,
                const timespec& now);

    // Return a string form of the collected statistics. Times are represented
    // relative to start_time, if possible.
    std::string DebugInfo(const timespec& start_time) const;

   private:
    // *_errno_ and other *_error_* members track `send` reporting an error by
    // returning -1. They don’t track the occurrence of
    // https://crbug.com/40064248. *_suspicious_* tracks possible occurrences of
    // that bug, and a CHECK failure records a definite occurrence.
    int first_send_errno_ = 0;
    int last_send_errno_ = 0;

    uint64_t sends_ok_ = 0;
    uint64_t sends_suspicious_ = 0;
    uint64_t sends_suspicious_consecutive_ = 0;
    uint64_t sends_error_ = 0;
    uint64_t sends_error_consecutive_ = 0;
    uint64_t send_bytes_ok_ = 0;
    uint64_t send_bytes_suspicious_ = 0;
    uint64_t send_bytes_suspicious_consecutive_ = 0;

    // Times are recorded relative to CLOCK_MONOTONIC_RAW.
    timespec first_send_ok_time_ = {};
    timespec last_send_ok_time_ = {};
    timespec first_send_suspicious_time_ = {};
    timespec last_send_suspicious_time_ = {};
    timespec first_send_suspicious_consecutive_time_ = {};
    timespec first_send_error_time_ = {};
    timespec last_send_error_time_ = {};
    timespec first_send_error_consecutive_time_ = {};
  };
#endif  // DEBUG_CRBUG_40064248_STATISTICS

  // base::MessagePumpForIO::FdWatcher methods.
  void OnFileCanReadWithoutBlocking(int fd) override;
  void OnFileCanWriteWithoutBlocking(int fd) override;

  int DoAccept(std::unique_ptr<SocketPosix>* socket);
  void AcceptCompleted();

  int DoConnect();
  void ConnectCompleted();

  int DoRead(IOBuffer* buf, int buf_len);
  void RetryRead(int rv);
  void ReadCompleted();

  int DoWrite(IOBuffer* buf, int buf_len);
  void WriteCompleted();

  // |close_socket| indicates whether the socket should also be closed.
  void StopWatchingAndCleanUp(bool close_socket);

  SocketDescriptor socket_fd_;

  base::MessagePumpForIO::FdWatchController accept_socket_watcher_;
  raw_ptr<std::unique_ptr<SocketPosix>> accept_socket_;
  CompletionOnceCallback accept_callback_;

  base::MessagePumpForIO::FdWatchController read_socket_watcher_;

  // Non-null when a Read() is in progress.
  scoped_refptr<IOBuffer> read_buf_;
  int read_buf_len_ = 0;
  CompletionOnceCallback read_callback_;

  // Non-null when a ReadIfReady() is in progress.
  CompletionOnceCallback read_if_ready_callback_;

  base::MessagePumpForIO::FdWatchController write_socket_watcher_;
  scoped_refptr<IOBuffer> write_buf_;
  int write_buf_len_ = 0;
  // External callback; called when write or connect is complete.
  CompletionOnceCallback write_callback_;

  // A connect operation is pending. In this case, |write_callback_| needs to be
  // called when connect is complete.
  bool waiting_connect_ = false;

  std::unique_ptr<SockaddrStorage> peer_address_;

  base::ThreadChecker thread_checker_;

#if defined(DEBUG_CRBUG_40064248_STATISTICS)
  Statistics statistics_;
#endif  // DEBUG_CRBUG_40064248_STATISTICS
};

}  // namespace net

#endif  // NET_SOCKET_SOCKET_POSIX_H_
