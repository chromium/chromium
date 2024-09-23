// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/transport_client_socket_pool.h"

#include <string_view>
#include <utility>

#include "base/auto_reset.h"
#include "base/barrier_closure.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/contains.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/not_fatal_until.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/values.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_server.h"
#include "net/log/net_log.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source.h"
#include "net/socket/connect_job_factory.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

namespace net {

namespace {

// Indicate whether or not we should establish a new transport layer connection
// after a certain timeout has passed without receiving an ACK.
bool g_connect_backup_jobs_enabled = true;

base::Value::Dict NetLogCreateConnectJobParams(
    bool backup_job,
    const ClientSocketPool::GroupId* group_id) {
  return base::Value::Dict()
      .Set("backup_job", backup_job)
      .Set("group_id", group_id->ToString());
}

}  // namespace

const char TransportClientSocketPool::kCertDatabaseChanged[] =
    "Cert database changed";
const char TransportClientSocketPool::kCertVerifierChanged[] =
    "Cert verifier changed";
const char TransportClientSocketPool::kClosedConnectionReturnedToPool[] =
    "Connection was closed when it was returned to the pool";
const char TransportClientSocketPool::kDataReceivedUnexpectedly[] =
    "Data received unexpectedly";
const char TransportClientSocketPool::kIdleTimeLimitExpired[] =
    "Idle time limit expired";
const char TransportClientSocketPool::kNetworkChanged[] = "Network changed";
const char TransportClientSocketPool::kRemoteSideClosedConnection[] =
    "Remote side closed connection";
const char TransportClientSocketPool::kSocketGenerationOutOfDate[] =
    "Socket generation out of date";
const char TransportClientSocketPool::kSocketPoolDestroyed[] =
    "Socket pool destroyed";
const char TransportClientSocketPool::kSslConfigChanged[] =
    "SSL configuration changed";

TransportClientSocketPool::Request::Request(
    ClientSocketHandle* handle,
    CompletionOnceCallback callback,
    const ProxyAuthCallback& proxy_auth_callback,
    RequestPriority priority,
    const SocketTag& socket_tag,
    RespectLimits respect_limits,
    Flags flags,
    scoped_refptr<SocketParams> socket_params,
    const std::optional<NetworkTrafficAnnotationTag>& proxy_annotation_tag,
    const NetLogWithSource& net_log)
    : handle_(handle),
      callback_(std::move(callback)),
      proxy_auth_callback_(proxy_auth_callback),
      priority_(priority),
      respect_limits_(respect_limits),
      flags_(flags),
      socket_params_(std::move(socket_params)),
      proxy_annotation_tag_(proxy_annotation_tag),
      net_log_(net_log),
      socket_tag_(socket_tag) {
  if (respect_limits_ == ClientSocketPool::RespectLimits::DISABLED)
    DCHECK_EQ(priority_, MAXIMUM_PRIORITY);
}

TransportClientSocketPool::Request::~Request() = default;

void TransportClientSocketPool::Request::AssignJob(ConnectJob* job) {
  DCHECK(job);
  DCHECK(!job_);
  job_ = job;
  if (job_->priority() != priority_)
    job_->ChangePriority(priority_);
}

ConnectJob* TransportClientSocketPool::Request::ReleaseJob() {
  DCHECK(job_);
  ConnectJob* job = job_;
  job_ = nullptr;
  return job;
}

struct TransportClientSocketPool::IdleSocket {
  // An idle socket can't be used if it is disconnected or has been used
  // before and has received data unexpectedly (hence no longer idle).  The
  // unread data would be mistaken for the beginning of the next response if
  // we were to use the socket for a new request.
  //
  // Note that a socket that has never been used before (like a preconnected
  // socket) may be used even with unread data.  This may be, e.g., a SPDY
  // SETTINGS frame.
  //
  // If the socket is not usable, |net_log_reason_utf8| is set to a string
  // indicating why the socket is not usable.
  bool IsUsable(const char** net_log_reason_utf8) const;

  std::unique_ptr<StreamSocket> socket;
  base::TimeTicks start_time;
};

TransportClientSocketPool::TransportClientSocketPool(
    int max_sockets,
    int max_sockets_per_group,
    base::TimeDelta unused_idle_socket_timeout,
    const ProxyChain& proxy_chain,
    bool is_for_websockets,
    const CommonConnectJobParams* common_connect_job_params,
    bool cleanup_on_ip_address_change)
    : TransportClientSocketPool(max_sockets,
                                max_sockets_per_group,
                                unused_idle_socket_timeout,
                                ClientSocketPool::used_idle_socket_timeout(),
                                proxy_chain,
                                is_for_websockets,
                                common_connect_job_params,
                                cleanup_on_ip_address_change,
                                std::make_unique<ConnectJobFactory>(),
                                common_connect_job_params->ssl_client_context,
                                /*connect_backup_jobs_enabled=*/true) {}

TransportClientSocketPool::~TransportClientSocketPool() {
  // Clean up any idle sockets and pending connect jobs.  Assert that we have no
  // remaining active sockets or pending requests.  They should have all been
  // cleaned up prior to |this| being destroyed.
  FlushWithError(ERR_ABORTED, kSocketPoolDestroyed);
  DCHECK(group_map_.empty());
  DCHECK(pending_callback_map_.empty());
  DCHECK_EQ(0, connecting_socket_count_);
  DCHECK_EQ(0, handed_out_socket_count_);
  CHECK(higher_pools_.empty());

  if (ssl_client_context_)
    ssl_client_context_->RemoveObserver(this);

  if (cleanup_on_ip_address_change_)
    NetworkChangeNotifier::RemoveIPAddressObserver(this);
}

std::unique_ptr<TransportClientSocketPool>
TransportClientSocketPool::CreateForTesting(
    int max_sockets,
    int max_sockets_per_group,
    base::TimeDelta unused_idle_socket_timeout,
    base::TimeDelta used_idle_socket_timeout,
    const ProxyChain& proxy_chain,
    bool is_for_websockets,
    const CommonConnectJobParams* common_connect_job_params,
    std::unique_ptr<ConnectJobFactory> connect_job_factory,
    SSLClientContext* ssl_client_context,
    bool connect_backup_jobs_enabled) {
  return base::WrapUnique<TransportClientSocketPool>(
      new TransportClientSocketPool(
          max_sockets, max_sockets_per_group, unused_idle_socket_timeout,
          used_idle_socket_timeout, proxy_chain, is_for_websockets,
          common_connect_job_params, /*cleanup_on_ip_address_change=*/true,
          std::move(connect_job_factory), ssl_client_context,
          connect_backup_jobs_enabled));
}

TransportClientSocketPool::CallbackResultPair::CallbackResultPair()
    : result(OK) {}

TransportClientSocketPool::CallbackResultPair::CallbackResultPair(
    CompletionOnceCallback callback_in,
    int result_in)
    : callback(std::move(callback_in)), result(result_in) {}

TransportClientSocketPool::CallbackResultPair::CallbackResultPair(
    TransportClientSocketPool::CallbackResultPair&& other) = default;

TransportClientSocketPool::CallbackResultPair&
TransportClientSocketPool::CallbackResultPair::operator=(
    TransportClientSocketPool::CallbackResultPair&& other) = default;

TransportClientSocketPool::CallbackResultPair::~CallbackResultPair() = default;

bool TransportClientSocketPool::IsStalled() const {
  // If fewer than |max_sockets_| are in use, then clearly |this| is not
  // stalled.
  if ((handed_out_socket_count_ + connecting_socket_count_) < max_sockets_)
    return false;
  // So in order to be stalled, |this| must be using at least |max_sockets_| AND
  // |this| must have a request that is actually stalled on the global socket
  // limit.  To find such a request, look for a group that has more requests
  // than jobs AND where the number of sockets is less than
  // |max_sockets_per_group_|.  (If the number of sockets is equal to
  // |max_sockets_per_group_|, then the request is stalled on the group limit,
  // which does not count.)
  for (const auto& it : group_map_) {
    if (it.second->CanUseAdditionalSocketSlot(max_sockets_per_group_))
      return true;
  }
  return false;
}

void TransportClientSocketPool::AddHigherLayeredPool(
    HigherLayeredPool* higher_pool) {
  CHECK(higher_pool);
  CHECK(!base::Contains(higher_pools_, higher_pool));
  higher_pools_.insert(higher_pool);
}

void TransportClientSocketPool::RemoveHigherLayeredPool(
    HigherLayeredPool* higher_pool) {
  CHECK(higher_pool);
  CHECK(base::Contains(higher_pools_, higher_pool));
  higher_pools_.erase(higher_pool);
}

int TransportClientSocketPool::RequestSocket(
    const GroupId& group_id,
    scoped_refptr<SocketParams> params,
    const std::optional<NetworkTrafficAnnotationTag>& proxy_annotation_tag,
    RequestPriority priority,
    const SocketTag& socket_tag,
    RespectLimits respect_limits,
    ClientSocketHandle* handle,
    CompletionOnceCallback callback,
    const ProxyAuthCallback& proxy_auth_callback,
    const NetLogWithSource& net_log) {
  CHECK(callback);
  CHECK(handle);

  NetLogTcpClientSocketPoolRequestedSocket(net_log, group_id);

  std::unique_ptr<Request> request = std::make_unique<Request>(
      handle, std::move(callback), proxy_auth_callback, priority, socket_tag,
      respect_limits, NORMAL, std::move(params), proxy_annotation_tag, net_log);

  // Cleanup any timed-out idle sockets.
  CleanupIdleSockets(false, nullptr /* net_log_reason_utf8 */);

  request->net_log().BeginEvent(NetLogEventType::SOCKET_POOL);

  int rv =
      RequestSocketInternal(group_id, *request,
                            /*preconnect_done_closure=*/base::OnceClosure());
  if (rv != ERR_IO_PENDING) {
    if (rv == OK) {
      request->handle()->socket()->ApplySocketTag(request->socket_tag());
    }
    request->net_log().EndEventWithNetErrorCode(NetLogEventType::SOCKET_POOL,
                                                rv);
    CHECK(!request->handle()->is_initialized());
    request.reset();
  } else {
    Group* group = GetOrCreateGroup(group_id);
    group->InsertUnboundRequest(std::move(request));
    // Have to do this asynchronously, as closing sockets in higher level pools
    // call back in to |this|, which will cause all sorts of fun and exciting
    // re-entrancy issues if the socket pool is doing something else at the
    // time.
    if (group->CanUseAdditionalSocketSlot(max_sockets_per_group_)) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(
              &TransportClientSocketPool::TryToCloseSocketsInLayeredPools,
              weak_factory_.GetWeakPtr()));
    }
  }
  return rv;
}

