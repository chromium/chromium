// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_CLIENT_SOCKET_HANDLE_H_
#define NET_SOCKET_CLIENT_SOCKET_HANDLE_H_

#include <memory>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "net/base/ip_endpoint.h"
#include "net/base/load_states.h"
#include "net/base/load_timing_info.h"
#include "net/base/net_errors.h"
#include "net/base/net_export.h"
#include "net/base/request_priority.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/log/net_log_source.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/client_socket_pool.h"
#include "net/socket/connection_attempts.h"
#include "net/socket/stream_socket.h"
#include "net/socket/stream_socket_handle.h"
#include "net/ssl/ssl_cert_request_info.h"

namespace net {

class ConnectJob;
struct NetworkTrafficAnnotationTag;
class SocketTag;

// A container for a StreamSocket.
//
// The handle's |group_id| uniquely identifies the origin and type of the
// connection.  It is used by the ClientSocketPool to group similar connected
// client socket objects.
//
class NET_EXPORT ClientSocketHandle : public StreamSocketHandle {
 public:
  ClientSocketHandle();

  ClientSocketHandle(const ClientSocketHandle&) = delete;
  ClientSocketHandle& operator=(const ClientSocketHandle&) = delete;

  ~ClientSocketHandle() override;

  // Initializes a ClientSocketHandle object, which involves talking to the
  // ClientSocketPool to obtain a connected socket, possibly reusing one.  This
  // method returns either OK or ERR_IO_PENDING.  On ERR_IO_PENDING, |priority|
  // is used to determine the placement in ClientSocketPool's wait list.
  // If |respect_limits| is DISABLED, will bypass the wait list, but |priority|
  // must also be HIGHEST, if set.
  //
  // If this method succeeds, then the socket member will be set to an existing
  // connected socket if an existing connected socket was available to reuse,
  // otherwise it will be set to a new connected socket.  Consumers can then
  // call is_reused() to see if the socket was reused.  If not reusing an
  // existing socket, ClientSocketPool may need to establish a new
  // connection using |socket_params|.
  //
  // This method returns ERR_IO_PENDING if it cannot complete synchronously, in
  // which case the consumer will be notified of completion via |callback|.
  //
  // If the pool was not able to reuse an existing socket, the new socket
  // may report a recoverable error.  In this case, the return value will
  // indicate an error and the socket member will be set.  If it is determined
  // that the error is not recoverable, the Disconnect method should be used
  // on the socket, so that it does not get reused.
  //
  // A non-recoverable error may set additional state in the ClientSocketHandle
  // to allow the caller to determine what went wrong.
  //
  // Init may be called multiple times.
  //
  // Profiling information for the request is saved to |net_log| if non-NULL.
  int Init(
      const ClientSocketPool::GroupId& group_id,
      scoped_refptr<ClientSocketPool::SocketParams> socket_params,
      const std::optional<NetworkTrafficAnnotationTag>& proxy_annotation_tag,
      RequestPriority priority,
      const SocketTag& socket_tag,
      ClientSocketPool::RespectLimits respect_limits,
      CompletionOnceCallback callback,
      const ClientSocketPool::ProxyAuthCallback& proxy_auth_callback,
      ClientSocketPool* pool,
      const NetLogWithSource& net_log);

  // Changes the priority of the ClientSocketHandle to the passed value.
  // This function is a no-op if |priority| is the same as the current
  // priority, of if Init() has not been called since the last time
  // the ClientSocketHandle was reset.
  void SetPriority(RequestPriority priority);

  // An initialized handle can be reset, which causes it to return to the
  // un-initialized state.  This releases the underlying socket, which in the
  // case of a socket that still has an established connection, indicates that
  // the socket may be kept alive for use by a subsequent ClientSocketHandle.
  //
  // NOTE: To prevent the socket from being kept alive, be sure to call its
  // Disconnect method.  This will result in the ClientSocketPool deleting the
  // StreamSocket.
  void Reset() override;

  // Like Reset(), but also closes the socket (if there is one) and cancels any
  // pending attempt to establish a connection, if the connection attempt is
  // still ongoing.
  void ResetAndCloseSocket();

  // Used after Init() is called, but before the ClientSocketPool has
  // initialized the ClientSocketHandle.
  LoadState GetLoadState() const;

