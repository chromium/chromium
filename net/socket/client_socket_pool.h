// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_CLIENT_SOCKET_POOL_H_
#define NET_SOCKET_CLIENT_SOCKET_POOL_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "net/base/completion_once_callback.h"
#include "net/base/load_states.h"
#include "net/base/net_export.h"
#include "net/base/network_isolation_key.h"
#include "net/base/privacy_mode.h"
#include "net/base/request_priority.h"
#include "net/dns/host_resolver.h"
#include "net/http/http_request_info.h"
#include "net/log/net_log_capture_mode.h"
#include "net/socket/connect_job.h"
#include "net/socket/socket_tag.h"

namespace base {
class Value;
namespace trace_event {
class ProcessMemoryDump;
}
}  // namespace base

namespace net {

class ClientSocketHandle;
struct CommonConnectJobParams;
class HttpAuthController;
class HttpResponseInfo;
class NetLogWithSource;
struct NetworkTrafficAnnotationTag;
class ProxyServer;
struct SSLConfig;
class StreamSocket;

// ClientSocketPools are layered. This defines an interface for lower level
// socket pools to communicate with higher layer pools.
class NET_EXPORT HigherLayeredPool {
 public:
  virtual ~HigherLayeredPool() {}

  // Instructs the HigherLayeredPool to close an idle connection. Return true if
  // one was closed.  Closing an idle connection will call into the lower layer
  // pool it came from, so must be careful of re-entrancy when using this.
  virtual bool CloseOneIdleConnection() = 0;
};

// ClientSocketPools are layered. This defines an interface for higher level
// socket pools to communicate with lower layer pools.
class NET_EXPORT LowerLayeredPool {
 public:
  virtual ~LowerLayeredPool() {}

  // Returns true if a there is currently a request blocked on the per-pool
  // (not per-host) max socket limit, either in this pool, or one that it is
  // layered on top of.
  virtual bool IsStalled() const = 0;

  // Called to add or remove a higher layer pool on top of |this|.  A higher
  // layer pool may be added at most once to |this|, and must be removed prior
  // to destruction of |this|.
  virtual void AddHigherLayeredPool(HigherLayeredPool* higher_pool) = 0;
  virtual void RemoveHigherLayeredPool(HigherLayeredPool* higher_pool) = 0;
};

// A ClientSocketPool is used to restrict the number of sockets open at a time.
// It also maintains a list of idle persistent sockets.
//
// Subclasses must also have an inner class SocketParams which is
// the type for the |params| argument in RequestSocket() and
// RequestSockets() below.
class NET_EXPORT ClientSocketPool : public LowerLayeredPool {
 public:
  // Indicates whether or not a request for a socket should respect the
  // SocketPool's global and per-group socket limits.
  enum class RespectLimits { DISABLED, ENABLED };

  // ProxyAuthCallback is invoked when there is an auth challenge while
  // connecting to a tunnel. When |restart_with_auth_callback| is invoked, the
  // corresponding socket request is guaranteed not to be completed
  // synchronously, nor will the ProxyAuthCallback be invoked against
  // synchronously.
  typedef base::RepeatingCallback<void(
      const HttpResponseInfo& response,
      HttpAuthController* auth_controller,
      base::OnceClosure restart_with_auth_callback)>
      ProxyAuthCallback;

  enum class SocketType {
    kHttp,

    // This is a connection that uses an SSL connection to the final
    // destination, though not necessarily to the proxy, if there is one.
    kSsl,
  };

  // Group ID for a socket request. Requests with the same group ID are
  // considered indistinguishable.
  class NET_EXPORT GroupId {
   public:
    GroupId();
    GroupId(const HostPortPair& destination,
            SocketType socket_type,
            PrivacyMode privacy_mode,
            NetworkIsolationKey network_isolation_key,
            bool disable_secure_dns);
    GroupId(const GroupId& group_id);

    ~GroupId();

    GroupId& operator=(const GroupId& group_id);
    GroupId& operator=(GroupId&& group_id);

    const HostPortPair& destination() const { return destination_; }

    SocketType socket_type() const { return socket_type_; }

    PrivacyMode privacy_mode() const { return privacy_mode_; }

    const NetworkIsolationKey& network_isolation_key() const {
      return network_isolation_key_;
    }

    bool disable_secure_dns() const { return disable_secure_dns_; }

    // Returns the group ID as a string, for logging.
    std::string ToString() const;

    bool operator==(const GroupId& other) const {
      return std::tie(destination_, socket_type_, privacy_mode_,
                      network_isolation_key_, disable_secure_dns_) ==
             std::tie(other.destination_, other.socket_type_,
                      other.privacy_mode_, other.network_isolation_key_,
                      other.disable_secure_dns_);
    }

