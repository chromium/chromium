// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_TCP_SOCKET_IO_COMPLETION_PORT_WIN_H_
#define NET_SOCKET_TCP_SOCKET_IO_COMPLETION_PORT_WIN_H_

#include <memory>

#include "base/win/windows_types.h"
#include "net/base/net_export.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/tcp_socket_win.h"

namespace net {

class NetLog;
struct NetLogSource;
class SocketPerformanceWatcher;

// An implementation of TCPSocketWin which uses an IO completion port to be
// notified of completed reads and writes. The goal is to avoid the PostTask
// overhead associated with the use of base::ObjectWatcher in
// TCPSocketDefaultWin.
class NET_EXPORT TcpSocketIoCompletionPortWin : public TCPSocketWin {
 public:
  // Disables usage of FILE_SKIP_COMPLETION_PORT_ON_SUCCESS in a scope. This
  // only affect sockets on which `Read()` or `Write()` hasn't been called yet.
  class NET_EXPORT DisableSkipCompletionPortOnSuccessForTesting {
   public:
    DisableSkipCompletionPortOnSuccessForTesting();
    ~DisableSkipCompletionPortOnSuccessForTesting();
    DisableSkipCompletionPortOnSuccessForTesting(
        const DisableSkipCompletionPortOnSuccessForTesting&) = delete;
    DisableSkipCompletionPortOnSuccessForTesting& operator=(
        const DisableSkipCompletionPortOnSuccessForTesting&) = delete;
  };

  TcpSocketIoCompletionPortWin(
      std::unique_ptr<SocketPerformanceWatcher> socket_performance_watcher,
      NetLog* net_log,
      const NetLogSource& source);

  TcpSocketIoCompletionPortWin(
      std::unique_ptr<SocketPerformanceWatcher> socket_performance_watcher,
      NetLogWithSource net_log_source);

  ~TcpSocketIoCompletionPortWin() override;

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

  // Ensures that overlapped IO is initialized for `socket_`. This entails:
  // - Registering `core_` as an IO handler.
  // - Attempting to activate `FILE_SKIP_COMPLETION_PORT_ON_SUCCESS`.
  void EnsureOverlappedIOInitialized();

  // Handles a completed read/write operation on `socket_`. `bytes_transferred`
  // is the number of bytes actually read/written. `error` is the error code for
  // the operation. `buffer` is the buffer used to initiate the read/write
  // operation and `buffer_length` is its size (`bytes_transferred` should be <=
  // `buffer_length`). Returns the value to pass to the completion callback
  // associated with the operation (<0 is an error code, >=0 is a number of
  // bytes transferred).
  int DidCompleteRead(DWORD bytes_transferred,
                      DWORD error,
                      scoped_refptr<IOBuffer> buffer,
                      int buffer_length);
  int DidCompleteWrite(DWORD bytes_transferred,
                       DWORD error,
                       scoped_refptr<IOBuffer> buffer,
                       int buffer_length);

  CoreImpl& GetCoreImpl();

  // Number of read operations waiting for an I/O completion packet.
  int num_pending_reads_ = 0;

  // Whether queuing a completion packet is skipped when an operation on
  // `socket_` succeeds immediately.
  bool skip_completion_port_on_success_ = false;
};

}  // namespace net

#endif  // NET_SOCKET_TCP_SOCKET_IO_COMPLETION_PORT_WIN_H_