int TransportClientSocketPool::RequestSockets(
    const GroupId& group_id,
    scoped_refptr<SocketParams> params,
    const std::optional<NetworkTrafficAnnotationTag>& proxy_annotation_tag,
    int num_sockets,
    CompletionOnceCallback callback,
    const NetLogWithSource& net_log) {
  // TODO(eroman): Split out the host and port parameters.
  net_log.AddEvent(NetLogEventType::TCP_CLIENT_SOCKET_POOL_REQUESTED_SOCKETS,
                   [&] { return NetLogGroupIdParams(group_id); });

  Request request(nullptr /* no handle */, CompletionOnceCallback(),
                  ProxyAuthCallback(), IDLE, SocketTag(),
                  RespectLimits::ENABLED, NO_IDLE_SOCKETS, std::move(params),
                  proxy_annotation_tag, net_log);

  // Cleanup any timed-out idle sockets.
  CleanupIdleSockets(false, nullptr /* net_log_reason_utf8 */);

  if (num_sockets > max_sockets_per_group_) {
    num_sockets = max_sockets_per_group_;
  }

  request.net_log().BeginEventWithIntParams(
      NetLogEventType::SOCKET_POOL_CONNECTING_N_SOCKETS, "num_sockets",
      num_sockets);

  Group* group = GetOrCreateGroup(group_id);

  // RequestSocketsInternal() may delete the group.
  bool deleted_group = false;

  int rv = OK;

  base::RepeatingClosure preconnect_done_closure = base::BarrierClosure(
      num_sockets,
      base::BindOnce(
          [](CompletionOnceCallback callback) {
            base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE, base::BindOnce(std::move(callback), OK));
          },
          std::move(callback)));
  int pending_connect_job_count = 0;
  for (int num_iterations_left = num_sockets;
       group->NumActiveSocketSlots() < num_sockets && num_iterations_left > 0;
       num_iterations_left--) {
    rv = RequestSocketInternal(group_id, request, preconnect_done_closure);
    if (rv == ERR_IO_PENDING) {
      ++pending_connect_job_count;
    }
    if (rv < 0 && rv != ERR_IO_PENDING) {
      // We're encountering a synchronous error.  Give up.
      if (!base::Contains(group_map_, group_id))
        deleted_group = true;
      break;
    }
    if (!base::Contains(group_map_, group_id)) {
      // Unexpected.  The group should only be getting deleted on synchronous
      // error.
      NOTREACHED_IN_MIGRATION();
      deleted_group = true;
      break;
    }
  }

  if (!deleted_group && group->IsEmpty())
    RemoveGroup(group_id);

  if (rv == ERR_IO_PENDING)
    rv = OK;
  request.net_log().EndEventWithNetErrorCode(
      NetLogEventType::SOCKET_POOL_CONNECTING_N_SOCKETS, rv);

  // Currently we don't handle preconnect errors. So this method returns OK even
  // if failed to preconnect.
  // TODO(crbug.com/40843081): Consider support error handlings when needed.
  if (pending_connect_job_count == 0)
    return OK;
  for (int i = 0; i < num_sockets - pending_connect_job_count; ++i) {
    preconnect_done_closure.Run();
  }

  return ERR_IO_PENDING;
}

int TransportClientSocketPool::RequestSocketInternal(
    const GroupId& group_id,
    const Request& request,
    base::OnceClosure preconnect_done_closure) {
#if DCHECK_IS_ON()
  DCHECK(!request_in_process_);
  base::AutoReset<bool> auto_reset(&request_in_process_, true);
#endif  // DCHECK_IS_ON()

  ClientSocketHandle* const handle = request.handle();
  const bool preconnecting = !handle;
  DCHECK_EQ(preconnecting, !!preconnect_done_closure);

  Group* group = nullptr;
  auto group_it = group_map_.find(group_id);
  if (group_it != group_map_.end()) {
    group = group_it->second;

    if (!(request.flags() & NO_IDLE_SOCKETS)) {
      // Try to reuse a socket.
      if (AssignIdleSocketToRequest(request, group))
        return OK;
    }

    // If there are more ConnectJobs than pending requests, don't need to do
    // anything.  Can just wait for the extra job to connect, and then assign it
    // to the request.
    if (!preconnecting && group->TryToUseNeverAssignedConnectJob())
      return ERR_IO_PENDING;

    // Can we make another active socket now?
    if (!group->HasAvailableSocketSlot(max_sockets_per_group_) &&
        request.respect_limits() == RespectLimits::ENABLED) {
      // TODO(willchan): Consider whether or not we need to close a socket in a
      // higher layered group. I don't think this makes sense since we would
      // just reuse that socket then if we needed one and wouldn't make it down
      // to this layer.
      request.net_log().AddEvent(
          NetLogEventType::SOCKET_POOL_STALLED_MAX_SOCKETS_PER_GROUP);
      return preconnecting ? ERR_PRECONNECT_MAX_SOCKET_LIMIT : ERR_IO_PENDING;
    }
  }

  if (ReachedMaxSocketsLimit() &&
      request.respect_limits() == RespectLimits::ENABLED) {
    // NOTE(mmenke):  Wonder if we really need different code for each case
    // here.  Only reason for them now seems to be preconnects.
    if (idle_socket_count_ > 0) {
      // There's an idle socket in this pool. Either that's because there's
      // still one in this group, but we got here due to preconnecting
      // bypassing idle sockets, or because there's an idle socket in another
      // group.
      bool closed = CloseOneIdleSocketExceptInGroup(group);
      if (preconnecting && !closed)
        return ERR_PRECONNECT_MAX_SOCKET_LIMIT;
    } else {
      // We could check if we really have a stalled group here, but it
      // requires a scan of all groups, so just flip a flag here, and do the
      // check later.
      request.net_log().AddEvent(
          NetLogEventType::SOCKET_POOL_STALLED_MAX_SOCKETS);
      return preconnecting ? ERR_PRECONNECT_MAX_SOCKET_LIMIT : ERR_IO_PENDING;
    }
  }

  // We couldn't find a socket to reuse, and there's space to allocate one,
  // so allocate and connect a new one.
  group = GetOrCreateGroup(group_id);
  std::unique_ptr<ConnectJob> connect_job(
      CreateConnectJob(group_id, request.socket_params(), proxy_chain_,
                       request.proxy_annotation_tag(), request.priority(),
                       request.socket_tag(), group));
  connect_job->net_log().AddEvent(
      NetLogEventType::SOCKET_POOL_CONNECT_JOB_CREATED, [&] {
        return NetLogCreateConnectJobParams(false /* backup_job */, &group_id);
      });

  int rv = connect_job->Connect();
  if (rv == ERR_IO_PENDING) {
    if (preconnect_done_closure) {
      DCHECK(preconnecting);
      connect_job->set_done_closure(std::move(preconnect_done_closure));
    }
    // If we didn't have any sockets in this group, set a timer for potentially
    // creating a new one.  If the SYN is lost, this backup socket may complete
    // before the slow socket, improving end user latency.
    if (connect_backup_jobs_enabled_ && group->IsEmpty())
      group->StartBackupJobTimer(group_id);
    group->AddJob(std::move(connect_job), preconnecting);
    connecting_socket_count_++;
    return rv;
  }

  LogBoundConnectJobToRequest(connect_job->net_log().source(), request);
  if (preconnecting) {
    if (rv == OK)
      AddIdleSocket(connect_job->PassSocket(), group);
  } else {
    DCHECK(handle);
    if (rv != OK)
      handle->SetAdditionalErrorState(connect_job.get());
    std::unique_ptr<StreamSocket> socket = connect_job->PassSocket();
    if (socket) {
      HandOutSocket(std::move(socket),
                    StreamSocketHandle::SocketReuseType::kUnused,
                    connect_job->connect_timing(), handle,
                    /*time_idle=*/base::TimeDelta(), group, request.net_log());
    }
  }
  if (group->IsEmpty())
    RemoveGroup(group_id);

  return rv;
}