    bool operator<(const GroupId& other) const {
      return std::tie(destination_, socket_type_, privacy_mode_,
                      network_isolation_key_, disable_secure_dns_) <
             std::tie(other.destination_, other.socket_type_,
                      other.privacy_mode_, other.network_isolation_key_,
                      other.disable_secure_dns_);
    }

   private:
    // The host and port of the final destination (not the proxy).
    HostPortPair destination_;

    SocketType socket_type_;

    // If this request is for a privacy mode / uncredentialed connection.
    PrivacyMode privacy_mode_;

    // Used to separate requests made in different contexts.
    NetworkIsolationKey network_isolation_key_;

    // If host resolutions for this request may not use secure DNS.
    bool disable_secure_dns_;
  };

  // Parameters that, in combination with GroupId, proxy, websocket information,
  // and global state, are sufficient to create a ConnectJob.
  //
  // DO NOT ADD ANY FIELDS TO THIS CLASS.
  //
  // TODO(https://crbug.com/921369) In order to resolve longstanding issues
  // related to pooling distinguishable sockets together, remove this class
  // entirely.
  class NET_EXPORT_PRIVATE SocketParams
      : public base::RefCounted<SocketParams> {
   public:
    // For non-SSL requests / non-HTTPS proxies, the corresponding SSLConfig
    // argument may be nullptr.
    SocketParams(std::unique_ptr<SSLConfig> ssl_config_for_origin,
                 std::unique_ptr<SSLConfig> ssl_config_for_proxy);

    // Creates a  SocketParams object with none of the fields populated. This
    // works for the HTTP case only.
    static scoped_refptr<SocketParams> CreateForHttpForTesting();

    const SSLConfig* ssl_config_for_origin() const {
      return ssl_config_for_origin_.get();
    }

    const SSLConfig* ssl_config_for_proxy() const {
      return ssl_config_for_proxy_.get();
    }

   private:
    friend class base::RefCounted<SocketParams>;
    ~SocketParams();

    std::unique_ptr<SSLConfig> ssl_config_for_origin_;
    std::unique_ptr<SSLConfig> ssl_config_for_proxy_;

    DISALLOW_COPY_AND_ASSIGN(SocketParams);
  };

  ~ClientSocketPool() override;

  // Requests a connected socket with a specified GroupId.
  //
  // There are five possible results from calling this function:
  // 1) RequestSocket returns OK and initializes |handle| with a reused socket.
  // 2) RequestSocket returns OK with a newly connected socket.
  // 3) RequestSocket returns ERR_IO_PENDING.  The handle will be added to a
  // wait list until a socket is available to reuse or a new socket finishes
  // connecting.  |priority| will determine the placement into the wait list.
  // 4) An error occurred early on, so RequestSocket returns an error code.
  // 5) A recoverable error occurred while setting up the socket.  An error
  // code is returned, but the |handle| is initialized with the new socket.
  // The caller must recover from the error before using the connection, or
  // Disconnect the socket before releasing or resetting the |handle|.
  // The current recoverable errors are: the errors accepted by
  // IsCertificateError(err) and HTTPS_PROXY_TUNNEL_RESPONSE when reported by
  // HttpProxyClientSocketPool.
  //
  // If this function returns OK, then |handle| is initialized upon return.
  // The |handle|'s is_initialized method will return true in this case.  If a
  // StreamSocket was reused, then ClientSocketPool will call
  // |handle|->set_reused(true).  In either case, the socket will have been
  // allocated and will be connected.  A client might want to know whether or
  // not the socket is reused in order to request a new socket if it encounters
  // an error with the reused socket.
  //
  // If ERR_IO_PENDING is returned, then the callback will be used to notify the
  // client of completion.
  //
  // Profiling information for the request is saved to |net_log| if non-NULL.
  //
  // If |respect_limits| is DISABLED, priority must be HIGHEST.
  //
  // |proxy_annotation_tag| is the annotation used for proxy-related reads and
  // writes, and may be nullopt if (and only if) no proxy is in use.
  //
  // |proxy_auth_callback| will be invoked each time an auth challenge is seen
  // while establishing a tunnel. It will be invoked asynchronously, once for
  // each auth challenge seen.
  virtual int RequestSocket(
      const GroupId& group_id,
      scoped_refptr<SocketParams> params,
      const base::Optional<NetworkTrafficAnnotationTag>& proxy_annotation_tag,
      RequestPriority priority,
      const SocketTag& socket_tag,
      RespectLimits respect_limits,
      ClientSocketHandle* handle,
      CompletionOnceCallback callback,
      const ProxyAuthCallback& proxy_auth_callback,
      const NetLogWithSource& net_log) = 0;

