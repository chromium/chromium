// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_TCP_SOCKET_WIN_H_
#define NET_SOCKET_TCP_SOCKET_WIN_H_

#include <winsock2.h>

#include <stdint.h>

#include <memory>

#include "base/check_deref.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/threading/thread_checker.h"
#include "base/win/object_watcher.h"
#include "net/base/address_family.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_export.h"
#include "net/base/network_handle.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/socket_descriptor.h"
#include "net/socket/socket_performance_watcher.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace net {

class TCPSocketDefaultWin;

class AddressList;
class IOBuffer;
class IPEndPoint;
class NetLog;
struct NetLogSource;
class SocketTag;

class NET_EXPORT TCPSocketWin : public base::win::ObjectWatcher::Delegate {
 public:
  static std::unique_ptr<TCPSocketWin> Create(
      std::unique_ptr<SocketPerformanceWatcher> socket_performance_watcher,
      NetLog* net_log,
      const NetLogSource& source);
  static std::unique_ptr<TCPSocketWin> Create(
      std::unique_ptr<SocketPerformanceWatcher> socket_performance_watcher,
      NetLogWithSource net_log_source);

  TCPSocketWin(const TCPSocketWin&) = delete;
  TCPSocketWin& operator=(const TCPSocketWin&) = delete;

  // IMPORTANT: All subclasses must call `Close`. The base class cannot do it
  // because `Close` invokes virtual methods, but it CHECKs that the socket is
  // closed.
  ~TCPSocketWin() override;

  int Open(AddressFamily family);

  // Takes ownership of `socket`, which is known to already be connected to the
  // given peer address. However, peer address may be the empty address, for
  // compatibility. The given peer address will be returned by GetPeerAddress.
  // `socket` must support overlapped I/O operations operations.
  int AdoptConnectedSocket(SocketDescriptor socket,
                           const IPEndPoint& peer_address);
  // Takes ownership of |socket|, which may or may not be open, bound, or
  // listening. The caller must determine the state of the socket based on its
  // provenance and act accordingly. The socket may have connections waiting to
  // be accepted, but must not be actually connected. `socket` must support
  // overlapped I/O operations operations.
  int AdoptUnconnectedSocket(SocketDescriptor socket);

  int Bind(const IPEndPoint& address);

  int Listen(int backlog);
  int Accept(std::unique_ptr<TCPSocketWin>* socket,
             IPEndPoint* address,
             CompletionOnceCallback callback);

  int Connect(const IPEndPoint& address, CompletionOnceCallback callback);
  bool IsConnected() const;
  bool IsConnectedAndIdle() const;

  // Multiple outstanding requests are not supported.
  // Full duplex mode (reading and writing at the same time) is supported.
  // These methods can only be called from an IO thread.
  virtual int Read(IOBuffer* buf,
                   int buf_len,
                   CompletionOnceCallback callback) = 0;
  virtual int ReadIfReady(IOBuffer* buf,
                          int buf_len,
                          CompletionOnceCallback callback) = 0;
  virtual int CancelReadIfReady() = 0;
  virtual int Write(IOBuffer* buf,
                    int buf_len,
                    CompletionOnceCallback callback,
                    const NetworkTrafficAnnotationTag& traffic_annotation) = 0;

  int GetLocalAddress(IPEndPoint* address) const;
  int GetPeerAddress(IPEndPoint* address) const;

  // Sets various socket options.
  // The commonly used options for server listening sockets:
  // - SetExclusiveAddrUse().
  int SetDefaultOptionsForServer();
  // The commonly used options for client sockets and accepted sockets:
  // - SetNoDelay(true);
  // - SetKeepAlive(true, 45).
  void SetDefaultOptionsForClient();
  int SetExclusiveAddrUse();
  int SetReceiveBufferSize(int32_t size);
  int SetSendBufferSize(int32_t size);
  bool SetKeepAlive(bool enable, int delay);
  bool SetNoDelay(bool no_delay);
  int SetIPv6Only(bool ipv6_only);

  // Gets the estimated RTT. Returns false if the RTT is
  // unavailable. May also return false when estimated RTT is 0.
  [[nodiscard]] bool GetEstimatedRoundTripTime(base::TimeDelta* out_rtt) const;

  void Close();

  bool IsValid() const { return socket_ != INVALID_SOCKET; }

  // Detachs from the current thread, to allow the socket to be transferred to
  // a new thread. Should only be called when the object is no longer used by
  // the old thread.
  void DetachFromThread();