bool TransportClientSocketPool::AssignIdleSocketToRequest(
    const Request& request,
    Group* group) {
  std::list<IdleSocket>* idle_sockets = group->mutable_idle_sockets();
  auto idle_socket_it = idle_sockets->end();

  // Iterate through the idle sockets forwards (oldest to newest)
  //   * Delete any disconnected ones.
  //   * If we find a used idle socket, assign to |idle_socket|.  At the end,
  //   the |idle_socket_it| will be set to the newest used idle socket.
  for (auto it = idle_sockets->begin(); it != idle_sockets->end();) {
    // Check whether socket is usable. Note that it's unlikely that the socket
    // is not usable because this function is always invoked after a
    // reusability check, but in theory socket can be closed asynchronously.
    const char* net_log_reason_utf8;
    if (!it->IsUsable(&net_log_reason_utf8)) {
      it->socket->NetLog().AddEventWithStringParams(
          NetLogEventType::SOCKET_POOL_CLOSING_SOCKET, "reason",
          net_log_reason_utf8);
      DecrementIdleCount();
      it = idle_sockets->erase(it);
      continue;
    }

    if (it->socket->WasEverUsed()) {
      // We found one we can reuse!
      idle_socket_it = it;
    }

    ++it;
  }

  // If we haven't found an idle socket, that means there are no used idle
  // sockets.  Pick the oldest (first) idle socket (FIFO).

  if (idle_socket_it == idle_sockets->end() && !idle_sockets->empty())
    idle_socket_it = idle_sockets->begin();

  if (idle_socket_it != idle_sockets->end()) {
    DecrementIdleCount();
    base::TimeDelta idle_time =
        base::TimeTicks::Now() - idle_socket_it->start_time;
    std::unique_ptr<StreamSocket> socket = std::move(idle_socket_it->socket);
    idle_sockets->erase(idle_socket_it);
    // TODO(davidben): If |idle_time| is under some low watermark, consider
    // treating as UNUSED rather than UNUSED_IDLE. This will avoid
    // HttpNetworkTransaction retrying on some errors.
    ClientSocketHandle::SocketReuseType reuse_type =
        socket->WasEverUsed()
            ? StreamSocketHandle::SocketReuseType::kReusedIdle
            : StreamSocketHandle::SocketReuseType::kUnusedIdle;

    HandOutSocket(std::move(socket), reuse_type,
                  LoadTimingInfo::ConnectTiming(), request.handle(), idle_time,
                  group, request.net_log());
    return true;
  }

  return false;
}

// static
void TransportClientSocketPool::LogBoundConnectJobToRequest(
    const NetLogSource& connect_job_source,
    const Request& request) {
  request.net_log().AddEventReferencingSource(
      NetLogEventType::SOCKET_POOL_BOUND_TO_CONNECT_JOB, connect_job_source);
}

void TransportClientSocketPool::SetPriority(const GroupId& group_id,
                                            ClientSocketHandle* handle,
                                            RequestPriority priority) {
  auto group_it = group_map_.find(group_id);
  if (group_it == group_map_.end()) {
    DCHECK(base::Contains(pending_callback_map_, handle));
    // The Request has already completed and been destroyed; nothing to
    // reprioritize.
    return;
  }

  group_it->second->SetPriority(handle, priority);
}

void TransportClientSocketPool::CancelRequest(const GroupId& group_id,
                                              ClientSocketHandle* handle,
                                              bool cancel_connect_job) {
  auto callback_it = pending_callback_map_.find(handle);
  if (callback_it != pending_callback_map_.end()) {
    int result = callback_it->second.result;
    pending_callback_map_.erase(callback_it);
    std::unique_ptr<StreamSocket> socket = handle->PassSocket();
    if (socket) {
      if (result != OK) {
        socket->Disconnect();
      } else if (cancel_connect_job) {
        // Close the socket if |cancel_connect_job| is true and there are no
        // other pending requests.
        Group* group = GetOrCreateGroup(group_id);
        if (group->unbound_request_count() == 0)
          socket->Disconnect();
      }
      ReleaseSocket(handle->group_id(), std::move(socket),
                    handle->group_generation());
    }
    return;
  }

  CHECK(base::Contains(group_map_, group_id));
  Group* group = GetOrCreateGroup(group_id);

  std::unique_ptr<Request> request = group->FindAndRemoveBoundRequest(handle);
  if (request) {
    --connecting_socket_count_;
    OnAvailableSocketSlot(group_id, group);
    CheckForStalledSocketGroups();
    return;
  }

  // Search |unbound_requests_| for matching handle.
  request = group->FindAndRemoveUnboundRequest(handle);
  if (request) {
    request->net_log().AddEvent(NetLogEventType::CANCELLED);
    request->net_log().EndEvent(NetLogEventType::SOCKET_POOL);

    // Let the job run, unless |cancel_connect_job| is true, or we're at the
    // socket limit and there are no other requests waiting on the job.
    bool reached_limit = ReachedMaxSocketsLimit();
    if (group->jobs().size() > group->unbound_request_count() &&
        (cancel_connect_job || reached_limit)) {
      RemoveConnectJob(group->jobs().begin()->get(), group);
      if (group->IsEmpty())
        RemoveGroup(group->group_id());
      if (reached_limit)
        CheckForStalledSocketGroups();
    }
  }
}

void TransportClientSocketPool::CloseIdleSockets(
    const char* net_log_reason_utf8) {
  CleanupIdleSockets(true, net_log_reason_utf8);
  DCHECK_EQ(0, idle_socket_count_);
}

void TransportClientSocketPool::CloseIdleSocketsInGroup(
    const GroupId& group_id,
    const char* net_log_reason_utf8) {
  if (idle_socket_count_ == 0)
    return;
  auto it = group_map_.find(group_id);
  if (it == group_map_.end())
    return;
  CleanupIdleSocketsInGroup(true, it->second, base::TimeTicks::Now(),
                            net_log_reason_utf8);
  if (it->second->IsEmpty())
    RemoveGroup(it);
}

int TransportClientSocketPool::IdleSocketCount() const {
  return idle_socket_count_;
}

size_t TransportClientSocketPool::IdleSocketCountInGroup(
    const GroupId& group_id) const {
  auto i = group_map_.find(group_id);
  CHECK(i != group_map_.end());

  return i->second->idle_sockets().size();
}

LoadState TransportClientSocketPool::GetLoadState(
    const GroupId& group_id,
    const ClientSocketHandle* handle) const {
  if (base::Contains(pending_callback_map_, handle))
    return LOAD_STATE_CONNECTING;

  auto group_it = group_map_.find(group_id);
  if (group_it == group_map_.end()) {
    // TODO(mmenke):  This is actually reached in the wild, for unknown reasons.
    // Would be great to understand why, and if it's a bug, fix it.  If not,
    // should have a test for that case.
    NOTREACHED_IN_MIGRATION();
    return LOAD_STATE_IDLE;
  }

  const Group& group = *group_it->second;
  ConnectJob* job = group.GetConnectJobForHandle(handle);
  if (job)
    return job->GetLoadState();

  if (group.CanUseAdditionalSocketSlot(max_sockets_per_group_))
    return LOAD_STATE_WAITING_FOR_STALLED_SOCKET_POOL;
  return LOAD_STATE_WAITING_FOR_AVAILABLE_SOCKET;
}

base::Value TransportClientSocketPool::GetInfoAsValue(
    const std::string& name,
    const std::string& type) const {
  // TODO(mmenke): This currently doesn't return bound Requests or ConnectJobs.
  auto dict = base::Value::Dict()
                  .Set("name", name)
                  .Set("type", type)
                  .Set("handed_out_socket_count", handed_out_socket_count_)
                  .Set("connecting_socket_count", connecting_socket_count_)
                  .Set("idle_socket_count", idle_socket_count_)
                  .Set("max_socket_count", max_sockets_)
                  .Set("max_sockets_per_group", max_sockets_per_group_);

  if (group_map_.empty())
    return base::Value(std::move(dict));

  base::Value::Dict all_groups_dict;
  for (const auto& entry : group_map_) {
    const Group* group = entry.second;

    base::Value::List idle_socket_list;
    for (const auto& idle_socket : group->idle_sockets()) {
      int source_id = idle_socket.socket->NetLog().source().id;
      idle_socket_list.Append(source_id);
    }

    base::Value::List connect_jobs_list;
    for (const auto& job : group->jobs()) {
      int source_id = job->net_log().source().id;
      connect_jobs_list.Append(source_id);
    }

    auto group_dict =
        base::Value::Dict()
            .Set("pending_request_count",
                 static_cast<int>(group->unbound_request_count()))
            .Set("active_socket_count", group->active_socket_count())
            .Set("idle_sockets", std::move(idle_socket_list))
            .Set("connect_jobs", std::move(connect_jobs_list))
            .Set("is_stalled",
                 group->CanUseAdditionalSocketSlot(max_sockets_per_group_))
            .Set("backup_job_timer_is_running",
                 group->BackupJobTimerIsRunning());

    if (group->has_unbound_requests()) {
      group_dict.Set("top_pending_priority",
                     RequestPriorityToString(group->TopPendingPriority()));
    }

    all_groups_dict.Set(entry.first.ToString(), std::move(group_dict));
  }
  dict.Set("groups", std::move(all_groups_dict));
  return base::Value(std::move(dict));
}