  // RequestSockets is used to request that |num_sockets| be connected in the
  // connection group for |group_id|.  If the connection group already has
  // |num_sockets| idle sockets / active sockets / currently connecting sockets,
  // then this function doesn't do anything.  Otherwise, it will start up as
  // many connections as necessary to reach |num_sockets| total sockets for the
  // group.  It uses |params| to control how to connect the sockets.   The
  // ClientSocketPool will assign a priority to the new connections, if any.
  // This priority will probably be lower than all others, since this method
  // is intended to make sure ahead of time that |num_sockets| sockets are
  // available to talk to a host.
  virtual void RequestSockets(
      const GroupId& group_id,
      scoped_refptr<SocketParams> params,
      const base::Optional<NetworkTrafficAnnotationTag>& proxy_annotation_tag,
      int num_sockets,
      const NetLogWithSource& net_log) = 0;

  // Called to change the priority of a RequestSocket call that returned
  // ERR_IO_PENDING and has not yet asynchronously completed.  The same handle
  // parameter must be passed to this method as was passed to the
  // RequestSocket call being modified.
  // This function is a no-op if |priority| is the same as the current
  // request priority.
  virtual void SetPriority(const GroupId& group_id,
                           ClientSocketHandle* handle,
                           RequestPriority priority) = 0;

  // Called to cancel a RequestSocket call that returned ERR_IO_PENDING.  The
  // same handle parameter must be passed to this method as was passed to the
  // RequestSocket call being cancelled.  The associated callback is not run.
  // If |cancel_connect_job| is true, and there are more ConnectJobs than
  // requests, a ConnectJob will be canceled. If it's false, excess ConnectJobs
  // may be allowed to continue, just in case there are new requests to the same
  // endpoint.
  virtual void CancelRequest(const GroupId& group_id,
                             ClientSocketHandle* handle,
                             bool cancel_connect_job) = 0;

  // Called to release a socket once the socket is no longer needed.  If the
  // socket still has an established connection, then it will be added to the
  // set of idle sockets to be used to satisfy future RequestSocket calls.
  // Otherwise, the StreamSocket is destroyed.  |generation| is used to
  // differentiate between updated versions of the same pool instance.  The
  // pool's generation will change when it flushes, so it can use this
  // |generation| to discard sockets with mismatched ids.
  virtual void ReleaseSocket(const GroupId& group_id,
                             std::unique_ptr<StreamSocket> socket,
                             int64_t generation) = 0;

  // This flushes all state from the ClientSocketPool.  This means that all
  // idle and connecting sockets are discarded with the given |error|.
  // Active sockets being held by ClientSocketPool clients will be discarded
  // when released back to the pool.
  // Does not flush any pools wrapped by |this|.
  virtual void FlushWithError(int error) = 0;

  // Called to close any idle connections held by the connection manager.
  virtual void CloseIdleSockets() = 0;

  // Called to close any idle connections held by the connection manager.
  virtual void CloseIdleSocketsInGroup(const GroupId& group_id) = 0;

  // The total number of idle sockets in the pool.
  virtual int IdleSocketCount() const = 0;

  // The total number of idle sockets in a connection group.
  virtual size_t IdleSocketCountInGroup(const GroupId& group_id) const = 0;

  // Determine the LoadState of a connecting ClientSocketHandle.
  virtual LoadState GetLoadState(const GroupId& group_id,
                                 const ClientSocketHandle* handle) const = 0;

  // Retrieves information on the current state of the pool as a
  // Value.
  // If |include_nested_pools| is true, the states of any nested
  // ClientSocketPools will be included.
  virtual base::Value GetInfoAsValue(const std::string& name,
                                     const std::string& type) const = 0;

  // Dumps memory allocation stats. |parent_dump_absolute_name| is the name
  // used by the parent MemoryAllocatorDump in the memory dump hierarchy.
  virtual void DumpMemoryStats(
      base::trace_event::ProcessMemoryDump* pmd,
      const std::string& parent_dump_absolute_name) const = 0;

  // Returns the maximum amount of time to wait before retrying a connect.
  static const int kMaxConnectRetryIntervalMs = 250;

  static base::TimeDelta used_idle_socket_timeout();
  static void set_used_idle_socket_timeout(base::TimeDelta timeout);

 protected:
  ClientSocketPool();

  void NetLogTcpClientSocketPoolRequestedSocket(const NetLogWithSource& net_log,
                                                const GroupId& group_id);

  // Utility method to log a GroupId with a NetLog event.
  static base::Value NetLogGroupIdParams(const GroupId& group_id);

  static std::unique_ptr<ConnectJob> CreateConnectJob(
      GroupId group_id,
      scoped_refptr<SocketParams> socket_params,
      const ProxyServer& proxy_server,
      const base::Optional<NetworkTrafficAnnotationTag>& proxy_annotation_tag,
      bool is_for_websockets,
      const CommonConnectJobParams* common_connect_job_params,
      RequestPriority request_priority,
      SocketTag socket_tag,
      ConnectJob::Delegate* delegate);

 private:
  DISALLOW_COPY_AND_ASSIGN(ClientSocketPool);
};

}  // namespace net

#endif  // NET_SOCKET_CLIENT_SOCKET_POOL_H_
