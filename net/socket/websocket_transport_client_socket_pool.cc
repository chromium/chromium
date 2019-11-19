// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/websocket_transport_client_socket_pool.h"

#include <algorithm>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source.h"
#include "net/log/net_log_source_type.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/connect_job.h"
#include "net/socket/websocket_endpoint_lock_manager.h"
#include "net/socket/websocket_transport_connect_job.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace net {

WebSocketTransportClientSocketPool::WebSocketTransportClientSocketPool(
    int max_sockets,
    int max_sockets_per_group,
    const ProxyServer& proxy_server,
    const CommonConnectJobParams* common_connect_job_params)
    : proxy_server_(proxy_server),
      common_connect_job_params_(common_connect_job_params),
      max_sockets_(max_sockets),
      handed_out_socket_count_(0),
      flushing_(false) {
  DCHECK(common_connect_job_params_->websocket_endpoint_lock_manager);
}

WebSocketTransportClientSocketPool::~WebSocketTransportClientSocketPool() {
  // Clean up any pending connect jobs.
  FlushWithError(ERR_ABORTED);
  DCHECK(pending_connects_.empty());
  DCHECK_EQ(0, handed_out_socket_count_);
  DCHECK(stalled_request_queue_.empty());
  DCHECK(stalled_request_map_.empty());
}

// static
void WebSocketTransportClientSocketPool::UnlockEndpoint(
    ClientSocketHandle* handle,
    WebSocketEndpointLockManager* websocket_endpoint_lock_manager) {
  DCHECK(handle->is_initialized());
  DCHECK(handle->socket());
  IPEndPoint address;
  if (handle->socket()->GetPeerAddress(&address) == OK)
    websocket_endpoint_lock_manager->UnlockEndpoint(address);
}

int WebSocketTransportClientSocketPool::RequestSocket(
    const GroupId& group_id,
    scoped_refptr<SocketParams> params,
    const base::Optional<NetworkTrafficAnnotationTag>& proxy_annotation_tag,
    RequestPriority priority,
    const SocketTag& socket_tag,
    RespectLimits respect_limits,
    ClientSocketHandle* handle,
    CompletionOnceCallback callback,
    const ProxyAuthCallback& proxy_auth_callback,
    const NetLogWithSource& request_net_log) {
  DCHECK(params);
  CHECK(!callback.is_null());
  CHECK(handle);
  DCHECK(socket_tag == SocketTag());

  NetLogTcpClientSocketPoolRequestedSocket(request_net_log, group_id);
  request_net_log.BeginEvent(NetLogEventType::SOCKET_POOL);

  if (ReachedMaxSocketsLimit() &&
      respect_limits == ClientSocketPool::RespectLimits::ENABLED) {
    request_net_log.AddEvent(NetLogEventType::SOCKET_POOL_STALLED_MAX_SOCKETS);
    stalled_request_queue_.emplace_back(group_id, params, proxy_annotation_tag,
                                        priority, handle, std::move(callback),
                                        proxy_auth_callback, request_net_log);
    auto iterator = stalled_request_queue_.end();
    --iterator;
    DCHECK_EQ(handle, iterator->handle);
    // Because StalledRequestQueue is a std::list, its iterators are guaranteed
    // to remain valid as long as the elements are not removed. As long as
    // stalled_request_queue_ and stalled_request_map_ are updated in sync, it
    // is safe to dereference an iterator in stalled_request_map_ to find the
    // corresponding list element.
    stalled_request_map_.insert(
        StalledRequestMap::value_type(handle, iterator));
    return ERR_IO_PENDING;
  }

  std::unique_ptr<ConnectJobDelegate> connect_job_delegate =
      std::make_unique<ConnectJobDelegate>(this, std::move(callback), handle,
                                           request_net_log);

  std::unique_ptr<ConnectJob> connect_job =
      CreateConnectJob(group_id, params, proxy_server_, proxy_annotation_tag,
                       true /* is_for_websockets */, common_connect_job_params_,
                       priority, SocketTag(), connect_job_delegate.get());

  int result = connect_job_delegate->Connect(std::move(connect_job));

  // Regardless of the outcome of |connect_job|, it will always be bound to
  // |handle|, since this pool uses early-binding. So the binding is logged
  // here, without waiting for the result.
  request_net_log.AddEventReferencingSource(
      NetLogEventType::SOCKET_POOL_BOUND_TO_CONNECT_JOB,
      connect_job_delegate->connect_job_net_log().source());

  if (result == ERR_IO_PENDING) {
    // TODO(ricea): Implement backup job timer?
    AddJob(handle, std::move(connect_job_delegate));
  } else {
    TryHandOutSocket(result, connect_job_delegate.get());
  }

  return result;
}