bool TransportClientSocketPool::HasActiveSocket(const GroupId& group_id) const {
  return HasGroup(group_id);
}

bool TransportClientSocketPool::IdleSocket::IsUsable(
    const char** net_log_reason_utf8) const {
  DCHECK(net_log_reason_utf8);
  if (socket->WasEverUsed()) {
    if (!socket->IsConnectedAndIdle()) {
      if (!socket->IsConnected()) {
        *net_log_reason_utf8 = kRemoteSideClosedConnection;
      } else {
        *net_log_reason_utf8 = kDataReceivedUnexpectedly;
      }
      return false;
    }
    return true;
  }

  if (!socket->IsConnected()) {
    *net_log_reason_utf8 = kRemoteSideClosedConnection;
    return false;
  }
  return true;
}

TransportClientSocketPool::TransportClientSocketPool(
    int max_sockets,
    int max_sockets_per_group,
    base::TimeDelta unused_idle_socket_timeout,
    base::TimeDelta used_idle_socket_timeout,
    const ProxyChain& proxy_chain,
    bool is_for_websockets,
    const CommonConnectJobParams* common_connect_job_params,
    bool cleanup_on_ip_address_change,
    std::unique_ptr<ConnectJobFactory> connect_job_factory,
    SSLClientContext* ssl_client_context,
    bool connect_backup_jobs_enabled)
    : ClientSocketPool(is_for_websockets,
                       common_connect_job_params,
                       std::move(connect_job_factory)),
      max_sockets_(max_sockets),
      max_sockets_per_group_(max_sockets_per_group),
      unused_idle_socket_timeout_(unused_idle_socket_timeout),
      used_idle_socket_timeout_(used_idle_socket_timeout),
      proxy_chain_(proxy_chain),
      cleanup_on_ip_address_change_(cleanup_on_ip_address_change),
      connect_backup_jobs_enabled_(connect_backup_jobs_enabled &&
                                   g_connect_backup_jobs_enabled),
      ssl_client_context_(ssl_client_context) {
  DCHECK_LE(0, max_sockets_per_group);
  DCHECK_LE(max_sockets_per_group, max_sockets);

  if (cleanup_on_ip_address_change_)
    NetworkChangeNotifier::AddIPAddressObserver(this);

  if (ssl_client_context_)
    ssl_client_context_->AddObserver(this);
}

void TransportClientSocketPool::OnSSLConfigChanged(
    SSLClientContext::SSLConfigChangeType change_type) {
  const char* message = nullptr;
  // When the SSL config or cert verifier config changes, flush all idle
  // sockets so they won't get re-used, and allow any active sockets to finish,
  // but don't put them back in the socket pool.
  switch (change_type) {
    case SSLClientContext::SSLConfigChangeType::kSSLConfigChanged:
      message = kNetworkChanged;
      break;
    case SSLClientContext::SSLConfigChangeType::kCertDatabaseChanged:
      message = kCertDatabaseChanged;
      break;
    case SSLClientContext::SSLConfigChangeType::kCertVerifierChanged:
      message = kCertVerifierChanged;
      break;
  };

  base::TimeTicks now = base::TimeTicks::Now();
  for (auto it = group_map_.begin(); it != group_map_.end();) {
    it = RefreshGroup(it, now, message);
  }
  CheckForStalledSocketGroups();
}

// TODO(crbug.com/40181080): Get `server` as SchemeHostPort?
void TransportClientSocketPool::OnSSLConfigForServersChanged(
    const base::flat_set<HostPortPair>& servers) {
  // Current time value. Retrieving it once at the function start rather than
  // inside the inner loop, since it shouldn't change by any meaningful amount.
  //
  // TODO(davidben): This value is not actually needed because
  // CleanupIdleSocketsInGroup() is called with |force| = true. Tidy up
  // interfaces so the parameter is not necessary.
  base::TimeTicks now = base::TimeTicks::Now();

  // If the proxy chain includes a server from `servers` and uses SSL settings
  // (HTTPS or QUIC), refresh every group.
  bool proxy_matches = false;
  for (const ProxyServer& proxy_server : proxy_chain_.proxy_servers()) {
    if (proxy_server.is_secure_http_like() &&
        servers.contains(proxy_server.host_port_pair())) {
      proxy_matches = true;
    }
  }

  bool refreshed_any = false;
  for (auto it = group_map_.begin(); it != group_map_.end();) {
    if (proxy_matches ||
        (GURL::SchemeIsCryptographic(it->first.destination().scheme()) &&
         servers.contains(
             HostPortPair::FromSchemeHostPort(it->first.destination())))) {
      refreshed_any = true;
      // Note this call may destroy the group and invalidate |to_refresh|.
      it = RefreshGroup(it, now, kSslConfigChanged);
    } else {
      ++it;
    }
  }

  if (refreshed_any) {
    // Check to see if any group can use the freed up socket slots. It would be
    // more efficient to give the slots to the refreshed groups, if the still
    // exists and need them, but this should be rare enough that it doesn't
    // matter. This will also make sure the slots are given to the group with
    // the highest priority request without an assigned ConnectJob.
    CheckForStalledSocketGroups();
  }
}

bool TransportClientSocketPool::HasGroup(const GroupId& group_id) const {
  return base::Contains(group_map_, group_id);
}

void TransportClientSocketPool::CleanupIdleSockets(
    bool force,
    const char* net_log_reason_utf8) {
  if (idle_socket_count_ == 0)
    return;

  // Current time value. Retrieving it once at the function start rather than
  // inside the inner loop, since it shouldn't change by any meaningful amount.
  base::TimeTicks now = base::TimeTicks::Now();

  for (auto i = group_map_.begin(); i != group_map_.end();) {
    Group* group = i->second;
    CHECK(group);
    CleanupIdleSocketsInGroup(force, group, now, net_log_reason_utf8);
    // Delete group if no longer needed.
    if (group->IsEmpty()) {
      i = RemoveGroup(i);
    } else {
      ++i;
    }
  }
}

bool TransportClientSocketPool::CloseOneIdleSocket() {
  if (idle_socket_count_ == 0)
    return false;
  return CloseOneIdleSocketExceptInGroup(nullptr);
}

bool TransportClientSocketPool::CloseOneIdleConnectionInHigherLayeredPool() {
  // This pool doesn't have any idle sockets. It's possible that a pool at a
  // higher layer is holding one of this sockets active, but it's actually idle.
  // Query the higher layers.
  for (HigherLayeredPool* higher_pool : higher_pools_) {
    if (higher_pool->CloseOneIdleConnection())
      return true;
  }
  return false;
}

void TransportClientSocketPool::CleanupIdleSocketsInGroup(
    bool force,
    Group* group,
    const base::TimeTicks& now,
    const char* net_log_reason_utf8) {
  // If |force| is true, a reason must be provided.
  DCHECK(!force || net_log_reason_utf8);

  auto idle_socket_it = group->mutable_idle_sockets()->begin();
  while (idle_socket_it != group->idle_sockets().end()) {
    bool should_clean_up = force;
    const char* reason_for_closing_socket = net_log_reason_utf8;
    base::TimeDelta timeout = idle_socket_it->socket->WasEverUsed()
                                  ? used_idle_socket_timeout_
                                  : unused_idle_socket_timeout_;

    // Timeout errors take precedence over the reason for flushing sockets in
    // the group, if applicable.
    if (now - idle_socket_it->start_time >= timeout) {
      should_clean_up = true;
      reason_for_closing_socket = kIdleTimeLimitExpired;
    }

    // Usability errors take precedence over over other errors.
    if (!idle_socket_it->IsUsable(&reason_for_closing_socket))
      should_clean_up = true;

    if (should_clean_up) {
      DCHECK(reason_for_closing_socket);
      idle_socket_it->socket->NetLog().AddEventWithStringParams(
          NetLogEventType::SOCKET_POOL_CLOSING_SOCKET, "reason",
          reason_for_closing_socket);
      idle_socket_it = group->mutable_idle_sockets()->erase(idle_socket_it);
      DecrementIdleCount();
    } else {
      DCHECK(!reason_for_closing_socket);
      ++idle_socket_it;
    }
  }
}

TransportClientSocketPool::Group* TransportClientSocketPool::GetOrCreateGroup(
    const GroupId& group_id) {
  auto it = group_map_.find(group_id);
  if (it != group_map_.end())
    return it->second;
  Group* group = new Group(group_id, this);
  group_map_[group_id] = group;
  return group;
}

void TransportClientSocketPool::RemoveGroup(const GroupId& group_id) {
  auto it = group_map_.find(group_id);
  CHECK(it != group_map_.end());

  RemoveGroup(it);
}

TransportClientSocketPool::GroupMap::iterator
TransportClientSocketPool::RemoveGroup(GroupMap::iterator it) {
  delete it->second;
  return group_map_.erase(it);
}

// static
bool TransportClientSocketPool::connect_backup_jobs_enabled() {
  return g_connect_backup_jobs_enabled;
}

// static
bool TransportClientSocketPool::set_connect_backup_jobs_enabled(bool enabled) {
  bool old_value = g_connect_backup_jobs_enabled;
  g_connect_backup_jobs_enabled = enabled;
  return old_value;
}

