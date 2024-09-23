// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_CLIENT_SOCKET_POOL_H_
#define NET_SOCKET_CLIENT_SOCKET_POOL_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "base/values.h"
#include "net/base/completion_once_callback.h"
#include "net/base/load_states.h"
#include "net/base/net_export.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/privacy_mode.h"
#include "net/base/request_priority.h"
#include "net/dns/host_resolver.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/http/http_request_info.h"
#include "net/log/net_log_capture_mode.h"
#include "net/socket/connect_job.h"
#include "net/socket/socket_tag.h"
#include "net/ssl/ssl_config.h"
#include "url/scheme_host_port.h"

namespace net {

class ClientSocketHandle;
class ConnectJobFactory;
class HttpAuthController;
class HttpResponseInfo;
class NetLogWithSource;
struct NetworkTrafficAnnotationTag;
class ProxyChain;
struct SSLConfig;
class StreamSocket;

// ClientSocketPools are layered. This defines an interface for lower level
// socket pools to communicate with higher layer pools.
class NET_EXPORT HigherLayeredPool {
 public:
  virtual ~HigherLayeredPool() = default;

  // Instructs the HigherLayeredPool to close an idle connection. Return true if
  // one was closed.  Closing an idle connection will call into the lower layer
  // pool it came from, so must be careful of re-entrancy when using this.
  virtual bool CloseOneIdleConnection() = 0;
};

// ClientSocketPools are layered. This defines an interface for higher level
// socket pools to communicate with lower layer pools.
class NET_EXPORT LowerLayeredPool {
 public:
  virtual ~LowerLayeredPool() = default;

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

  // Group ID for a socket request. Requests with the same group ID are
  // considered indistinguishable.
  class NET_EXPORT GroupId {
   public:
    // Returns the prefix for `privacy_mode` for logging.
    static std::string_view GetPrivacyModeGroupIdPrefix(
        PrivacyMode privacy_mode);

    // Returns the prefix for `secure_dns_policy` for logging.
    static std::string_view GetSecureDnsPolicyGroupIdPrefix(
        SecureDnsPolicy secure_dns_policy);

    GroupId();
    GroupId(url::SchemeHostPort destination,
            PrivacyMode privacy_mode,
            NetworkAnonymizationKey network_anonymization_key,
            SecureDnsPolicy secure_dns_policy,
            bool disable_cert_network_fetches);
    GroupId(const GroupId& group_id);

    ~GroupId();

    GroupId& operator=(const GroupId& group_id);
    GroupId& operator=(GroupId&& group_id);

    const url::SchemeHostPort& destination() const { return destination_; }

    PrivacyMode privacy_mode() const { return privacy_mode_; }

    const NetworkAnonymizationKey& network_anonymization_key() const {
      return network_anonymization_key_;
    }

    SecureDnsPolicy secure_dns_policy() const { return secure_dns_policy_; }

    bool disable_cert_network_fetches() const {
      return disable_cert_network_fetches_;
    }

    // Returns the group ID as a string, for logging.
    std::string ToString() const;

    bool operator==(const GroupId& other) const {
      return std::tie(destination_, privacy_mode_, network_anonymization_key_,
                      secure_dns_policy_, disable_cert_network_fetches_) ==
             std::tie(other.destination_, other.privacy_mode_,
                      other.network_anonymization_key_,
                      other.secure_dns_policy_,
                      other.disable_cert_network_fetches_);
    }

    bool operator<(const GroupId& other) const {
      return std::tie(destination_, privacy_mode_, network_anonymization_key_,
                      secure_dns_policy_, disable_cert_network_fetches_) <
             std::tie(other.destination_, other.privacy_mode_,
                      other.network_anonymization_key_,
                      other.secure_dns_policy_,
                      other.disable_cert_network_fetches_);
    }

   private:
    // The endpoint of the final destination (not the proxy).
    url::SchemeHostPort destination_;

    // If this request is for a privacy mode / uncredentialed connection.
    PrivacyMode privacy_mode_;

    // Used to separate requests made in different contexts.
    NetworkAnonymizationKey network_anonymization_key_;

    // Controls the Secure DNS behavior to use when creating this socket.
    SecureDnsPolicy secure_dns_policy_;

    // Whether cert validation-related network fetches are allowed. Should only
    // be true for a very limited number of network-configuration related
    // scripts (e.g., PAC fetches).
    bool disable_cert_network_fetches_;
  };