  // Marks the start/end of a series of connect attempts for logging purpose.
  //
  // TCPClientSocket may attempt to connect to multiple addresses until it
  // succeeds in establishing a connection. The corresponding log will have
  // multiple NetLogEventType::TCP_CONNECT_ATTEMPT entries nested within a
  // NetLogEventType::TCP_CONNECT. These methods set the start/end of
  // NetLogEventType::TCP_CONNECT.
  //
  // TODO(yzshen): Change logging format and let TCPClientSocket log the
  // start/end of a series of connect attempts itself.
  void StartLoggingMultipleConnectAttempts(const AddressList& addresses);
  void EndLoggingMultipleConnectAttempts(int net_error);

  const NetLogWithSource& net_log() const { return net_log_; }

  // Return the underlying SocketDescriptor and clean up this object, which may
  // no longer be used. This method should be used only for testing. No read,
  // write, or accept operations should be pending.
  SocketDescriptor ReleaseSocketDescriptorForTesting();

  // Exposes the underlying socket descriptor for testing its state. Does not
  // release ownership of the descriptor.
  SocketDescriptor SocketDescriptorForTesting() const;

  // Closes the underlying socket descriptor but otherwise keeps this object
  // functional. Should only be used in `TCPSocketTest`.
  void CloseSocketDescriptorForTesting();

  // Apply |tag| to this socket.
  void ApplySocketTag(const SocketTag& tag);

  // Not implemented. Returns ERR_NOT_IMPLEMENTED.
  int BindToNetwork(handles::NetworkHandle network);

  // May return nullptr.
  SocketPerformanceWatcher* socket_performance_watcher() const {
    return socket_performance_watcher_.get();
  }

 protected:
  friend class TCPSocketDefaultWin;

  // Encapsulates state that must be preserved while network IO operations are
  // in progress. If the owning TCPSocketWin is destroyed while an operation is
  // in progress, the Core is detached and lives until the operation completes
  // and the OS doesn't reference any resource owned by it.
  class Core : public base::RefCounted<Core> {
   public:
    Core(const Core&) = delete;
    Core& operator=(const Core&) = delete;

    // Invoked when the socket is closed. Clears any reference from the `Core`
    // to its parent socket.
    virtual void Detach() = 0;

    // Returns the event to use for watching the completion of a connect()
    // operation.
    virtual HANDLE GetConnectEvent() = 0;

    // Must be invoked after initiating a connect() operation. Will invoke
    // `DidCompleteConnect()` when the connect() operation is complete.
    virtual void WatchForConnect() = 0;

   protected:
    friend class base::RefCounted<Core>;

    Core();
    virtual ~Core();
  };

  TCPSocketWin(
      std::unique_ptr<SocketPerformanceWatcher> socket_performance_watcher,
      NetLog* net_log,
      const NetLogSource& source);

  TCPSocketWin(
      std::unique_ptr<SocketPerformanceWatcher> socket_performance_watcher,
      NetLogWithSource net_log_source);

  // Instantiates a `Core` object for this socket.
  virtual scoped_refptr<Core> CreateCore() = 0;

  // Whether there is a pending read operation on this socket.
  virtual bool HasPendingRead() const = 0;

  // Invoked when the socket is closed.
  virtual void OnClosed() = 0;

  // base::ObjectWatcher::Delegate implementation.
  void OnObjectSignaled(HANDLE object) override;

  int AcceptInternal(std::unique_ptr<TCPSocketWin>* socket,
                     IPEndPoint* address);

  int DoConnect();
  void DoConnectComplete(int result);

  void LogConnectBegin(const AddressList& addresses);
  void LogConnectEnd(int net_error);

  void DidCompleteConnect();

  SOCKET socket_;

  // Whether `core_` is registered as an IO handler for `socket_` (see
  // `base::CurrentIOThread::RegisterIOHandler`). Calling
  // `ReleaseSocketDescriptorForTesting()` is disallowed when this is true, as
  // that could result in `core_` being notified of operations that weren't
  // issued by `this` (possibly after `core_` has been deleted).
  bool registered_as_io_handler_ = false;

  // |socket_performance_watcher_| may be nullptr.
  std::unique_ptr<SocketPerformanceWatcher> socket_performance_watcher_;

  HANDLE accept_event_;
  base::win::ObjectWatcher accept_watcher_;

  raw_ptr<std::unique_ptr<TCPSocketWin>> accept_socket_ = nullptr;
  raw_ptr<IPEndPoint> accept_address_ = nullptr;
  CompletionOnceCallback accept_callback_;

  // The core of the socket that can live longer than the socket itself. We pass
  // resources to the Windows async IO functions and we have to make sure that
  // they are not destroyed while the OS still references them.
  scoped_refptr<Core> core_;

  // Callback invoked when connect is complete.
  CompletionOnceCallback connect_callback_;

  std::unique_ptr<IPEndPoint> peer_address_;
  // The OS error that a connect attempt last completed with.
  int connect_os_error_ = 0;

  bool logging_multiple_connect_attempts_ = false;

  NetLogWithSource net_log_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace net

#endif  // NET_SOCKET_TCP_SOCKET_WIN_H_