void TransportClientSocketPool::IncrementIdleCount() {
  ++idle_socket_count_;
}

void TransportClientSocketPool::DecrementIdleCount() {
  --idle_socket_count_;
}

void TransportClientSocketPool::ReleaseSocket(
    const GroupId& group_id,
    std::unique_ptr<StreamSocket> socket,
    int64_t group_generation) {
  auto i = group_map_.find(group_id);
  CHECK(i != group_map_.end());

  Group* group = i->second;
  CHECK(group);

  CHECK_GT(handed_out_socket_count_, 0);
  handed_out_socket_count_--;

  CHECK_GT(group->active_socket_count(), 0);
  group->DecrementActiveSocketCount();

  bool can_resuse_socket = false;
  std::string_view not_reusable_reason;
  if (!socket->IsConnectedAndIdle()) {
    if (!socket->IsConnected()) {
      not_reusable_reason = kClosedConnectionReturnedToPool;
    } else {
      not_reusable_reason = kDataReceivedUnexpectedly;
    }
  } else if (group_generation != group->generation()) {
    not_reusable_reason = kSocketGenerationOutOfDate;
  } else {
    can_resuse_socket = true;
  }

  if (can_resuse_socket) {
    DCHECK(not_reusable_reason.empty());

    // Add it to the idle list.
    AddIdleSocket(std::move(socket), group);
    OnAvailableSocketSlot(group_id, group);
  } else {
    DCHECK(!not_reusable_reason.empty());

    socket->NetLog().AddEventWithStringParams(
        NetLogEventType::SOCKET_POOL_CLOSING_SOCKET, "reason",
        not_reusable_reason);
    if (group->IsEmpty())
      RemoveGroup(i);
    socket.reset();
  }

  CheckForStalledSocketGroups();
}

void TransportClientSocketPool::CheckForStalledSocketGroups() {
  // Loop until there's nothing more to do.
  while (true) {
    // If we have idle sockets, see if we can give one to the top-stalled group.
    Group* top_group = nullptr;
    GroupId top_group_id;
    if (!FindTopStalledGroup(&top_group, &top_group_id))
      return;

    if (ReachedMaxSocketsLimit()) {
      if (idle_socket_count_ > 0) {
        CloseOneIdleSocket();
      } else {
        // We can't activate more sockets since we're already at our global
        // limit.
        return;
      }
    }

    // Note that this may delete top_group.
    OnAvailableSocketSlot(top_group_id, top_group);
  }
}

// Search for the highest priority pending request, amongst the groups that
// are not at the |max_sockets_per_group_| limit. Note: for requests with
// the same priority, the winner is based on group hash ordering (and not
// insertion order).
bool TransportClientSocketPool::FindTopStalledGroup(Group** group,
                                                    GroupId* group_id) const {
  CHECK(group);
  CHECK(group_id);
  Group* top_group = nullptr;
  const GroupId* top_group_id = nullptr;
  bool has_stalled_group = false;
  for (const auto& it : group_map_) {
    Group* curr_group = it.second;
    if (!curr_group->has_unbound_requests())
      continue;
    if (curr_group->CanUseAdditionalSocketSlot(max_sockets_per_group_)) {
      has_stalled_group = true;
      bool has_higher_priority =
          !top_group ||
          curr_group->TopPendingPriority() > top_group->TopPendingPriority();
      if (has_higher_priority) {
        top_group = curr_group;
        top_group_id = &it.first;
      }
    }
  }

  if (top_group) {
    *group = top_group;
    *group_id = *top_group_id;
  } else {
    CHECK(!has_stalled_group);
  }
  return has_stalled_group;
}

void TransportClientSocketPool::OnIPAddressChanged() {
  DCHECK(cleanup_on_ip_address_change_);
  FlushWithError(ERR_NETWORK_CHANGED, kNetworkChanged);
}

void TransportClientSocketPool::FlushWithError(
    int error,
    const char* net_log_reason_utf8) {
  CancelAllConnectJobs();
  CloseIdleSockets(net_log_reason_utf8);
  CancelAllRequestsWithError(error);
  for (const auto& group : group_map_) {
    group.second->IncrementGeneration();
  }
}

void TransportClientSocketPool::RemoveConnectJob(ConnectJob* job,
                                                 Group* group) {
  CHECK_GT(connecting_socket_count_, 0);
  connecting_socket_count_--;

  DCHECK(group);
  group->RemoveUnboundJob(job);
}

void TransportClientSocketPool::OnAvailableSocketSlot(const GroupId& group_id,
                                                      Group* group) {
  DCHECK(base::Contains(group_map_, group_id));
  if (group->IsEmpty()) {
    RemoveGroup(group_id);
  } else if (group->has_unbound_requests()) {
    ProcessPendingRequest(group_id, group);
  }
}

void TransportClientSocketPool::ProcessPendingRequest(const GroupId& group_id,
                                                      Group* group) {
  const Request* next_request = group->GetNextUnboundRequest();
  DCHECK(next_request);

  // If the group has no idle sockets, and can't make use of an additional slot,
  // either because it's at the limit or because it's at the socket per group
  // limit, then there's nothing to do.
  if (group->idle_sockets().empty() &&
      !group->CanUseAdditionalSocketSlot(max_sockets_per_group_)) {
    return;
  }

  int rv =
      RequestSocketInternal(group_id, *next_request,
                            /*preconnect_done_closure=*/base::OnceClosure());
  if (rv != ERR_IO_PENDING) {
    std::unique_ptr<Request> request = group->PopNextUnboundRequest();
    DCHECK(request);
    if (group->IsEmpty())
      RemoveGroup(group_id);

    request->net_log().EndEventWithNetErrorCode(NetLogEventType::SOCKET_POOL,
                                                rv);
    InvokeUserCallbackLater(request->handle(), request->release_callback(), rv,
                            request->socket_tag());
  }
}

void TransportClientSocketPool::HandOutSocket(
    std::unique_ptr<StreamSocket> socket,
    ClientSocketHandle::SocketReuseType reuse_type,
    const LoadTimingInfo::ConnectTiming& connect_timing,
    ClientSocketHandle* handle,
    base::TimeDelta idle_time,
    Group* group,
    const NetLogWithSource& net_log) {
  DCHECK(socket);
  handle->SetSocket(std::move(socket));
  handle->set_reuse_type(reuse_type);
  handle->set_idle_time(idle_time);
  handle->set_group_generation(group->generation());
  handle->set_connect_timing(connect_timing);

  if (reuse_type == StreamSocketHandle::SocketReuseType::kReusedIdle) {
    net_log.AddEventWithIntParams(
        NetLogEventType::SOCKET_POOL_REUSED_AN_EXISTING_SOCKET, "idle_ms",
        static_cast<int>(idle_time.InMilliseconds()));
  }

  net_log.AddEventReferencingSource(
      NetLogEventType::SOCKET_POOL_BOUND_TO_SOCKET,
      handle->socket()->NetLog().source());

  handed_out_socket_count_++;
  group->IncrementActiveSocketCount();
}

void TransportClientSocketPool::AddIdleSocket(
    std::unique_ptr<StreamSocket> socket,
    Group* group) {
  DCHECK(socket);
  IdleSocket idle_socket;
  idle_socket.socket = std::move(socket);
  idle_socket.start_time = base::TimeTicks::Now();

  group->mutable_idle_sockets()->push_back(std::move(idle_socket));
  IncrementIdleCount();
}

void TransportClientSocketPool::CancelAllConnectJobs() {
  for (auto i = group_map_.begin(); i != group_map_.end();) {
    Group* group = i->second;
    CHECK(group);
    connecting_socket_count_ -= group->jobs().size();
    group->RemoveAllUnboundJobs();

    // Delete group if no longer needed.
    if (group->IsEmpty()) {
      i = RemoveGroup(i);
    } else {
      ++i;
    }
  }
}

void TransportClientSocketPool::CancelAllRequestsWithError(int error) {
  for (auto i = group_map_.begin(); i != group_map_.end();) {
    Group* group = i->second;
    CHECK(group);

    while (true) {
      std::unique_ptr<Request> request = group->PopNextUnboundRequest();
      if (!request)
        break;
      InvokeUserCallbackLater(request->handle(), request->release_callback(),
                              error, request->socket_tag());
    }

    // Mark bound connect jobs as needing to fail. Can't fail them immediately
    // because they may have access to objects owned by the ConnectJob, and
    // could access them if a user callback invocation is queued. It would also
    // result in the consumer handling two messages at once, which in general
    // isn't safe for a lot of code.
    group->SetPendingErrorForAllBoundRequests(error);

    // Delete group if no longer needed.
    if (group->IsEmpty()) {
      i = RemoveGroup(i);
    } else {
      ++i;
    }
  }
}

bool TransportClientSocketPool::ReachedMaxSocketsLimit() const {
  // Each connecting socket will eventually connect and be handed out.
  int total =
      handed_out_socket_count_ + connecting_socket_count_ + idle_socket_count_;
  // There can be more sockets than the limit since some requests can ignore
  // the limit
  if (total < max_sockets_)
    return false;
  return true;
}

