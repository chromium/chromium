// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_CLIENT_SOCKET_HANDLE_H_
#define NET_SOCKET_CLIENT_SOCKET_HANDLE_H_

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "net/base/ip_endpoint.h"
#include "net/base/load_states.h"
#include "net/base/load_timing_info.h"
#include "net/base/net_errors.h"
#include "net/base/net_export.h"
#include "net/base/request_priority.h"
#include "net/log/net_log_source.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/client_socket_pool.h"
#include "net/socket/connection_attempts.h"
#include "net/socket/stream_socket.h"
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
class NET_EXPORT ClientSocketHandle {
 public:
  enum SocketReuseType {
    UNUSED = 0,   // unused socket that just finished connecting
    UNUSED_IDLE,  // unused socket that has been idle for awhile
    REUSED_IDLE,  // previously used socket
    NUM_TYPES,
  };

  ClientSocketHandle();
  ~ClientSocketHandle();

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
      const base::Optional<NetworkTrafficAnnotationTag>& proxy_annotation_tag,
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
  void Reset();

  // Like Reset(), but also closes the socket (if there is one) and cancels any
  // pending attempt to establish a connection, if the connection attempt is
  // still ongoing.
  void ResetAndCloseSocket();

  // Used after Init() is called, but before the ClientSocketPool has
  // initialized the ClientSocketHandle.
  LoadState GetLoadState() const;

  bool IsPoolStalled() const;

  // Adds a higher layered pool on top of the socket pool that |socket_| belongs
  // to.  At most one higher layered pool can be added to a
  // ClientSocketHandle at a time.  On destruction or reset, automatically
  // removes the higher pool if RemoveHigherLayeredPool has not been called.
  void AddHigherLayeredPool(HigherLayeredPool* higher_pool);

  // Removes a higher layered pool from the socket pool that |socket_| belongs
  // to.  |higher_pool| must have been added by the above function.
  void RemoveHigherLayeredPool(HigherLayeredPool* higher_pool);

  // Closes idle sockets that are in the same group with |this|.
  void CloseIdleSocketsInGroup();

  // Returns true when Init() has completed successfully.
  bool is_initialized() const { return is_initialized_; }

  // Sets the portion of LoadTimingInfo related to connection establishment, and
  // the socket id.  |is_reused| is needed because the handle may not have full
  // reuse information.  |load_timing_info| must have all default values when
  // called. Returns false and makes no changes to |load_timing_info| when
  // |socket_| is NULL.
  bool GetLoadTimingInfo(bool is_reused,
                         LoadTimingInfo* load_timing_info) const;

  // Dumps memory allocation stats into |stats|. |stats| can be assumed as being
  // default initialized upon entry. Implementation overrides fields in
  // |stats|.
  void DumpMemoryStats(StreamSocket::SocketMemoryStats* stats) const;

  // Used by ClientSocketPool to initialize the ClientSocketHandle.
  //
  // SetSocket() may also be used if this handle is used as simply for
  // socket storage (e.g., http://crbug.com/37810).
  void SetSocket(std::unique_ptr<StreamSocket> s);

  // Populates several fields of |this| with error-related information from the
  // provided completed ConnectJob. Should only be called on ConnectJob failure.
  void SetAdditionalErrorState(ConnectJob* connect_job);

  void set_reuse_type(SocketReuseType reuse_type) { reuse_type_ = reuse_type; }
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

  // Only valid if there is no |socket_|.
  bool is_ssl_error() const {
    DCHECK(!socket_);
    return is_ssl_error_;
  }

  // On an ERR_SSL_CLIENT_AUTH_CERT_NEEDED error, the |cert_request_info| field
  // is set.
  scoped_refptr<SSLCertRequestInfo> ssl_cert_request_info() const {
    return ssl_cert_request_info_;
  }

  // If the connection failed, returns the connection attempts made. (If it
  // succeeded, they will be returned through the socket instead; see
  // |StreamSocket::GetConnectionAttempts|.)
  const ConnectionAttempts& connection_attempts() {
    return connection_attempts_;
  }

  StreamSocket* socket() { return socket_.get(); }

  // SetSocket() must be called with a new socket before this handle
  // is destroyed if is_initialized() is true.
  std::unique_ptr<StreamSocket> PassSocket();

  // These may only be used if is_initialized() is true.
  const ClientSocketPool::GroupId& group_id() const { return group_id_; }
  int64_t group_generation() const { return group_generation_; }
  bool is_reused() const { return reuse_type_ == REUSED_IDLE; }
  base::TimeDelta idle_time() const { return idle_time_; }
  SocketReuseType reuse_type() const { return reuse_type_; }
  const LoadTimingInfo::ConnectTiming& connect_timing() const {
    return connect_timing_;
  }
  void set_connect_timing(const LoadTimingInfo::ConnectTiming& connect_timing) {
    connect_timing_ = connect_timing;
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

  bool is_initialized_;
  ClientSocketPool* pool_;
  HigherLayeredPool* higher_pool_;
  std::unique_ptr<StreamSocket> socket_;
  ClientSocketPool::GroupId group_id_;
  SocketReuseType reuse_type_;
  CompletionOnceCallback callback_;
  base::TimeDelta idle_time_;
  // See ClientSocketPool::ReleaseSocket() for an explanation.
  int64_t group_generation_;
  bool is_ssl_error_;
  scoped_refptr<SSLCertRequestInfo> ssl_cert_request_info_;
  std::vector<ConnectionAttempt> connection_attempts_;

  NetLogSource requesting_source_;

  // Timing information is set when a connection is successfully established.
  LoadTimingInfo::ConnectTiming connect_timing_;

  DISALLOW_COPY_AND_ASSIGN(ClientSocketHandle);
};

}  // namespace net

#endif  // NET_SOCKET_CLIENT_SOCKET_HANDLE_H_