void WebSocketTransportClientSocketPool::RequestSockets(
    const GroupId& group_id,
    scoped_refptr<SocketParams> params,
    const base::Optional<NetworkTrafficAnnotationTag>& proxy_annotation_tag,
    int num_sockets,
    const NetLogWithSource& net_log) {
  NOTIMPLEMENTED();
}

void WebSocketTransportClientSocketPool::SetPriority(const GroupId& group_id,
                                                     ClientSocketHandle* handle,
                                                     RequestPriority priority) {
  // Since sockets requested by RequestSocket are bound early and
  // stalled_request_{queue,map} don't take priorities into account, there's
  // nothing to do within the pool to change priority of the request.
  // TODO(rdsmith, ricea): Make stalled_request_{queue,map} take priorities
  // into account.
  // TODO(rdsmith, chlily): Investigate plumbing the reprioritization request to
  // the connect job.
}

void WebSocketTransportClientSocketPool::CancelRequest(
    const GroupId& group_id,
    ClientSocketHandle* handle,
    bool cancel_connect_job) {
  DCHECK(!handle->is_initialized());
  if (DeleteStalledRequest(handle))
    return;
  std::unique_ptr<StreamSocket> socket = handle->PassSocket();
  if (socket)
    ReleaseSocket(handle->group_id(), std::move(socket),
                  handle->group_generation());
  if (!DeleteJob(handle))
    pending_callbacks_.erase(handle);

  ActivateStalledRequest();
}

void WebSocketTransportClientSocketPool::ReleaseSocket(
    const GroupId& group_id,
    std::unique_ptr<StreamSocket> socket,
    int64_t generation) {
  CHECK_GT(handed_out_socket_count_, 0);
  --handed_out_socket_count_;

  ActivateStalledRequest();
}

void WebSocketTransportClientSocketPool::FlushWithError(int error) {
  DCHECK_NE(error, OK);

  // Sockets which are in LOAD_STATE_CONNECTING are in danger of unlocking
  // sockets waiting for the endpoint lock. If they connected synchronously,
  // then OnConnectJobComplete(). The |flushing_| flag tells this object to
  // ignore spurious calls to OnConnectJobComplete(). It is safe to ignore those
  // calls because this method will delete the jobs and call their callbacks
  // anyway.
  flushing_ = true;
  for (auto it = pending_connects_.begin(); it != pending_connects_.end();) {
    InvokeUserCallbackLater(it->second->socket_handle(),
                            it->second->release_callback(), error);
    it = pending_connects_.erase(it);
  }
  for (auto it = stalled_request_queue_.begin();
       it != stalled_request_queue_.end(); ++it) {
    InvokeUserCallbackLater(it->handle, std::move(it->callback), error);
  }
  stalled_request_map_.clear();
  stalled_request_queue_.clear();
  flushing_ = false;
}

void WebSocketTransportClientSocketPool::CloseIdleSockets() {
  // We have no idle sockets.
}

void WebSocketTransportClientSocketPool::CloseIdleSocketsInGroup(
    const GroupId& group_id) {
  // We have no idle sockets.
}

int WebSocketTransportClientSocketPool::IdleSocketCount() const {
  return 0;
}

size_t WebSocketTransportClientSocketPool::IdleSocketCountInGroup(
    const GroupId& group_id) const {
  return 0;
}

LoadState WebSocketTransportClientSocketPool::GetLoadState(
    const GroupId& group_id,
    const ClientSocketHandle* handle) const {
  if (stalled_request_map_.find(handle) != stalled_request_map_.end())
    return LOAD_STATE_WAITING_FOR_AVAILABLE_SOCKET;
  if (pending_callbacks_.count(handle))
    return LOAD_STATE_CONNECTING;
  return LookupConnectJob(handle)->GetLoadState();
}

base::Value WebSocketTransportClientSocketPool::GetInfoAsValue(
    const std::string& name,
    const std::string& type) const {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey("name", name);
  dict.SetStringKey("type", type);
  dict.SetIntKey("handed_out_socket_count", handed_out_socket_count_);
  dict.SetIntKey("connecting_socket_count", pending_connects_.size());
  dict.SetIntKey("idle_socket_count", 0);
  dict.SetIntKey("max_socket_count", max_sockets_);
  dict.SetIntKey("max_sockets_per_group", max_sockets_);
  return dict;
}

void WebSocketTransportClientSocketPool::DumpMemoryStats(
    base::trace_event::ProcessMemoryDump* pmd,
    const std::string& parent_dump_absolute_name) const {
  // Not supported.
}

bool WebSocketTransportClientSocketPool::IsStalled() const {
  return !stalled_request_queue_.empty();
}