bool TransportClientSocketPool::CloseOneIdleSocketExceptInGroup(
    const Group* exception_group) {
  CHECK_GT(idle_socket_count_, 0);

  for (auto i = group_map_.begin(); i != group_map_.end(); ++i) {
    Group* group = i->second;
    CHECK(group);
    if (exception_group == group)
      continue;
    std::list<IdleSocket>* idle_sockets = group->mutable_idle_sockets();

    if (!idle_sockets->empty()) {
      idle_sockets->pop_front();
      DecrementIdleCount();
      if (group->IsEmpty())
        RemoveGroup(i);

      return true;
    }
  }

  return false;
}

void TransportClientSocketPool::OnConnectJobComplete(Group* group,
                                                     int result,
                                                     ConnectJob* job) {
  DCHECK_NE(ERR_IO_PENDING, result);
  DCHECK(group_map_.find(group->group_id()) != group_map_.end());
  DCHECK_EQ(group, group_map_[group->group_id()]);
  DCHECK(result != OK || job->socket() != nullptr);

  // Check if the ConnectJob is already bound to a Request. If so, result is
  // returned to that specific request.
  std::optional<Group::BoundRequest> bound_request =
      group->FindAndRemoveBoundRequestForConnectJob(job);
  Request* request = nullptr;
  std::unique_ptr<Request> owned_request;
  if (bound_request) {
    --connecting_socket_count_;

    // If the socket pools were previously flushed with an error, return that
    // error to the bound request and discard the socket.
    if (bound_request->pending_error != OK) {
      InvokeUserCallbackLater(bound_request->request->handle(),
                              bound_request->request->release_callback(),
                              bound_request->pending_error,
                              bound_request->request->socket_tag());
      bound_request->request->net_log().EndEventWithNetErrorCode(
          NetLogEventType::SOCKET_POOL, bound_request->pending_error);
      OnAvailableSocketSlot(group->group_id(), group);
      CheckForStalledSocketGroups();
      return;
    }

    // If the ConnectJob is from a previous generation, add the request back to
    // the group, and kick off another request. The socket will be discarded.
    if (bound_request->generation != group->generation()) {
      group->InsertUnboundRequest(std::move(bound_request->request));
      OnAvailableSocketSlot(group->group_id(), group);
      CheckForStalledSocketGroups();
      return;
    }

    request = bound_request->request.get();
  } else {
    // In this case, RemoveConnectJob(job, _) must be called before exiting this
    // method. Otherwise, |job| will be leaked.
    owned_request = group->PopNextUnboundRequest();
    request = owned_request.get();

    if (!request) {
      if (result == OK)
        AddIdleSocket(job->PassSocket(), group);
      RemoveConnectJob(job, group);
      OnAvailableSocketSlot(group->group_id(), group);
      CheckForStalledSocketGroups();
      return;
    }

    LogBoundConnectJobToRequest(job->net_log().source(), *request);
  }

  // The case where there's no request is handled above.
  DCHECK(request);

  if (result != OK)
    request->handle()->SetAdditionalErrorState(job);
  if (job->socket()) {
    HandOutSocket(job->PassSocket(),
                  StreamSocketHandle::SocketReuseType::kUnused,
                  job->connect_timing(), request->handle(), base::TimeDelta(),
                  group, request->net_log());
  }
  request->net_log().EndEventWithNetErrorCode(NetLogEventType::SOCKET_POOL,
                                              result);
  InvokeUserCallbackLater(request->handle(), request->release_callback(),
                          result, request->socket_tag());
  if (!bound_request)
    RemoveConnectJob(job, group);
  // If no socket was handed out, there's a new socket slot available.
  if (!request->handle()->socket()) {
    OnAvailableSocketSlot(group->group_id(), group);
    CheckForStalledSocketGroups();
  }
}

void TransportClientSocketPool::OnNeedsProxyAuth(
    Group* group,
    const HttpResponseInfo& response,
    HttpAuthController* auth_controller,
    base::OnceClosure restart_with_auth_callback,
    ConnectJob* job) {
  DCHECK(group_map_.find(group->group_id()) != group_map_.end());
  DCHECK_EQ(group, group_map_[group->group_id()]);

  const Request* request = group->BindRequestToConnectJob(job);
  // If can't bind the ConnectJob to a request, treat this as a ConnectJob
  // failure.
  if (!request) {
    OnConnectJobComplete(group, ERR_PROXY_AUTH_REQUESTED, job);
    return;
  }

  request->proxy_auth_callback().Run(response, auth_controller,
                                     std::move(restart_with_auth_callback));
}

void TransportClientSocketPool::InvokeUserCallbackLater(
    ClientSocketHandle* handle,
    CompletionOnceCallback callback,
    int rv,
    const SocketTag& socket_tag) {
  CHECK(!base::Contains(pending_callback_map_, handle));
  pending_callback_map_[handle] = CallbackResultPair(std::move(callback), rv);
  if (rv == OK) {
    handle->socket()->ApplySocketTag(socket_tag);
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&TransportClientSocketPool::InvokeUserCallback,
                                weak_factory_.GetWeakPtr(),
                                // This is safe as `handle` is checked against a
                                // map to verify it's alive before dereference.
                                // This code path must only be reachable by
                                // `handle`s that have had Init called.
                                base::UnsafeDangling(handle)));
}

void TransportClientSocketPool::InvokeUserCallback(
    MayBeDangling<ClientSocketHandle> handle) {
  auto it = pending_callback_map_.find(handle);

  // Exit if the request has already been cancelled.
  if (it == pending_callback_map_.end())
    return;

  CHECK(!handle->is_initialized());
  CompletionOnceCallback callback = std::move(it->second.callback);
  int result = it->second.result;
  pending_callback_map_.erase(it);
  std::move(callback).Run(result);
}

void TransportClientSocketPool::TryToCloseSocketsInLayeredPools() {
  while (IsStalled()) {
    // Closing a socket will result in calling back into |this| to use the freed
    // socket slot, so nothing else is needed.
    if (!CloseOneIdleConnectionInHigherLayeredPool())
      return;
  }
}

TransportClientSocketPool::GroupMap::iterator
TransportClientSocketPool::RefreshGroup(GroupMap::iterator it,
                                        const base::TimeTicks& now,
                                        const char* net_log_reason_utf8) {
  Group* group = it->second;
  CHECK(group);
  CleanupIdleSocketsInGroup(true /* force */, group, now, net_log_reason_utf8);

  connecting_socket_count_ -= group->jobs().size();
  group->RemoveAllUnboundJobs();

  // Otherwise, prevent reuse of existing sockets.
  group->IncrementGeneration();

  // Delete group if no longer needed.
  if (group->IsEmpty()) {
    return RemoveGroup(it);
  }
  return ++it;
}

TransportClientSocketPool::Group::Group(
    const GroupId& group_id,
    TransportClientSocketPool* client_socket_pool)
    : group_id_(group_id),
      client_socket_pool_(client_socket_pool),
      unbound_requests_(NUM_PRIORITIES) {}

TransportClientSocketPool::Group::~Group() {
  DCHECK_EQ(0u, never_assigned_job_count());
  DCHECK_EQ(0u, unassigned_job_count());
  DCHECK(unbound_requests_.empty());
  DCHECK(jobs_.empty());
  DCHECK(bound_requests_.empty());
}

void TransportClientSocketPool::Group::OnConnectJobComplete(int result,
                                                            ConnectJob* job) {
  DCHECK_NE(ERR_IO_PENDING, result);
  client_socket_pool_->OnConnectJobComplete(this, result, job);
}

void TransportClientSocketPool::Group::OnNeedsProxyAuth(
    const HttpResponseInfo& response,
    HttpAuthController* auth_controller,
    base::OnceClosure restart_with_auth_callback,
    ConnectJob* job) {
  client_socket_pool_->OnNeedsProxyAuth(this, response, auth_controller,
                                        std::move(restart_with_auth_callback),
                                        job);
}

void TransportClientSocketPool::Group::StartBackupJobTimer(
    const GroupId& group_id) {
  // Only allow one timer to run at a time.
  if (BackupJobTimerIsRunning())
    return;

  // Unretained here is okay because |backup_job_timer_| is
  // automatically cancelled when it's destroyed.
  backup_job_timer_.Start(FROM_HERE,
                          client_socket_pool_->ConnectRetryInterval(),
                          base::BindOnce(&Group::OnBackupJobTimerFired,
                                         base::Unretained(this), group_id));
}

bool TransportClientSocketPool::Group::BackupJobTimerIsRunning() const {
  return backup_job_timer_.IsRunning();
}

bool TransportClientSocketPool::Group::TryToUseNeverAssignedConnectJob() {
  SanityCheck();

  if (never_assigned_job_count_ == 0)
    return false;
  --never_assigned_job_count_;
  return true;
}

void TransportClientSocketPool::Group::AddJob(std::unique_ptr<ConnectJob> job,
                                              bool is_preconnect) {
  SanityCheck();

  if (is_preconnect)
    ++never_assigned_job_count_;
  jobs_.push_back(std::move(job));
  TryToAssignUnassignedJob(jobs_.back().get());

  SanityCheck();
}