  bool IsPoolStalled() const override;

  // Adds a higher layered pool on top of the socket pool that |socket_| belongs
  // to.  At most one higher layered pool can be added to a
  // ClientSocketHandle at a time.  On destruction or reset, automatically
  // removes the higher pool if RemoveHigherLayeredPool has not been called.
  void AddHigherLayeredPool(HigherLayeredPool* higher_pool) override;

  // Removes a higher layered pool from the socket pool that |socket_| belongs
  // to.  |higher_pool| must have been added by the above function.
  void RemoveHigherLayeredPool(HigherLayeredPool* higher_pool) override;

  // Closes idle sockets that are in the same group with |this|.
  void CloseIdleSocketsInGroup(const char* net_log_reason_utf8);

  // Populates several fields of |this| with error-related information from the
  // provided completed ConnectJob. Should only be called on ConnectJob failure.
  void SetAdditionalErrorState(ConnectJob* connect_job);

  void set_idle_time(base::TimeDelta idle_time) { idle_time_ = idle_time; }
  void set_group_generation(int64_t group_generation) {
    group_generation_ = group_generation;
  }
  void set_is_ssl_error(bool is_ssl_error) { is_ssl_error_ = is_ssl_error; }
  void set_ssl_cert_request_info(
      scoped_refptr<SSLCertRequestInfo> ssl_cert_request_info) {
    ssl_cert_request_info_ = std::move(ssl_cert_request_info);
  }
  void set_connection_attempts(const ConnectionAttempts& attempts) {
    connection_attempts_ = attempts;
  }
  ResolveErrorInfo resolve_error_info() const { return resolve_error_info_; }

  // Only valid if there is no |socket_|.
  bool is_ssl_error() const {
    DCHECK(!socket());
    return is_ssl_error_;
  }

  // On an ERR_SSL_CLIENT_AUTH_CERT_NEEDED error, the |cert_request_info| field
  // is set.
  scoped_refptr<SSLCertRequestInfo> ssl_cert_request_info() const {
    return ssl_cert_request_info_;
  }

  // If the connection failed, returns the connection attempts made.
  const ConnectionAttempts& connection_attempts() {
    return connection_attempts_;
  }

  // These may only be used if is_initialized() is true.
  const ClientSocketPool::GroupId& group_id() const { return group_id_; }
  int64_t group_generation() const { return group_generation_; }
  bool is_reused() const {
    return reuse_type() == SocketReuseType::kReusedIdle;
  }
  base::TimeDelta idle_time() const { return idle_time_; }

  base::WeakPtr<ClientSocketHandle> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  // Called on asynchronous completion of an Init() request.
  void OnIOComplete(int result);

  // Called on completion (both asynchronous & synchronous) of an Init()
  // request.
  void HandleInitCompletion(int result);

  // Resets the state of the ClientSocketHandle.  |cancel| indicates whether or
  // not to try to cancel the request with the ClientSocketPool.  Does not
  // reset the supplemental error state. |cancel_connect_job| indicates whether
  // a pending ConnectJob, if there is one in the SocketPool, should be
  // cancelled in addition to cancelling the request. It may only be true if
  // |cancel| is also true.
  void ResetInternal(bool cancel, bool cancel_connect_job);

  // Resets the supplemental error state.
  void ResetErrorState();

  raw_ptr<ClientSocketPool> pool_ = nullptr;
  raw_ptr<HigherLayeredPool> higher_pool_ = nullptr;
  ClientSocketPool::GroupId group_id_;
  CompletionOnceCallback callback_;
  base::TimeDelta idle_time_;
  // See ClientSocketPool::ReleaseSocket() for an explanation.
  int64_t group_generation_ = -1;
  ResolveErrorInfo resolve_error_info_;
  bool is_ssl_error_ = false;
  scoped_refptr<SSLCertRequestInfo> ssl_cert_request_info_;
  std::vector<ConnectionAttempt> connection_attempts_;

  NetLogSource requesting_source_;

  base::WeakPtrFactory<ClientSocketHandle> weak_factory_{this};
};

}  // namespace net

#endif  // NET_SOCKET_CLIENT_SOCKET_HANDLE_H_