  // Parameters that, in combination with GroupId, proxy, websocket information,
  // and global state, are sufficient to create a ConnectJob.
  //
  // DO NOT ADD ANY FIELDS TO THIS CLASS.
  //
  // TODO(crbug.com/40609237) In order to resolve longstanding issues
  // related to pooling distinguishable sockets together, remove this class
  // entirely.
  class NET_EXPORT_PRIVATE SocketParams
      : public base::RefCounted<SocketParams> {
   public:
    // For non-SSL requests, `allowed_bad_certs` argument will be ignored (and
    // is likely empty, anyways).
    explicit SocketParams(
        const std::vector<SSLConfig::CertAndStatus>& allowed_bad_certs);

    SocketParams(const SocketParams&) = delete;
    SocketParams& operator=(const SocketParams&) = delete;

    // Creates a SocketParams object with none of the fields populated. This
    // works for the HTTP case only.
    static scoped_refptr<SocketParams> CreateForHttpForTesting();

    const std::vector<SSLConfig::CertAndStatus>& allowed_bad_certs() const {
      return allowed_bad_certs_;
    }

   private:
    friend class base::RefCounted<SocketParams>;
    ~SocketParams();

    std::vector<SSLConfig::CertAndStatus> allowed_bad_certs_;
  };

  ClientSocketPool(const ClientSocketPool&) = delete;
  ClientSocketPool& operator=(const ClientSocketPool&) = delete;

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
      const std::optional<NetworkTrafficAnnotationTag>& proxy_annotation_tag,
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
  // then this function doesn't do anything and returns OK.  Otherwise, it will
  // start up as many connections as necessary to reach |num_sockets| total
  // sockets for the group and returns ERR_IO_PENDING. And |callback| will be
  // called with OK when the connection tasks are finished.
  // It uses |params| to control how to connect the sockets. The
  // ClientSocketPool will assign a priority to the new connections, if any.
  // This priority will probably be lower than all others, since this method
  // is intended to make sure ahead of time that |num_sockets| sockets are
  // available to talk to a host.
  virtual int RequestSockets(
      const GroupId& group_id,
      scoped_refptr<SocketParams> params,
      const std::optional<NetworkTrafficAnnotationTag>& proxy_annotation_tag,
      int num_sockets,
      CompletionOnceCallback callback,
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

  // This flushes all state from the ClientSocketPool.  Pending socket requests
  // are failed with |error|, while |reason| is logged to the NetLog.
  //
  // Active sockets being held by ClientSocketPool clients will be discarded
  // when released back to the pool, though they will be closed with an error
  // about being of the wrong generation, rather than |net_log_reason_utf8|.
  virtual void FlushWithError(int error, const char* net_log_reason_utf8) = 0;

  // Called to close any idle connections held by the connection manager.
  // |reason| is logged to NetLog for debugging purposes.
  virtual void CloseIdleSockets(const char* net_log_reason_utf8) = 0;

  // Called to close any idle connections held by the connection manager.
  // |reason| is logged to NetLog for debugging purposes.
  virtual void CloseIdleSocketsInGroup(const GroupId& group_id,
                                       const char* net_log_reason_utf8) = 0;

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

  // Returns whether a connected (idle or handed out) or connecting socket
  // exists for the group. This method is not supported for WebSockets.
  virtual bool HasActiveSocket(const GroupId& group_id) const = 0;

  // Returns the maximum amount of time to wait before retrying a connect.
  static const int kMaxConnectRetryIntervalMs = 250;

  static base::TimeDelta used_idle_socket_timeout();
  static void set_used_idle_socket_timeout(base::TimeDelta timeout);

 protected:
  ClientSocketPool(bool is_for_websockets,
                   const CommonConnectJobParams* common_connect_job_params,
                   std::unique_ptr<ConnectJobFactory> connect_job_factory);

  void NetLogTcpClientSocketPoolRequestedSocket(const NetLogWithSource& net_log,
                                                const GroupId& group_id);

  // Utility method to log a GroupId with a NetLog event.
  static base::Value::Dict NetLogGroupIdParams(const GroupId& group_id);

  std::unique_ptr<ConnectJob> CreateConnectJob(
      GroupId group_id,
      scoped_refptr<SocketParams> socket_params,
      const ProxyChain& proxy_chain,
      const std::optional<NetworkTrafficAnnotationTag>& proxy_annotation_tag,
      RequestPriority request_priority,
      SocketTag socket_tag,
      ConnectJob::Delegate* delegate);

 private:
  const bool is_for_websockets_;
  const raw_ptr<const CommonConnectJobParams> common_connect_job_params_;
  const std::unique_ptr<ConnectJobFactory> connect_job_factory_;
};

}  // namespace net

#endif  // NET_SOCKET_CLIENT_SOCKET_POOL_H_