std::unique_ptr<ConnectJob> TransportClientSocketPool::Group::RemoveUnboundJob(
    ConnectJob* job) {
  SanityCheck();

  // Check that |job| is in the list.
  auto it = base::ranges::find(jobs_, job, &std::unique_ptr<ConnectJob>::get);
  CHECK(it != jobs_.end(), base::NotFatalUntil::M130);

  // Check if |job| is in the unassigned jobs list. If so, remove it.
  auto it2 = base::ranges::find(unassigned_jobs_, job);
  if (it2 != unassigned_jobs_.end()) {
    unassigned_jobs_.erase(it2);
  } else {
    // Otherwise, |job| must be assigned to some Request. Unassign it, then
    // try to replace it with another job if possible (either by taking an
    // unassigned job or stealing from another request, if any requests after it
    // have a job).
    RequestQueue::Pointer request_with_job = FindUnboundRequestWithJob(job);
    DCHECK(!request_with_job.is_null());
    request_with_job.value()->ReleaseJob();
    TryToAssignJobToRequest(request_with_job);
  }
  std::unique_ptr<ConnectJob> owned_job = std::move(*it);
  jobs_.erase(it);

  size_t job_count = jobs_.size();
  if (job_count < never_assigned_job_count_)
    never_assigned_job_count_ = job_count;

  // If we've got no more jobs for this group, then we no longer need a
  // backup job either.
  if (jobs_.empty()) {
    DCHECK(unassigned_jobs_.empty());
    backup_job_timer_.Stop();
  }

  SanityCheck();
  return owned_job;
}

