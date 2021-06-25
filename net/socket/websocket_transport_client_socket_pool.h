// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_WEBSOCKET_TRANSPORT_CLIENT_SOCKET_POOL_H_
#define NET_SOCKET_WEBSOCKET_TRANSPORT_CLIENT_SOCKET_POOL_H_

#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "net/base/net_export.h"
#include "net/base/proxy_server.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/client_socket_pool.h"
#include "net/socket/connect_job.h"
#include "net/socket/ssl_client_socket.h"

namespace base {
namespace trace_event {
class ProcessMemoryDump;
}
}  // namespace base

namespace net {

struct CommonConnectJobParams;
struct NetworkTrafficAnnotationTag;
class WebSocketTransportConnectJob;

class NET_EXPORT_PRIVATE WebSocketTransportClientSocketPool
    : public ClientSocketPool {
 public:
  WebSocketTransportClientSocketPool(
      int max_sockets,
      int max_sockets_per_group,
      const ProxyServer& proxy_server,
      const CommonConnectJobParams* common_connect_job_params);

  ~WebSocketTransportClientSocketPool() override;

  // Allow another connection to be started to the IPEndPoint that this |handle|
  // is connected to. Used when the WebSocket handshake completes successfully.
  // This only works if the socket is connected, however the caller does not
  // need to explicitly check for this. Instead, ensure that dead sockets are
  // returned to ReleaseSocket() in a timely fashion.
  static void UnlockEndpoint(
      ClientSocketHandle* handle,
      WebSocketEndpointLockManager* websocket_endpoint_lock_manager);

  // ClientSocketPool implementation.
  int RequestSocket(
      const GroupId& group_id,
      scoped_refptr<SocketParams> params,
      const base::Optional<NetworkTrafficAnnotationTag>& proxy_annotation_tag,
      RequestPriority priority,
      const SocketTag& socket_tag,
      RespectLimits respect_limits,
      ClientSocketHandle* handle,
      CompletionOnceCallback callback,
      const ProxyAuthCallback& proxy_auth_callback,
      const NetLogWithSource& net_log) override;
  void RequestSockets(
      const GroupId& group_id,
      scoped_refptr<SocketParams> params,
      const base::Optional<NetworkTrafficAnnotationTag>& proxy_annotation_tag,
      int num_sockets,
      const NetLogWithSource& net_log) override;
  void SetPriority(const GroupId& group_id,
                   ClientSocketHandle* handle,
                   RequestPriority priority) override;
  void CancelRequest(const GroupId& group_id,
                     ClientSocketHandle* handle,
                     bool cancel_connect_job) override;
  void ReleaseSocket(const GroupId& group_id,
                     std::unique_ptr<StreamSocket> socket,
                     int64_t generation) override;
  void FlushWithError(int error, const char* net_log_reason_utf8) override;
  void CloseIdleSockets(const char* net_log_reason_utf8) override;
  void CloseIdleSocketsInGroup(const GroupId& group_id,
                               const char* net_log_reason_utf8) override;
  int IdleSocketCount() const override;
  size_t IdleSocketCountInGroup(const GroupId& group_id) const override;
  LoadState GetLoadState(const GroupId& group_id,
                         const ClientSocketHandle* handle) const override;
  base::Value GetInfoAsValue(const std::string& name,
                             const std::string& type) const override;
  void DumpMemoryStats(
      base::trace_event::ProcessMemoryDump* pmd,
      const std::string& parent_dump_absolute_name) const override;

  // HigherLayeredPool implementation.
  bool IsStalled() const override;
  void AddHigherLayeredPool(HigherLayeredPool* higher_pool) override;
  void RemoveHigherLayeredPool(HigherLayeredPool* higher_pool) override;

 private:
  class ConnectJobDelegate : public ConnectJob::Delegate {
   public:
    ConnectJobDelegate(WebSocketTransportClientSocketPool* owner,
                       CompletionOnceCallback callback,
                       ClientSocketHandle* socket_handle,
                       const NetLogWithSource& request_net_log);
    ~ConnectJobDelegate() override;

    // ConnectJob::Delegate implementation
    void OnConnectJobComplete(int result, ConnectJob* job) override;
    void OnNeedsProxyAuth(const HttpResponseInfo& response,
                          HttpAuthController* auth_controller,
                          base::OnceClosure restart_with_auth_callback,
                          ConnectJob* job) override;

    // Calls Connect() on |connect_job|, and takes ownership. Returns Connect's
    // return value.
    int Connect(std::unique_ptr<ConnectJob> connect_job);

    CompletionOnceCallback release_callback() { return std::move(callback_); }
    ConnectJob* connect_job() { return connect_job_.get(); }
    ClientSocketHandle* socket_handle() { return socket_handle_; }

    const NetLogWithSource& request_net_log() { return request_net_log_; }
    const NetLogWithSource& connect_job_net_log();

   private:
    WebSocketTransportClientSocketPool* owner_;

    CompletionOnceCallback callback_;
    std::unique_ptr<ConnectJob> connect_job_;
    ClientSocketHandle* const socket_handle_;
    const NetLogWithSource request_net_log_;

    DISALLOW_COPY_AND_ASSIGN(ConnectJobDelegate);
  };

  // Store the arguments from a call to RequestSocket() that has stalled so we
  // can replay it when there are available socket slots.
  struct StalledRequest {
    StalledRequest(
        const GroupId& group_id,
        const scoped_refptr<SocketParams>& params,
        const base::Optional<NetworkTrafficAnnotationTag>& proxy_annotation_tag,
        RequestPriority priority,
        ClientSocketHandle* handle,
        CompletionOnceCallback callback,
        const ProxyAuthCallback& proxy_auth_callback,
        const NetLogWithSource& net_log);
    StalledRequest(StalledRequest&& other);
    ~StalledRequest();

    const GroupId group_id;
    const scoped_refptr<SocketParams> params;
    const base::Optional<NetworkTrafficAnnotationTag> proxy_annotation_tag;
    const RequestPriority priority;
    ClientSocketHandle* const handle;
    CompletionOnceCallback callback;
    ProxyAuthCallback proxy_auth_callback;
    const NetLogWithSource net_log;
  };

  typedef std::map<const ClientSocketHandle*,
                   std::unique_ptr<ConnectJobDelegate>>
      PendingConnectsMap;
  // This is a list so that we can remove requests from the middle, and also
  // so that iterators are not invalidated unless the corresponding request is
  // removed.
  typedef std::list<StalledRequest> StalledRequestQueue;
  typedef std::map<const ClientSocketHandle*, StalledRequestQueue::iterator>
      StalledRequestMap;

  // Tries to hand out the socket connected by |job|. |result| must be (async)
  // result of WebSocketTransportConnectJob::Connect(). Returns true iff it has
  // handed out a socket.
  bool TryHandOutSocket(int result, ConnectJobDelegate* connect_job_delegate);
  void OnConnectJobComplete(int result,
                            ConnectJobDelegate* connect_job_delegate);
  void InvokeUserCallbackLater(ClientSocketHandle* handle,
                               CompletionOnceCallback callback,
                               int rv);
  void InvokeUserCallback(ClientSocketHandle* handle,
                          CompletionOnceCallback callback,
                          int rv);
  bool ReachedMaxSocketsLimit() const;
  void HandOutSocket(std::unique_ptr<StreamSocket> socket,
                     const LoadTimingInfo::ConnectTiming& connect_timing,
                     ClientSocketHandle* handle,
                     const NetLogWithSource& net_log);
  void AddJob(ClientSocketHandle* handle,
              std::unique_ptr<ConnectJobDelegate> delegate);
  bool DeleteJob(ClientSocketHandle* handle);
  const ConnectJob* LookupConnectJob(const ClientSocketHandle* handle) const;
  void ActivateStalledRequest();
  bool DeleteStalledRequest(ClientSocketHandle* handle);

  const ProxyServer proxy_server_;
  const CommonConnectJobParams* const common_connect_job_params_;
  std::set<const ClientSocketHandle*> pending_callbacks_;
  PendingConnectsMap pending_connects_;
  StalledRequestQueue stalled_request_queue_;
  StalledRequestMap stalled_request_map_;
  const int max_sockets_;
  int handed_out_socket_count_;
  bool flushing_;

  base::WeakPtrFactory<WebSocketTransportClientSocketPool> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WebSocketTransportClientSocketPool);
};

}  // namespace net

#endif  // NET_SOCKET_WEBSOCKET_TRANSPORT_CLIENT_SOCKET_POOL_H_
