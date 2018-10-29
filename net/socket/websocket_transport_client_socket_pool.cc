// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/websocket_transport_client_socket_pool.h"

#include <algorithm>

#include "base/callback_helpers.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "net/base/net_errors.h"
#include "net/base/trace_constants.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source.h"
#include "net/log/net_log_source_type.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/client_socket_pool_base.h"
#include "net/socket/websocket_endpoint_lock_manager.h"
#include "net/socket/websocket_transport_connect_sub_job.h"

namespace net {

WebSocketTransportConnectJob::WebSocketTransportConnectJob(
    const std::string& group_name,
    RequestPriority priority,
    ClientSocketPool::RespectLimits respect_limits,
    const scoped_refptr<TransportSocketParams>& params,
    base::TimeDelta timeout_duration,
    CompletionOnceCallback callback,
    ClientSocketFactory* client_socket_factory,
    HostResolver* host_resolver,
    ClientSocketHandle* handle,
    Delegate* delegate,
    WebSocketEndpointLockManager* websocket_endpoint_lock_manager,
    NetLog* pool_net_log,
    const NetLogWithSource& request_net_log)
    : ConnectJob(group_name,
                 timeout_duration,
                 priority,
                 SocketTag(),
                 respect_limits,
                 delegate,
                 NetLogWithSource::Make(
                     pool_net_log,
                     NetLogSourceType::WEB_SOCKET_TRANSPORT_CONNECT_JOB)),
      params_(params),
      resolver_(host_resolver),
      client_socket_factory_(client_socket_factory),
      next_state_(STATE_NONE),
      race_result_(TransportConnectJob::RACE_UNKNOWN),
      handle_(handle),
      websocket_endpoint_lock_manager_(websocket_endpoint_lock_manager),
      callback_(std::move(callback)),
      request_net_log_(request_net_log),
      had_ipv4_(false),
      had_ipv6_(false) {}

WebSocketTransportConnectJob::~WebSocketTransportConnectJob() = default;

LoadState WebSocketTransportConnectJob::GetLoadState() const {
  LoadState load_state = LOAD_STATE_RESOLVING_HOST;
  if (ipv6_job_)
    load_state = ipv6_job_->GetLoadState();
  // This method should return LOAD_STATE_CONNECTING in preference to
  // LOAD_STATE_WAITING_FOR_AVAILABLE_SOCKET when possible because "waiting for
  // available socket" implies that nothing is happening.
  if (ipv4_job_ && load_state != LOAD_STATE_CONNECTING)
    load_state = ipv4_job_->GetLoadState();
  return load_state;
}

void WebSocketTransportConnectJob::OnIOComplete(int result) {
  result = DoLoop(result);
  if (result != ERR_IO_PENDING)
    NotifyDelegateOfCompletion(result);  // Deletes |this|
}

int WebSocketTransportConnectJob::DoLoop(int result) {
  DCHECK_NE(next_state_, STATE_NONE);

  int rv = result;
  do {
    State state = next_state_;
    next_state_ = STATE_NONE;
    switch (state) {
      case STATE_RESOLVE_HOST:
        DCHECK_EQ(OK, rv);
        rv = DoResolveHost();
        break;
      case STATE_RESOLVE_HOST_COMPLETE:
        rv = DoResolveHostComplete(rv);
        break;
      case STATE_TRANSPORT_CONNECT:
        DCHECK_EQ(OK, rv);
        rv = DoTransportConnect();
        break;
      case STATE_TRANSPORT_CONNECT_COMPLETE:
        rv = DoTransportConnectComplete(rv);
        break;
      default:
        NOTREACHED();
        rv = ERR_FAILED;
        break;
    }
  } while (rv != ERR_IO_PENDING && next_state_ != STATE_NONE);

  return rv;
}

int WebSocketTransportConnectJob::DoResolveHost() {
  next_state_ = STATE_RESOLVE_HOST_COMPLETE;
  connect_timing_.dns_start = base::TimeTicks::Now();

  return resolver_->Resolve(
      params_->destination(), priority(), &addresses_,
      base::Bind(&WebSocketTransportConnectJob::OnIOComplete,
                 base::Unretained(this)),
      &request_, net_log());
}

int WebSocketTransportConnectJob::DoResolveHostComplete(int result) {
  TRACE_EVENT0(kNetTracingCategory,
               "WebSocketTransportConnectJob::DoResolveHostComplete");
  connect_timing_.dns_end = base::TimeTicks::Now();
  // Overwrite connection start time, since for connections that do not go
  // through proxies, |connect_start| should not include dns lookup time.
  connect_timing_.connect_start = connect_timing_.dns_end;

  if (result != OK)
    return result;

  // Invoke callback, and abort if it fails.
  if (!params_->host_resolution_callback().is_null()) {
    result = params_->host_resolution_callback().Run(addresses_, net_log());
    if (result != OK)
      return result;
  }

  next_state_ = STATE_TRANSPORT_CONNECT;
  return result;
}

int WebSocketTransportConnectJob::DoTransportConnect() {
  AddressList ipv4_addresses;
  AddressList ipv6_addresses;
  int result = ERR_UNEXPECTED;
  next_state_ = STATE_TRANSPORT_CONNECT_COMPLETE;

  for (AddressList::const_iterator it = addresses_.begin();
       it != addresses_.end(); ++it) {
    switch (it->GetFamily()) {
      case ADDRESS_FAMILY_IPV4:
        ipv4_addresses.push_back(*it);
        break;

      case ADDRESS_FAMILY_IPV6:
        ipv6_addresses.push_back(*it);
        break;

      default:
        DVLOG(1) << "Unexpected ADDRESS_FAMILY: " << it->GetFamily();
        break;
    }
  }

  if (!ipv4_addresses.empty()) {
    had_ipv4_ = true;
    ipv4_job_.reset(new WebSocketTransportConnectSubJob(
        ipv4_addresses, this, SUB_JOB_IPV4, websocket_endpoint_lock_manager_));
  }

  if (!ipv6_addresses.empty()) {
    had_ipv6_ = true;
    ipv6_job_.reset(new WebSocketTransportConnectSubJob(
        ipv6_addresses, this, SUB_JOB_IPV6, websocket_endpoint_lock_manager_));
    result = ipv6_job_->Start();
    switch (result) {
      case OK:
        SetSocket(ipv6_job_->PassSocket());
        race_result_ = had_ipv4_ ? TransportConnectJob::RACE_IPV6_WINS
                                 : TransportConnectJob::RACE_IPV6_SOLO;
        return result;

      case ERR_IO_PENDING:
        if (ipv4_job_) {
          // This use of base::Unretained is safe because |fallback_timer_| is
          // owned by this object.
          fallback_timer_.Start(
              FROM_HERE, base::TimeDelta::FromMilliseconds(
                             TransportConnectJob::kIPv6FallbackTimerInMs),
              base::Bind(&WebSocketTransportConnectJob::StartIPv4JobAsync,
                         base::Unretained(this)));
        }
        return result;

      default:
        ipv6_job_.reset();
    }
  }

  DCHECK(!ipv6_job_);
  if (ipv4_job_) {
    result = ipv4_job_->Start();
    if (result == OK) {
      SetSocket(ipv4_job_->PassSocket());
      race_result_ = had_ipv6_ ? TransportConnectJob::RACE_IPV4_WINS
                               : TransportConnectJob::RACE_IPV4_SOLO;
    }
  }

  return result;
}

int WebSocketTransportConnectJob::DoTransportConnectComplete(int result) {
  if (result == OK)
    TransportConnectJob::HistogramDuration(connect_timing_, race_result_);
  return result;
}

void WebSocketTransportConnectJob::OnSubJobComplete(
    int result,
    WebSocketTransportConnectSubJob* job) {
  if (result == OK) {
    switch (job->type()) {
      case SUB_JOB_IPV4:
        race_result_ = had_ipv6_ ? TransportConnectJob::RACE_IPV4_WINS
                                 : TransportConnectJob::RACE_IPV4_SOLO;
        break;

      case SUB_JOB_IPV6:
        race_result_ = had_ipv4_ ? TransportConnectJob::RACE_IPV6_WINS
                                 : TransportConnectJob::RACE_IPV6_SOLO;
        break;
    }
    SetSocket(job->PassSocket());

    // Make sure all connections are cancelled even if this object fails to be
    // deleted.
    ipv4_job_.reset();
    ipv6_job_.reset();
  } else {
    switch (job->type()) {
      case SUB_JOB_IPV4:
        ipv4_job_.reset();
        break;

      case SUB_JOB_IPV6:
        ipv6_job_.reset();
        if (ipv4_job_ && !ipv4_job_->started()) {
          fallback_timer_.Stop();
          result = ipv4_job_->Start();
          if (result != ERR_IO_PENDING) {
            OnSubJobComplete(result, ipv4_job_.get());
            return;
          }
        }
        break;
    }
    if (ipv4_job_ || ipv6_job_)
      return;
  }
  OnIOComplete(result);
}

void WebSocketTransportConnectJob::StartIPv4JobAsync() {
  DCHECK(ipv4_job_);
  int result = ipv4_job_->Start();
  if (result != ERR_IO_PENDING)
    OnSubJobComplete(result, ipv4_job_.get());
}

int WebSocketTransportConnectJob::ConnectInternal() {
  next_state_ = STATE_RESOLVE_HOST;
  return DoLoop(OK);
}

WebSocketTransportClientSocketPool::WebSocketTransportClientSocketPool(
    int max_sockets,
    int max_sockets_per_group,
    HostResolver* host_resolver,
    ClientSocketFactory* client_socket_factory,
    WebSocketEndpointLockManager* websocket_endpoint_lock_manager,
    NetLog* net_log)
    : TransportClientSocketPool(max_sockets,
                                max_sockets_per_group,
                                host_resolver,
                                client_socket_factory,
                                nullptr,
                                net_log),
      connect_job_delegate_(this),
      pool_net_log_(net_log),
      client_socket_factory_(client_socket_factory),
      host_resolver_(host_resolver),
      websocket_endpoint_lock_manager_(websocket_endpoint_lock_manager),
      max_sockets_(max_sockets),
      handed_out_socket_count_(0),
      flushing_(false),
      weak_factory_(this) {}

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
    const std::string& group_name,
    const void* params,
    RequestPriority priority,
    const SocketTag& socket_tag,
    RespectLimits respect_limits,
    ClientSocketHandle* handle,
    CompletionOnceCallback callback,
    const NetLogWithSource& request_net_log) {
  DCHECK(params);
  const scoped_refptr<TransportSocketParams>& casted_params =
      *static_cast<const scoped_refptr<TransportSocketParams>*>(params);

  NetLogTcpClientSocketPoolRequestedSocket(request_net_log, &casted_params);

  CHECK(!callback.is_null());
  CHECK(handle);

  request_net_log.BeginEvent(NetLogEventType::SOCKET_POOL);

  DCHECK(socket_tag == SocketTag());

  if (ReachedMaxSocketsLimit() &&
      respect_limits == ClientSocketPool::RespectLimits::ENABLED) {
    request_net_log.AddEvent(NetLogEventType::SOCKET_POOL_STALLED_MAX_SOCKETS);
    stalled_request_queue_.emplace_back(casted_params, priority, handle,
                                        std::move(callback), request_net_log);
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

  std::unique_ptr<WebSocketTransportConnectJob> connect_job(
      new WebSocketTransportConnectJob(
          group_name, priority, respect_limits, casted_params,
          ConnectionTimeout(), std::move(callback), client_socket_factory_,
          host_resolver_, handle, &connect_job_delegate_,
          websocket_endpoint_lock_manager_, pool_net_log_, request_net_log));

  int result = connect_job->Connect();

  // Regardless of the outcome of |connect_job|, it will always be bound to
  // |handle|, since this pool uses early-binding. So the binding is logged
  // here, without waiting for the result.
  request_net_log.AddEvent(
      NetLogEventType::SOCKET_POOL_BOUND_TO_CONNECT_JOB,
      connect_job->net_log().source().ToEventParametersCallback());

  if (result == ERR_IO_PENDING) {
    // TODO(ricea): Implement backup job timer?
    AddJob(handle, std::move(connect_job));
  } else {
    TryHandOutSocket(result, connect_job.get());
  }

  return result;
}

void WebSocketTransportClientSocketPool::RequestSockets(
    const std::string& group_name,
    const void* params,
    int num_sockets,
    const NetLogWithSource& net_log) {
  NOTIMPLEMENTED();
}

void WebSocketTransportClientSocketPool::SetPriority(
    const std::string& group_name,
    ClientSocketHandle* handle,
    RequestPriority priority) {
  // Since sockets requested by RequestSocket are bound early and
  // stalled_request_{queue,map} don't take priorities into account, there's
  // nothing to do within the pool to change priority or the request.
  // TODO(rdsmith, ricea): Make stalled_request_{queue,map} take priorities
  // into account.
  // TODO(rdsmith): Investigate plumbing the reprioritization request to the
  // connect job.
}

void WebSocketTransportClientSocketPool::CancelRequest(
    const std::string& group_name,
    ClientSocketHandle* handle) {
  DCHECK(!handle->is_initialized());
  if (DeleteStalledRequest(handle))
    return;
  std::unique_ptr<StreamSocket> socket = handle->PassSocket();
  if (socket)
    ReleaseSocket(handle->group_name(), std::move(socket), handle->id());
  if (!DeleteJob(handle))
    pending_callbacks_.erase(handle);

  ActivateStalledRequest();
}

void WebSocketTransportClientSocketPool::ReleaseSocket(
    const std::string& group_name,
    std::unique_ptr<StreamSocket> socket,
    int id) {
  websocket_endpoint_lock_manager_->UnlockSocket(socket.get());
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
    InvokeUserCallbackLater(it->second->handle(),
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
    const std::string& group_name) {
  // We have no idle sockets.
}

int WebSocketTransportClientSocketPool::IdleSocketCount() const {
  return 0;
}

int WebSocketTransportClientSocketPool::IdleSocketCountInGroup(
    const std::string& group_name) const {
  return 0;
}

LoadState WebSocketTransportClientSocketPool::GetLoadState(
    const std::string& group_name,
    const ClientSocketHandle* handle) const {
  if (stalled_request_map_.find(handle) != stalled_request_map_.end())
    return LOAD_STATE_WAITING_FOR_AVAILABLE_SOCKET;
  if (pending_callbacks_.count(handle))
    return LOAD_STATE_CONNECTING;
  return LookupConnectJob(handle)->GetLoadState();
}

std::unique_ptr<base::DictionaryValue>
WebSocketTransportClientSocketPool::GetInfoAsValue(
    const std::string& name,
    const std::string& type,
    bool include_nested_pools) const {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  dict->SetString("name", name);
  dict->SetString("type", type);
  dict->SetInteger("handed_out_socket_count", handed_out_socket_count_);
  dict->SetInteger("connecting_socket_count", pending_connects_.size());
  dict->SetInteger("idle_socket_count", 0);
  dict->SetInteger("max_socket_count", max_sockets_);
  dict->SetInteger("max_sockets_per_group", max_sockets_);
  dict->SetInteger("pool_generation_number", 0);
  return dict;
}

base::TimeDelta WebSocketTransportClientSocketPool::ConnectionTimeout() const {
  // TODO(ricea): For now, we implement a global timeout for compatibility with
  // TransportConnectJob. Since WebSocketTransportConnectJob controls the
  // address selection process more tightly, it could do something smarter here.
  return base::TimeDelta::FromSeconds(TransportConnectJob::kTimeoutInSeconds);
}

bool WebSocketTransportClientSocketPool::IsStalled() const {
  return !stalled_request_queue_.empty();
}

bool WebSocketTransportClientSocketPool::TryHandOutSocket(
    int result,
    WebSocketTransportConnectJob* job) {
  DCHECK_NE(result, ERR_IO_PENDING);

  std::unique_ptr<StreamSocket> socket = job->PassSocket();
  ClientSocketHandle* const handle = job->handle();
  NetLogWithSource request_net_log = job->request_net_log();
  LoadTimingInfo::ConnectTiming connect_timing = job->connect_timing();

  if (result == OK) {
    DCHECK(socket);

    HandOutSocket(std::move(socket), connect_timing, handle, request_net_log);

    request_net_log.EndEvent(NetLogEventType::SOCKET_POOL);

    return true;
  }

  bool handed_out_socket = false;

  // If we got a socket, it must contain error information so pass that
  // up so that the caller can retrieve it.
  job->GetAdditionalErrorState(handle);
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
    WebSocketTransportConnectJob* job) {
  DCHECK_NE(ERR_IO_PENDING, result);

  // See comment in FlushWithError.
  if (flushing_) {
    std::unique_ptr<StreamSocket> socket = job->PassSocket();
    websocket_endpoint_lock_manager_->UnlockSocket(socket.get());
    return;
  }

  bool handed_out_socket = TryHandOutSocket(result, job);

  CompletionOnceCallback callback = job->release_callback();

  ClientSocketHandle* const handle = job->handle();

  bool delete_succeeded = DeleteJob(handle);
  DCHECK(delete_succeeded);

  job = nullptr;

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
  handle->set_pool_id(0);
  handle->set_connect_timing(connect_timing);

  net_log.AddEvent(
      NetLogEventType::SOCKET_POOL_BOUND_TO_SOCKET,
      handle->socket()->NetLog().source().ToEventParametersCallback());

  ++handed_out_socket_count_;
}

void WebSocketTransportClientSocketPool::AddJob(
    ClientSocketHandle* handle,
    std::unique_ptr<WebSocketTransportConnectJob> connect_job) {
  bool inserted = pending_connects_
                      .insert(PendingConnectsMap::value_type(
                          handle, std::move(connect_job)))
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

const WebSocketTransportConnectJob*
WebSocketTransportClientSocketPool::LookupConnectJob(
    const ClientSocketHandle* handle) const {
  auto it = pending_connects_.find(handle);
  CHECK(it != pending_connects_.end());
  return it->second.get();
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

    int rv =
        RequestSocket("ignored", &request.params, request.priority, SocketTag(),
                      // Stalled requests can't have |respect_limits|
                      // DISABLED.
                      RespectLimits::ENABLED, request.handle, copyable_callback,
                      request.net_log);

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
    WebSocketTransportClientSocketPool* owner)
    : owner_(owner) {}

WebSocketTransportClientSocketPool::ConnectJobDelegate::~ConnectJobDelegate() =
    default;

void
WebSocketTransportClientSocketPool::ConnectJobDelegate::OnConnectJobComplete(
    int result,
    ConnectJob* job) {
  owner_->OnConnectJobComplete(result,
                               static_cast<WebSocketTransportConnectJob*>(job));
}

WebSocketTransportClientSocketPool::StalledRequest::StalledRequest(
    const scoped_refptr<TransportSocketParams>& params,
    RequestPriority priority,
    ClientSocketHandle* handle,
    CompletionOnceCallback callback,
    const NetLogWithSource& net_log)
    : params(params),
      priority(priority),
      handle(handle),
      callback(std::move(callback)),
      net_log(net_log) {}

WebSocketTransportClientSocketPool::StalledRequest::StalledRequest(
    StalledRequest&& other) = default;

WebSocketTransportClientSocketPool::StalledRequest::~StalledRequest() = default;

}  // namespace net