void TransportClientSocketPool::Group::OnBackupJobTimerFired(
    const GroupId& group_id) {
  // If there are no more jobs pending, there is no work to do.
  // If we've done our cleanups correctly, this should not happen.
  if (jobs_.empty()) {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  // If the old job has already established a connection, don't start a backup
  // job. Backup jobs are only for issues establishing the initial TCP
  // connection - the timeout they used is tuned for that, and tests expect that
  // behavior.
  //
  // TODO(crbug.com/41440018): Replace both this and the
  // LOAD_STATE_RESOLVING_HOST check with a callback. Use the
  // LOAD_STATE_RESOLVING_HOST callback to start the timer (And invoke the
  // OnHostResolved callback of any pending requests), and the
  // HasEstablishedConnection() callback to stop the timer. That should result
  // in a more robust, testable API.
  if ((*jobs_.begin())->HasEstablishedConnection())
    return;

  // If our old job is waiting on DNS, or if we can't create any sockets
  // right now due to limits, just reset the timer.
  if (client_socket_pool_->ReachedMaxSocketsLimit() ||
      !HasAvailableSocketSlot(client_socket_pool_->max_sockets_per_group_) ||
      (*jobs_.begin())->GetLoadState() == LOAD_STATE_RESOLVING_HOST) {
    StartBackupJobTimer(group_id);
    return;
  }

  if (unbound_requests_.empty())
    return;

  Request* request = unbound_requests_.FirstMax().value().get();
  std::unique_ptr<ConnectJob> owned_backup_job =
      client_socket_pool_->CreateConnectJob(
          group_id, request->socket_params(), client_socket_pool_->proxy_chain_,
          request->proxy_annotation_tag(), request->priority(),
          request->socket_tag(), this);
  owned_backup_job->net_log().AddEvent(
      NetLogEventType::SOCKET_POOL_CONNECT_JOB_CREATED, [&] {
        return NetLogCreateConnectJobParams(true /* backup_job */, &group_id_);
      });
  ConnectJob* backup_job = owned_backup_job.get();
  AddJob(std::move(owned_backup_job), false);
  client_socket_pool_->connecting_socket_count_++;
  int rv = backup_job->Connect();
  if (rv != ERR_IO_PENDING) {
    client_socket_pool_->OnConnectJobComplete(this, rv, backup_job);
  }
}

void TransportClientSocketPool::Group::SanityCheck() const {
#if DCHECK_IS_ON()
  DCHECK_LE(never_assigned_job_count(), jobs_.size());
  DCHECK_LE(unassigned_job_count(), jobs_.size());

  // Check that |unassigned_jobs_| is empty iff there are at least as many
  // requests as jobs.
  DCHECK_EQ(unassigned_jobs_.empty(), jobs_.size() <= unbound_requests_.size());

  size_t num_assigned_jobs = jobs_.size() - unassigned_jobs_.size();

  RequestQueue::Pointer pointer = unbound_requests_.FirstMax();
  for (size_t i = 0; i < unbound_requests_.size();
       ++i, pointer = unbound_requests_.GetNextTowardsLastMin(pointer)) {
    DCHECK(!pointer.is_null());
    DCHECK(pointer.value());
    // Check that the first |num_assigned_jobs| requests have valid job
    // assignments.
    if (i < num_assigned_jobs) {
      // The request has a job.
      ConnectJob* job = pointer.value()->job();
      DCHECK(job);
      // The request's job is not in |unassigned_jobs_|
      DCHECK(!base::Contains(unassigned_jobs_, job));
      // The request's job is in |jobs_|
      DCHECK(base::Contains(jobs_, job, &std::unique_ptr<ConnectJob>::get));
      // The same job is not assigned to any other request with a job.
      RequestQueue::Pointer pointer2 =
          unbound_requests_.GetNextTowardsLastMin(pointer);
      for (size_t j = i + 1; j < num_assigned_jobs;
           ++j, pointer2 = unbound_requests_.GetNextTowardsLastMin(pointer2)) {
        DCHECK(!pointer2.is_null());
        ConnectJob* job2 = pointer2.value()->job();
        DCHECK(job2);
        DCHECK_NE(job, job2);
      }
      DCHECK_EQ(pointer.value()->priority(), job->priority());
    } else {
      // Check that any subsequent requests do not have a job.
      DCHECK(!pointer.value()->job());
    }
  }

  for (auto it = unassigned_jobs_.begin(); it != unassigned_jobs_.end(); ++it) {
    // Check that all unassigned jobs are in |jobs_|
    ConnectJob* job = *it;
    DCHECK(base::Contains(jobs_, job, &std::unique_ptr<ConnectJob>::get));
    // Check that there are no duplicated entries in |unassigned_jobs_|
    for (auto it2 = std::next(it); it2 != unassigned_jobs_.end(); ++it2) {
      DCHECK_NE(job, *it2);
    }

    // Check that no |unassigned_jobs_| are in |bound_requests_|.
    DCHECK(!base::Contains(bound_requests_, job,
                           [](const BoundRequest& bound_request) {
                             return bound_request.connect_job.get();
                           }));
  }
#endif
}

void TransportClientSocketPool::Group::RemoveAllUnboundJobs() {
  SanityCheck();

  // Remove jobs from any requests that have them.
  if (!unbound_requests_.empty()) {
    for (RequestQueue::Pointer pointer = unbound_requests_.FirstMax();
         !pointer.is_null() && pointer.value()->job();
         pointer = unbound_requests_.GetNextTowardsLastMin(pointer)) {
      pointer.value()->ReleaseJob();
    }
  }
  unassigned_jobs_.clear();
  never_assigned_job_count_ = 0;

  // Diagnostics check for crbug.com/1231248. `Group`s are deleted only on
  // removal from `TransportClientSocketPool::group_map_`, so if this check
  // fails, `this` has been deleted, likely through some reentrancy issue.
  CHECK(client_socket_pool_->HasGroup(group_id_));

  // Delete active jobs.
  jobs_.clear();
  // Stop backup job timer.
  backup_job_timer_.Stop();

  SanityCheck();
}

size_t TransportClientSocketPool::Group::ConnectJobCount() const {
  return bound_requests_.size() + jobs_.size();
}

ConnectJob* TransportClientSocketPool::Group::GetConnectJobForHandle(
    const ClientSocketHandle* handle) const {
  // Search through bound requests for |handle|.
  for (const auto& bound_pair : bound_requests_) {
    if (handle == bound_pair.request->handle())
      return bound_pair.connect_job.get();
  }

  // Search through the unbound requests that have corresponding jobs for a
  // request with |handle|.
  for (RequestQueue::Pointer pointer = unbound_requests_.FirstMax();
       !pointer.is_null() && pointer.value()->job();
       pointer = unbound_requests_.GetNextTowardsLastMin(pointer)) {
    if (pointer.value()->handle() == handle)
      return pointer.value()->job();
  }

  return nullptr;
}

void TransportClientSocketPool::Group::InsertUnboundRequest(
    std::unique_ptr<Request> request) {
  SanityCheck();

  // Should not have a job because it is not already in |unbound_requests_|
  DCHECK(!request->job());
  // This value must be cached before we release |request|.
  RequestPriority priority = request->priority();

  RequestQueue::Pointer new_position;
  if (request->respect_limits() == RespectLimits::DISABLED) {
    // Put requests with RespectLimits::DISABLED (which should have
    // priority == MAXIMUM_PRIORITY) ahead of other requests with
    // MAXIMUM_PRIORITY.
    DCHECK_EQ(priority, MAXIMUM_PRIORITY);
    new_position =
        unbound_requests_.InsertAtFront(std::move(request), priority);
  } else {
    new_position = unbound_requests_.Insert(std::move(request), priority);
  }
  DCHECK(!unbound_requests_.empty());

  TryToAssignJobToRequest(new_position);

  SanityCheck();
}

const TransportClientSocketPool::Request*
TransportClientSocketPool::Group::GetNextUnboundRequest() const {
  return unbound_requests_.empty() ? nullptr
                                   : unbound_requests_.FirstMax().value().get();
}

std::unique_ptr<TransportClientSocketPool::Request>
TransportClientSocketPool::Group::PopNextUnboundRequest() {
  if (unbound_requests_.empty())
    return nullptr;
  return RemoveUnboundRequest(unbound_requests_.FirstMax());
}

std::unique_ptr<TransportClientSocketPool::Request>
TransportClientSocketPool::Group::FindAndRemoveUnboundRequest(
    ClientSocketHandle* handle) {
  for (RequestQueue::Pointer pointer = unbound_requests_.FirstMax();
       !pointer.is_null();
       pointer = unbound_requests_.GetNextTowardsLastMin(pointer)) {
    if (pointer.value()->handle() == handle) {
      DCHECK_EQ(static_cast<RequestPriority>(pointer.priority()),
                pointer.value()->priority());
      std::unique_ptr<Request> request = RemoveUnboundRequest(pointer);
      return request;
    }
  }
  return nullptr;
}

void TransportClientSocketPool::Group::SetPendingErrorForAllBoundRequests(
    int pending_error) {
  for (auto& bound_request : bound_requests_) {
    // Earlier errors take precedence.
    if (bound_request.pending_error == OK)
      bound_request.pending_error = pending_error;
  }
}

const TransportClientSocketPool::Request*
TransportClientSocketPool::Group::BindRequestToConnectJob(
    ConnectJob* connect_job) {
  // Check if |job| is already bound to a Request.
  for (const auto& bound_pair : bound_requests_) {
    if (bound_pair.connect_job.get() == connect_job)
      return bound_pair.request.get();
  }

  // If not, try to bind it to a Request.
  const Request* request = GetNextUnboundRequest();
  // If there are no pending requests, or the highest priority request has no
  // callback to handle auth challenges, return nullptr.
  if (!request || request->proxy_auth_callback().is_null())
    return nullptr;

  // Otherwise, bind the ConnectJob to the Request.
  std::unique_ptr<Request> owned_request = PopNextUnboundRequest();
  DCHECK_EQ(owned_request.get(), request);
  std::unique_ptr<ConnectJob> owned_connect_job = RemoveUnboundJob(connect_job);
  LogBoundConnectJobToRequest(owned_connect_job->net_log().source(), *request);
  bound_requests_.emplace_back(BoundRequest(
      std::move(owned_connect_job), std::move(owned_request), generation()));
  return request;
}

std::optional<TransportClientSocketPool::Group::BoundRequest>
TransportClientSocketPool::Group::FindAndRemoveBoundRequestForConnectJob(
    ConnectJob* connect_job) {
  for (auto bound_pair = bound_requests_.begin();
       bound_pair != bound_requests_.end(); ++bound_pair) {
    if (bound_pair->connect_job.get() != connect_job)
      continue;
    BoundRequest ret = std::move(*bound_pair);
    bound_requests_.erase(bound_pair);
    return std::move(ret);
  }
  return std::nullopt;
}

std::unique_ptr<TransportClientSocketPool::Request>
TransportClientSocketPool::Group::FindAndRemoveBoundRequest(
    ClientSocketHandle* client_socket_handle) {
  for (auto bound_pair = bound_requests_.begin();
       bound_pair != bound_requests_.end(); ++bound_pair) {
    if (bound_pair->request->handle() != client_socket_handle)
      continue;
    std::unique_ptr<Request> request = std::move(bound_pair->request);
    bound_requests_.erase(bound_pair);
    return request;
  }
  return nullptr;
}

void TransportClientSocketPool::Group::SetPriority(ClientSocketHandle* handle,
                                                   RequestPriority priority) {
  for (RequestQueue::Pointer pointer = unbound_requests_.FirstMax();
       !pointer.is_null();
       pointer = unbound_requests_.GetNextTowardsLastMin(pointer)) {
    if (pointer.value()->handle() == handle) {
      if (pointer.value()->priority() == priority)
        return;

      std::unique_ptr<Request> request = RemoveUnboundRequest(pointer);

      // Requests that ignore limits much be created and remain at the highest
      // priority, and should not be reprioritized.
      DCHECK_EQ(request->respect_limits(), RespectLimits::ENABLED);

      request->set_priority(priority);
      InsertUnboundRequest(std::move(request));
      return;
    }
  }

  // This function must be called with a valid ClientSocketHandle.
  NOTREACHED_IN_MIGRATION();
}

bool TransportClientSocketPool::Group::RequestWithHandleHasJobForTesting(
    const ClientSocketHandle* handle) const {
  SanityCheck();
  if (GetConnectJobForHandle(handle))
    return true;

  // There's no corresponding ConnectJob. Verify that the handle is at least
  // owned by a request.
  RequestQueue::Pointer pointer = unbound_requests_.FirstMax();
  for (size_t i = 0; i < unbound_requests_.size(); ++i) {
    if (pointer.value()->handle() == handle)
      return false;
    pointer = unbound_requests_.GetNextTowardsLastMin(pointer);
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

TransportClientSocketPool::Group::BoundRequest::BoundRequest()
    : pending_error(OK) {}

TransportClientSocketPool::Group::BoundRequest::BoundRequest(
    std::unique_ptr<ConnectJob> connect_job,
    std::unique_ptr<Request> request,
    int64_t generation)
    : connect_job(std::move(connect_job)),
      request(std::move(request)),
      generation(generation),
      pending_error(OK) {}

TransportClientSocketPool::Group::BoundRequest::BoundRequest(
    BoundRequest&& other) = default;

TransportClientSocketPool::Group::BoundRequest&
TransportClientSocketPool::Group::BoundRequest::operator=(
    BoundRequest&& other) = default;

TransportClientSocketPool::Group::BoundRequest::~BoundRequest() = default;

std::unique_ptr<TransportClientSocketPool::Request>
TransportClientSocketPool::Group::RemoveUnboundRequest(
    const RequestQueue::Pointer& pointer) {
  SanityCheck();

  std::unique_ptr<Request> request = unbound_requests_.Erase(pointer);
  if (request->job()) {
    TryToAssignUnassignedJob(request->ReleaseJob());
  }
  // If there are no more unbound requests, kill the backup timer.
  if (unbound_requests_.empty())
    backup_job_timer_.Stop();

  SanityCheck();
  return request;
}

TransportClientSocketPool::RequestQueue::Pointer
TransportClientSocketPool::Group::FindUnboundRequestWithJob(
    const ConnectJob* job) const {
  SanityCheck();

  for (RequestQueue::Pointer pointer = unbound_requests_.FirstMax();
       !pointer.is_null() && pointer.value()->job();
       pointer = unbound_requests_.GetNextTowardsLastMin(pointer)) {
    if (pointer.value()->job() == job)
      return pointer;
  }
  // If a request with the job was not found, it must be in |unassigned_jobs_|.
  DCHECK(base::Contains(unassigned_jobs_, job));
  return RequestQueue::Pointer();
}

TransportClientSocketPool::RequestQueue::Pointer
TransportClientSocketPool::Group::GetFirstRequestWithoutJob() const {
  RequestQueue::Pointer pointer = unbound_requests_.FirstMax();
  size_t i = 0;
  for (; !pointer.is_null() && pointer.value()->job();
       pointer = unbound_requests_.GetNextTowardsLastMin(pointer)) {
    ++i;
  }
  DCHECK_EQ(i, jobs_.size() - unassigned_jobs_.size());
  DCHECK(pointer.is_null() || !pointer.value()->job());
  return pointer;
}

void TransportClientSocketPool::Group::TryToAssignUnassignedJob(
    ConnectJob* job) {
  unassigned_jobs_.push_back(job);
  RequestQueue::Pointer first_request_without_job = GetFirstRequestWithoutJob();
  if (!first_request_without_job.is_null()) {
    first_request_without_job.value()->AssignJob(unassigned_jobs_.back());
    unassigned_jobs_.pop_back();
  }
}

void TransportClientSocketPool::Group::TryToAssignJobToRequest(
    TransportClientSocketPool::RequestQueue::Pointer request_pointer) {
  DCHECK(!request_pointer.value()->job());
  if (!unassigned_jobs_.empty()) {
    request_pointer.value()->AssignJob(unassigned_jobs_.front());
    unassigned_jobs_.pop_front();
    return;
  }

  // If the next request in the queue does not have a job, then there are no
  // requests with a job after |request_pointer| from which we can steal.
  RequestQueue::Pointer next_request =
      unbound_requests_.GetNextTowardsLastMin(request_pointer);
  if (next_request.is_null() || !next_request.value()->job())
    return;

  // Walk down the queue to find the last request with a job.
  RequestQueue::Pointer cur = next_request;
  RequestQueue::Pointer next = unbound_requests_.GetNextTowardsLastMin(cur);
  while (!next.is_null() && next.value()->job()) {
    cur = next;
    next = unbound_requests_.GetNextTowardsLastMin(next);
  }
  // Steal the job from the last request with a job.
  TransferJobBetweenRequests(cur.value().get(), request_pointer.value().get());
}

void TransportClientSocketPool::Group::TransferJobBetweenRequests(
    TransportClientSocketPool::Request* source,
    TransportClientSocketPool::Request* dest) {
  DCHECK(!dest->job());
  DCHECK(source->job());
  dest->AssignJob(source->ReleaseJob());
}

}  // namespace net