void WebSocketTransportClientSocketPool::AddHigherLayeredPool(
    HigherLayeredPool* higher_pool) {
  // This class doesn't use connection limits like the pools for HTTP do, so no
  // need to track higher layered pools.
}

void WebSocketTransportClientSocketPool::RemoveHigherLayeredPool(
    HigherLayeredPool* higher_pool) {
  // This class doesn't use connection limits like the pools for HTTP do, so no
  // need to track higher layered pools.
}

bool WebSocketTransportClientSocketPool::TryHandOutSocket(
    int result,
    ConnectJobDelegate* connect_job_delegate) {
  DCHECK_NE(result, ERR_IO_PENDING);

  std::unique_ptr<StreamSocket> socket =
      connect_job_delegate->connect_job()->PassSocket();
  LoadTimingInfo::ConnectTiming connect_timing =
      connect_job_delegate->connect_job()->connect_timing();
  ClientSocketHandle* const handle = connect_job_delegate->socket_handle();
  NetLogWithSource request_net_log = connect_job_delegate->request_net_log();

  if (result == OK) {
    DCHECK(socket);

    HandOutSocket(std::move(socket), connect_timing, handle, request_net_log);

    request_net_log.EndEvent(NetLogEventType::SOCKET_POOL);

    return true;
  }

  bool handed_out_socket = false;

  // If we got a socket, it must contain error information so pass that
  // up so that the caller can retrieve it.
  handle->SetAdditionalErrorState(connect_job_delegate->connect_job());
  if (socket) {
    HandOutSocket(std::move(socket), connect_timing, handle, request_net_log);
    handed_out_socket = true;
  }

  request_net_log.EndEventWithNetErrorCode(NetLogEventType::SOCKET_POOL,
                                           result);

  return handed_out_socket;
}

void WebSocketTransportClientSocketPool::OnConnectJobComplete(
    int result,
    ConnectJobDelegate* connect_job_delegate) {
  DCHECK_NE(ERR_IO_PENDING, result);

  // See comment in FlushWithError.
  if (flushing_) {
    // Just delete the socket.
    std::unique_ptr<StreamSocket> socket =
        connect_job_delegate->connect_job()->PassSocket();
    return;
  }

  bool handed_out_socket = TryHandOutSocket(result, connect_job_delegate);

  CompletionOnceCallback callback = connect_job_delegate->release_callback();

  ClientSocketHandle* const handle = connect_job_delegate->socket_handle();

  bool delete_succeeded = DeleteJob(handle);
  DCHECK(delete_succeeded);

  connect_job_delegate = nullptr;

  if (!handed_out_socket)
    ActivateStalledRequest();

  InvokeUserCallbackLater(handle, std::move(callback), result);
}

void WebSocketTransportClientSocketPool::InvokeUserCallbackLater(
    ClientSocketHandle* handle,
    CompletionOnceCallback callback,
    int rv) {
  DCHECK(!pending_callbacks_.count(handle));
  pending_callbacks_.insert(handle);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&WebSocketTransportClientSocketPool::InvokeUserCallback,
                     weak_factory_.GetWeakPtr(), handle, std::move(callback),
                     rv));
}

void WebSocketTransportClientSocketPool::InvokeUserCallback(
    ClientSocketHandle* handle,
    CompletionOnceCallback callback,
    int rv) {
  if (pending_callbacks_.erase(handle))
    std::move(callback).Run(rv);
}

bool WebSocketTransportClientSocketPool::ReachedMaxSocketsLimit() const {
  return handed_out_socket_count_ >= max_sockets_ ||
         base::checked_cast<int>(pending_connects_.size()) >=
             max_sockets_ - handed_out_socket_count_;
}

void WebSocketTransportClientSocketPool::HandOutSocket(
    std::unique_ptr<StreamSocket> socket,
    const LoadTimingInfo::ConnectTiming& connect_timing,
    ClientSocketHandle* handle,
    const NetLogWithSource& net_log) {
  DCHECK(socket);
  DCHECK_EQ(ClientSocketHandle::UNUSED, handle->reuse_type());
  DCHECK_EQ(0, handle->idle_time().InMicroseconds());

  handle->SetSocket(std::move(socket));
  handle->set_group_generation(0);
  handle->set_connect_timing(connect_timing);

  net_log.AddEventReferencingSource(
      NetLogEventType::SOCKET_POOL_BOUND_TO_SOCKET,
      handle->socket()->NetLog().source());

  ++handed_out_socket_count_;
}

void WebSocketTransportClientSocketPool::AddJob(
    ClientSocketHandle* handle,
    std::unique_ptr<ConnectJobDelegate> delegate) {
  bool inserted =
      pending_connects_
          .insert(PendingConnectsMap::value_type(handle, std::move(delegate)))
          .second;
  DCHECK(inserted);
}

bool WebSocketTransportClientSocketPool::DeleteJob(ClientSocketHandle* handle) {
  auto it = pending_connects_.find(handle);
  if (it == pending_connects_.end())
    return false;
  // Deleting a ConnectJob which holds an endpoint lock can lead to a different
  // ConnectJob proceeding to connect. If the connect proceeds synchronously
  // (usually because of a failure) then it can trigger that job to be
  // deleted.
  pending_connects_.erase(it);
  return true;
}

const ConnectJob* WebSocketTransportClientSocketPool::LookupConnectJob(
    const ClientSocketHandle* handle) const {
  auto it = pending_connects_.find(handle);
  CHECK(it != pending_connects_.end());
  return it->second->connect_job();
}

void WebSocketTransportClientSocketPool::ActivateStalledRequest() {
  // Usually we will only be able to activate one stalled request at a time,
  // however if all the connects fail synchronously for some reason, we may be
  // able to clear the whole queue at once.
  while (!stalled_request_queue_.empty() && !ReachedMaxSocketsLimit()) {
    StalledRequest request = std::move(stalled_request_queue_.front());
    stalled_request_queue_.pop_front();
    stalled_request_map_.erase(request.handle);

    // Wrap request.callback into a copyable (repeating) callback so that it can
    // be passed to RequestSocket() and yet called if RequestSocket() returns
    // synchronously.
    auto copyable_callback =
        base::AdaptCallbackForRepeating(std::move(request.callback));

    int rv = RequestSocket(
        request.group_id, request.params, request.proxy_annotation_tag,
        request.priority, SocketTag(),
        // Stalled requests can't have |respect_limits|
        // DISABLED.
        RespectLimits::ENABLED, request.handle, copyable_callback,
        request.proxy_auth_callback, request.net_log);

    // ActivateStalledRequest() never returns synchronously, so it is never
    // called re-entrantly.
    if (rv != ERR_IO_PENDING)
      InvokeUserCallbackLater(request.handle, copyable_callback, rv);
  }
}

bool WebSocketTransportClientSocketPool::DeleteStalledRequest(
    ClientSocketHandle* handle) {
  auto it = stalled_request_map_.find(handle);
  if (it == stalled_request_map_.end())
    return false;
  stalled_request_queue_.erase(it->second);
  stalled_request_map_.erase(it);
  return true;
}

WebSocketTransportClientSocketPool::ConnectJobDelegate::ConnectJobDelegate(
    WebSocketTransportClientSocketPool* owner,
    CompletionOnceCallback callback,
    ClientSocketHandle* socket_handle,
    const NetLogWithSource& request_net_log)
    : owner_(owner),
      callback_(std::move(callback)),
      socket_handle_(socket_handle),
      request_net_log_(request_net_log) {}

WebSocketTransportClientSocketPool::ConnectJobDelegate::~ConnectJobDelegate() =
    default;

void
WebSocketTransportClientSocketPool::ConnectJobDelegate::OnConnectJobComplete(
    int result,
    ConnectJob* job) {
  DCHECK_EQ(job, connect_job_.get());
  owner_->OnConnectJobComplete(result, this);
}

void WebSocketTransportClientSocketPool::ConnectJobDelegate::OnNeedsProxyAuth(
    const HttpResponseInfo& response,
    HttpAuthController* auth_controller,
    base::OnceClosure restart_with_auth_callback,
    ConnectJob* job) {
  // This class isn't used for proxies.
  NOTREACHED();
}

int WebSocketTransportClientSocketPool::ConnectJobDelegate::Connect(
    std::unique_ptr<ConnectJob> connect_job) {
  connect_job_ = std::move(connect_job);
  return connect_job_->Connect();
}

const NetLogWithSource&
WebSocketTransportClientSocketPool::ConnectJobDelegate::connect_job_net_log() {
  return connect_job_->net_log();
}

WebSocketTransportClientSocketPool::StalledRequest::StalledRequest(
    const GroupId& group_id,
    const scoped_refptr<SocketParams>& params,
    const base::Optional<NetworkTrafficAnnotationTag>& proxy_annotation_tag,
    RequestPriority priority,
    ClientSocketHandle* handle,
    CompletionOnceCallback callback,
    const ProxyAuthCallback& proxy_auth_callback,
    const NetLogWithSource& net_log)
    : group_id(group_id),
      params(params),
      proxy_annotation_tag(proxy_annotation_tag),
      priority(priority),
      handle(handle),
      callback(std::move(callback)),
      proxy_auth_callback(proxy_auth_callback),
      net_log(net_log) {}

WebSocketTransportClientSocketPool::StalledRequest::StalledRequest(
    StalledRequest&& other) = default;

WebSocketTransportClientSocketPool::StalledRequest::~StalledRequest() = default;

}  // namespace net
