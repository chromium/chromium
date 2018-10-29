// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/client_socket_pool_base.h"

#include <algorithm>
#include <utility>

#include "base/compiler_specific.h"
#include "base/format_macros.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "net/base/net_errors.h"
#include "net/base/trace_constants.h"
#include "net/log/net_log.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source.h"

using base::TimeDelta;

namespace net {

namespace {

// Indicate whether or not we should establish a new transport layer connection
// after a certain timeout has passed without receiving an ACK.
bool g_connect_backup_jobs_enabled = true;

}  // namespace

ConnectJob::ConnectJob(const std::string& group_name,
                       base::TimeDelta timeout_duration,
                       RequestPriority priority,
                       const SocketTag& socket_tag,
                       ClientSocketPool::RespectLimits respect_limits,
                       Delegate* delegate,
                       const NetLogWithSource& net_log)
    : group_name_(group_name),
      timeout_duration_(timeout_duration),
      priority_(priority),
      socket_tag_(socket_tag),
      respect_limits_(respect_limits),
      delegate_(delegate),
      net_log_(net_log),
      idle_(true) {
  DCHECK(!group_name.empty());
  DCHECK(delegate);
  net_log.BeginEvent(NetLogEventType::SOCKET_POOL_CONNECT_JOB,
                     NetLog::StringCallback("group_name", &group_name_));
}

ConnectJob::~ConnectJob() {
  net_log().EndEvent(NetLogEventType::SOCKET_POOL_CONNECT_JOB);
}

std::unique_ptr<StreamSocket> ConnectJob::PassSocket() {
  return std::move(socket_);
}

int ConnectJob::Connect() {
  if (!timeout_duration_.is_zero())
    timer_.Start(FROM_HERE, timeout_duration_, this, &ConnectJob::OnTimeout);

  idle_ = false;

  LogConnectStart();

  int rv = ConnectInternal();

  if (rv != ERR_IO_PENDING) {
    LogConnectCompletion(rv);
    delegate_ = NULL;
  }

  return rv;
}

void ConnectJob::SetSocket(std::unique_ptr<StreamSocket> socket) {
  if (socket) {
    net_log().AddEvent(NetLogEventType::CONNECT_JOB_SET_SOCKET,
                       socket->NetLog().source().ToEventParametersCallback());
  }
  socket_ = std::move(socket);
}

void ConnectJob::NotifyDelegateOfCompletion(int rv) {
  TRACE_EVENT0(kNetTracingCategory, "ConnectJob::NotifyDelegateOfCompletion");
  // The delegate will own |this|.
  Delegate* delegate = delegate_;
  delegate_ = NULL;

  LogConnectCompletion(rv);
  delegate->OnConnectJobComplete(rv, this);
}

void ConnectJob::ResetTimer(base::TimeDelta remaining_time) {
  timer_.Stop();
  timer_.Start(FROM_HERE, remaining_time, this, &ConnectJob::OnTimeout);
}

void ConnectJob::LogConnectStart() {
  connect_timing_.connect_start = base::TimeTicks::Now();
  net_log().BeginEvent(NetLogEventType::SOCKET_POOL_CONNECT_JOB_CONNECT);
}

void ConnectJob::LogConnectCompletion(int net_error) {
  connect_timing_.connect_end = base::TimeTicks::Now();
  net_log().EndEventWithNetErrorCode(
      NetLogEventType::SOCKET_POOL_CONNECT_JOB_CONNECT, net_error);
}

void ConnectJob::OnTimeout() {
  // Make sure the socket is NULL before calling into |delegate|.
  SetSocket(std::unique_ptr<StreamSocket>());

  net_log_.AddEvent(NetLogEventType::SOCKET_POOL_CONNECT_JOB_TIMED_OUT);

  NotifyDelegateOfCompletion(ERR_TIMED_OUT);
}

namespace internal {

ClientSocketPoolBaseHelper::Request::Request(
    ClientSocketHandle* handle,
    CompletionOnceCallback callback,
    RequestPriority priority,
    const SocketTag& socket_tag,
    ClientSocketPool::RespectLimits respect_limits,
    Flags flags,
    const NetLogWithSource& net_log)
    : handle_(handle),
      callback_(std::move(callback)),
      priority_(priority),
      respect_limits_(respect_limits),
      flags_(flags),
      net_log_(net_log),
      socket_tag_(socket_tag) {
  if (respect_limits_ == ClientSocketPool::RespectLimits::DISABLED)
    DCHECK_EQ(priority_, MAXIMUM_PRIORITY);
}

ClientSocketPoolBaseHelper::Request::~Request() {
  liveness_ = DEAD;
}

void ClientSocketPoolBaseHelper::Request::CrashIfInvalid() const {
  CHECK_EQ(liveness_, ALIVE);
}

ClientSocketPoolBaseHelper::ClientSocketPoolBaseHelper(
    HigherLayeredPool* pool,
    int max_sockets,
    int max_sockets_per_group,
    base::TimeDelta unused_idle_socket_timeout,
    base::TimeDelta used_idle_socket_timeout,
    ConnectJobFactory* connect_job_factory)
    : idle_socket_count_(0),
      connecting_socket_count_(0),
      handed_out_socket_count_(0),
      max_sockets_(max_sockets),
      max_sockets_per_group_(max_sockets_per_group),
      unused_idle_socket_timeout_(unused_idle_socket_timeout),
      used_idle_socket_timeout_(used_idle_socket_timeout),
      connect_job_factory_(connect_job_factory),
      connect_backup_jobs_enabled_(false),
      pool_generation_number_(0),
      pool_(pool),
      weak_factory_(this) {
  DCHECK_LE(0, max_sockets_per_group);
  DCHECK_LE(max_sockets_per_group, max_sockets);

  NetworkChangeNotifier::AddIPAddressObserver(this);
}

ClientSocketPoolBaseHelper::~ClientSocketPoolBaseHelper() {
  // Clean up any idle sockets and pending connect jobs.  Assert that we have no
  // remaining active sockets or pending requests.  They should have all been
  // cleaned up prior to |this| being destroyed.
  FlushWithError(ERR_ABORTED);
  DCHECK(group_map_.empty());
  DCHECK(pending_callback_map_.empty());
  DCHECK_EQ(0, connecting_socket_count_);
  CHECK(higher_pools_.empty());

  NetworkChangeNotifier::RemoveIPAddressObserver(this);

  // Remove from lower layer pools.
  for (auto it = lower_pools_.begin(); it != lower_pools_.end(); ++it) {
    (*it)->RemoveHigherLayeredPool(pool_);
  }
}

ClientSocketPoolBaseHelper::CallbackResultPair::CallbackResultPair()
    : result(OK) {
}

ClientSocketPoolBaseHelper::CallbackResultPair::CallbackResultPair(
    CompletionOnceCallback callback_in,
    int result_in)
    : callback(std::move(callback_in)), result(result_in) {}

ClientSocketPoolBaseHelper::CallbackResultPair::CallbackResultPair(
    ClientSocketPoolBaseHelper::CallbackResultPair&& other) = default;

ClientSocketPoolBaseHelper::CallbackResultPair&
ClientSocketPoolBaseHelper::CallbackResultPair::operator=(
    ClientSocketPoolBaseHelper::CallbackResultPair&& other) = default;

ClientSocketPoolBaseHelper::CallbackResultPair::~CallbackResultPair() = default;

bool ClientSocketPoolBaseHelper::IsStalled() const {
  // If a lower layer pool is stalled, consider |this| stalled as well.
  for (auto it = lower_pools_.begin(); it != lower_pools_.end(); ++it) {
    if ((*it)->IsStalled())
      return true;
  }

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
  for (auto it = group_map_.begin(); it != group_map_.end(); ++it) {
    if (it->second->CanUseAdditionalSocketSlot(max_sockets_per_group_))
      return true;
  }
  return false;
}

void ClientSocketPoolBaseHelper::AddLowerLayeredPool(
    LowerLayeredPool* lower_pool) {
  DCHECK(pool_);
  CHECK(!base::ContainsKey(lower_pools_, lower_pool));
  lower_pools_.insert(lower_pool);
  lower_pool->AddHigherLayeredPool(pool_);
}

void ClientSocketPoolBaseHelper::AddHigherLayeredPool(
    HigherLayeredPool* higher_pool) {
  CHECK(higher_pool);
  CHECK(!base::ContainsKey(higher_pools_, higher_pool));
  higher_pools_.insert(higher_pool);
}

void ClientSocketPoolBaseHelper::RemoveHigherLayeredPool(
    HigherLayeredPool* higher_pool) {
  CHECK(higher_pool);
  CHECK(base::ContainsKey(higher_pools_, higher_pool));
  higher_pools_.erase(higher_pool);
}

int ClientSocketPoolBaseHelper::RequestSocket(
    const std::string& group_name,
    std::unique_ptr<Request> request) {
  CHECK(request->has_callback());
  CHECK(request->handle());

  // Cleanup any timed-out idle sockets.
  CleanupIdleSockets(false);

  request->net_log().BeginEvent(NetLogEventType::SOCKET_POOL);

  int rv = RequestSocketInternal(group_name, *request);
  if (rv != ERR_IO_PENDING) {
    if (rv == OK) {
      request->handle()->socket()->ApplySocketTag(request->socket_tag());
    }
    request->net_log().EndEventWithNetErrorCode(NetLogEventType::SOCKET_POOL,
                                                rv);
    CHECK(!request->handle()->is_initialized());
    request.reset();
  } else {
    Group* group = GetOrCreateGroup(group_name);
    group->InsertPendingRequest(std::move(request));
    // Have to do this asynchronously, as closing sockets in higher level pools
    // call back in to |this|, which will cause all sorts of fun and exciting
    // re-entrancy issues if the socket pool is doing something else at the
    // time.
    if (group->CanUseAdditionalSocketSlot(max_sockets_per_group_)) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::Bind(
              &ClientSocketPoolBaseHelper::TryToCloseSocketsInLayeredPools,
              weak_factory_.GetWeakPtr()));
    }
  }
  return rv;
}

void ClientSocketPoolBaseHelper::RequestSockets(const std::string& group_name,
                                                const Request& request,
                                                int num_sockets) {
  DCHECK(!request.has_callback());
  DCHECK(!request.handle());

  // Cleanup any timed-out idle sockets.
  CleanupIdleSockets(false);

  if (num_sockets > max_sockets_per_group_) {
    num_sockets = max_sockets_per_group_;
  }

  request.net_log().BeginEvent(
      NetLogEventType::SOCKET_POOL_CONNECTING_N_SOCKETS,
      NetLog::IntCallback("num_sockets", num_sockets));

  Group* group = GetOrCreateGroup(group_name);

  // RequestSocketsInternal() may delete the group.
  bool deleted_group = false;

  int rv = OK;
  for (int num_iterations_left = num_sockets;
       group->NumActiveSocketSlots() < num_sockets &&
       num_iterations_left > 0 ; num_iterations_left--) {
    rv = RequestSocketInternal(group_name, request);
    if (rv < 0 && rv != ERR_IO_PENDING) {
      // We're encountering a synchronous error.  Give up.
      if (!base::ContainsKey(group_map_, group_name))
        deleted_group = true;
      break;
    }
    if (!base::ContainsKey(group_map_, group_name)) {
      // Unexpected.  The group should only be getting deleted on synchronous
      // error.
      NOTREACHED();
      deleted_group = true;
      break;
    }
  }

  if (!deleted_group && group->IsEmpty())
    RemoveGroup(group_name);

  if (rv == ERR_IO_PENDING)
    rv = OK;
  request.net_log().EndEventWithNetErrorCode(
      NetLogEventType::SOCKET_POOL_CONNECTING_N_SOCKETS, rv);
}

int ClientSocketPoolBaseHelper::RequestSocketInternal(
    const std::string& group_name,
    const Request& request) {
  ClientSocketHandle* const handle = request.handle();
  const bool preconnecting = !handle;

  Group* group = nullptr;
  auto group_it = group_map_.find(group_name);
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
    if (!preconnecting && group->TryToUseUnassignedConnectJob())
      return ERR_IO_PENDING;

    // Can we make another active socket now?
    if (!group->HasAvailableSocketSlot(max_sockets_per_group_) &&
        request.respect_limits() == ClientSocketPool::RespectLimits::ENABLED) {
      // TODO(willchan): Consider whether or not we need to close a socket in a
      // higher layered group. I don't think this makes sense since we would
      // just reuse that socket then if we needed one and wouldn't make it down
      // to this layer.
      request.net_log().AddEvent(
          NetLogEventType::SOCKET_POOL_STALLED_MAX_SOCKETS_PER_GROUP);
      return ERR_IO_PENDING;
    }
  }

  if (ReachedMaxSocketsLimit() &&
      request.respect_limits() == ClientSocketPool::RespectLimits::ENABLED) {
    // NOTE(mmenke):  Wonder if we really need different code for each case
    // here.  Only reason for them now seems to be preconnects.
    if (idle_socket_count() > 0) {
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
      return ERR_IO_PENDING;
    }
  }

  // We couldn't find a socket to reuse, and there's space to allocate one,
  // so allocate and connect a new one.
  std::unique_ptr<ConnectJob> connect_job(
      connect_job_factory_->NewConnectJob(group_name, request, this));

  int rv = connect_job->Connect();
  if (rv == OK) {
    LogBoundConnectJobToRequest(connect_job->net_log().source(), request);
    if (!preconnecting) {
      HandOutSocket(connect_job->PassSocket(), ClientSocketHandle::UNUSED,
                    connect_job->connect_timing(), handle, base::TimeDelta(),
                    GetOrCreateGroup(group_name), request.net_log());
    } else {
      AddIdleSocket(connect_job->PassSocket(), GetOrCreateGroup(group_name));
    }
  } else if (rv == ERR_IO_PENDING) {
    // If we don't have any sockets in this group, set a timer for potentially
    // creating a new one.  If the SYN is lost, this backup socket may complete
    // before the slow socket, improving end user latency.
    Group* group = GetOrCreateGroup(group_name);
    if (connect_backup_jobs_enabled_ && group->IsEmpty()) {
      group->StartBackupJobTimer(group_name, this);
    }

    connecting_socket_count_++;

    group->AddJob(std::move(connect_job), preconnecting);
  } else {
    LogBoundConnectJobToRequest(connect_job->net_log().source(), request);
    std::unique_ptr<StreamSocket> error_socket;
    if (!preconnecting) {
      DCHECK(handle);
      connect_job->GetAdditionalErrorState(handle);
      error_socket = connect_job->PassSocket();
    }
    Group* group = GetOrCreateGroup(group_name);
    if (error_socket) {
      HandOutSocket(std::move(error_socket), ClientSocketHandle::UNUSED,
                    connect_job->connect_timing(), handle, base::TimeDelta(),
                    group, request.net_log());
    } else if (group->IsEmpty()) {
      RemoveGroup(group_name);
    }
  }

  return rv;
}

bool ClientSocketPoolBaseHelper::AssignIdleSocketToRequest(
    const Request& request, Group* group) {
  std::list<IdleSocket>* idle_sockets = group->mutable_idle_sockets();
  auto idle_socket_it = idle_sockets->end();

  // Iterate through the idle sockets forwards (oldest to newest)
  //   * Delete any disconnected ones.
  //   * If we find a used idle socket, assign to |idle_socket|.  At the end,
  //   the |idle_socket_it| will be set to the newest used idle socket.
  for (auto it = idle_sockets->begin(); it != idle_sockets->end();) {
    // Check whether socket is usable. Note that it's unlikely that the socket
    // is not usuable because this function is always invoked after a
    // reusability check, but in theory socket can be closed asynchronously.
    if (!it->IsUsable()) {
      DecrementIdleCount();
      delete it->socket;
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
    IdleSocket idle_socket = *idle_socket_it;
    idle_sockets->erase(idle_socket_it);
    // TODO(davidben): If |idle_time| is under some low watermark, consider
    // treating as UNUSED rather than UNUSED_IDLE. This will avoid
    // HttpNetworkTransaction retrying on some errors.
    ClientSocketHandle::SocketReuseType reuse_type =
        idle_socket.socket->WasEverUsed() ?
            ClientSocketHandle::REUSED_IDLE :
            ClientSocketHandle::UNUSED_IDLE;

    // If this socket took multiple attempts to obtain, don't report those
    // every time it's reused, just to the first user.
    if (idle_socket.socket->WasEverUsed())
      idle_socket.socket->ClearConnectionAttempts();

    HandOutSocket(std::unique_ptr<StreamSocket>(idle_socket.socket), reuse_type,
                  LoadTimingInfo::ConnectTiming(), request.handle(), idle_time,
                  group, request.net_log());
    return true;
  }

  return false;
}

// static
void ClientSocketPoolBaseHelper::LogBoundConnectJobToRequest(
    const NetLogSource& connect_job_source,
    const Request& request) {
  request.net_log().AddEvent(NetLogEventType::SOCKET_POOL_BOUND_TO_CONNECT_JOB,
                             connect_job_source.ToEventParametersCallback());
}

void ClientSocketPoolBaseHelper::SetPriority(const std::string& group_name,
                                             ClientSocketHandle* handle,
                                             RequestPriority priority) {
  auto group_it = group_map_.find(group_name);
  if (group_it == group_map_.end()) {
    DCHECK(base::ContainsKey(pending_callback_map_, handle));
    // The Request has already completed and been destroyed; nothing to
    // reprioritize.
    return;
  }

  group_it->second->SetPriority(handle, priority);
}

void ClientSocketPoolBaseHelper::CancelRequest(
    const std::string& group_name, ClientSocketHandle* handle) {
  auto callback_it = pending_callback_map_.find(handle);
  if (callback_it != pending_callback_map_.end()) {
    int result = callback_it->second.result;
    pending_callback_map_.erase(callback_it);
    std::unique_ptr<StreamSocket> socket = handle->PassSocket();
    if (socket) {
      if (result != OK)
        socket->Disconnect();
      ReleaseSocket(handle->group_name(), std::move(socket), handle->id());
    }
    return;
  }

  CHECK(base::ContainsKey(group_map_, group_name));

  Group* group = GetOrCreateGroup(group_name);

  // Search pending_requests for matching handle.
  std::unique_ptr<Request> request = group->FindAndRemovePendingRequest(handle);
  if (request) {
    request->net_log().AddEvent(NetLogEventType::CANCELLED);
    request->net_log().EndEvent(NetLogEventType::SOCKET_POOL);

    // We let the job run, unless we're at the socket limit and there is
    // not another request waiting on the job.
    if (group->jobs().size() > group->pending_request_count() &&
        ReachedMaxSocketsLimit()) {
      RemoveConnectJob(group->jobs().begin()->get(), group);
      CheckForStalledSocketGroups();
    }
  }
}

bool ClientSocketPoolBaseHelper::HasGroup(const std::string& group_name) const {
  return base::ContainsKey(group_map_, group_name);
}

void ClientSocketPoolBaseHelper::CloseIdleSockets() {
  CleanupIdleSockets(true);
  DCHECK_EQ(0, idle_socket_count_);
}

void ClientSocketPoolBaseHelper::CloseIdleSocketsInGroup(
    const std::string& group_name) {
  if (idle_socket_count_ == 0)
    return;
  auto it = group_map_.find(group_name);
  if (it == group_map_.end())
    return;
  CleanupIdleSocketsInGroup(true, it->second, base::TimeTicks::Now());
  if (it->second->IsEmpty())
    RemoveGroup(it);
}

int ClientSocketPoolBaseHelper::IdleSocketCountInGroup(
    const std::string& group_name) const {
  auto i = group_map_.find(group_name);
  CHECK(i != group_map_.end());

  return i->second->idle_sockets().size();
}

LoadState ClientSocketPoolBaseHelper::GetLoadState(
    const std::string& group_name,
    const ClientSocketHandle* handle) const {
  if (base::ContainsKey(pending_callback_map_, handle))
    return LOAD_STATE_CONNECTING;

  auto group_it = group_map_.find(group_name);
  if (group_it == group_map_.end()) {
    // TODO(mmenke):  This is actually reached in the wild, for unknown reasons.
    // Would be great to understand why, and if it's a bug, fix it.  If not,
    // should have a test for that case.
    NOTREACHED();
    return LOAD_STATE_IDLE;
  }

  const Group& group = *group_it->second;
  if (group.HasConnectJobForHandle(handle)) {
    // Just return the state of the oldest ConnectJob.
    return (*group.jobs().begin())->GetLoadState();
  }

  if (group.CanUseAdditionalSocketSlot(max_sockets_per_group_))
    return LOAD_STATE_WAITING_FOR_STALLED_SOCKET_POOL;
  return LOAD_STATE_WAITING_FOR_AVAILABLE_SOCKET;
}

std::unique_ptr<base::DictionaryValue>
ClientSocketPoolBaseHelper::GetInfoAsValue(const std::string& name,
                                           const std::string& type) const {
  auto dict = std::make_unique<base::DictionaryValue>();
  dict->SetString("name", name);
  dict->SetString("type", type);
  dict->SetInteger("handed_out_socket_count", handed_out_socket_count_);
  dict->SetInteger("connecting_socket_count", connecting_socket_count_);
  dict->SetInteger("idle_socket_count", idle_socket_count_);
  dict->SetInteger("max_socket_count", max_sockets_);
  dict->SetInteger("max_sockets_per_group", max_sockets_per_group_);
  dict->SetInteger("pool_generation_number", pool_generation_number_);

  if (group_map_.empty())
    return dict;

  auto all_groups_dict = std::make_unique<base::DictionaryValue>();
  for (auto it = group_map_.begin(); it != group_map_.end(); it++) {
    const Group* group = it->second;
    auto group_dict = std::make_unique<base::DictionaryValue>();

    group_dict->SetInteger("pending_request_count",
                           group->pending_request_count());
    if (group->has_pending_requests()) {
      group_dict->SetString(
          "top_pending_priority",
          RequestPriorityToString(group->TopPendingPriority()));
    }

    group_dict->SetInteger("active_socket_count", group->active_socket_count());

    auto idle_socket_list = std::make_unique<base::ListValue>();
    std::list<IdleSocket>::const_iterator idle_socket;
    for (idle_socket = group->idle_sockets().begin();
         idle_socket != group->idle_sockets().end();
         idle_socket++) {
      int source_id = idle_socket->socket->NetLog().source().id;
      idle_socket_list->AppendInteger(source_id);
    }
    group_dict->Set("idle_sockets", std::move(idle_socket_list));

    auto connect_jobs_list = std::make_unique<base::ListValue>();
    for (auto job = group->jobs().begin(); job != group->jobs().end(); job++) {
      int source_id = (*job)->net_log().source().id;
      connect_jobs_list->AppendInteger(source_id);
    }
    group_dict->Set("connect_jobs", std::move(connect_jobs_list));

    group_dict->SetBoolean("is_stalled", group->CanUseAdditionalSocketSlot(
                                             max_sockets_per_group_));
    group_dict->SetBoolean("backup_job_timer_is_running",
                           group->BackupJobTimerIsRunning());

    all_groups_dict->SetWithoutPathExpansion(it->first, std::move(group_dict));
  }
  dict->Set("groups", std::move(all_groups_dict));
  return dict;
}

void ClientSocketPoolBaseHelper::DumpMemoryStats(
    base::trace_event::ProcessMemoryDump* pmd,
    const std::string& parent_dump_absolute_name) const {
  size_t socket_count = 0;
  size_t total_size = 0;
  size_t buffer_size = 0;
  size_t cert_count = 0;
  size_t cert_size = 0;
  for (const auto& kv : group_map_) {
    for (const auto& socket : kv.second->idle_sockets()) {
      StreamSocket::SocketMemoryStats stats;
      socket.socket->DumpMemoryStats(&stats);
      total_size += stats.total_size;
      buffer_size += stats.buffer_size;
      cert_count += stats.cert_count;
      cert_size += stats.cert_size;
      ++socket_count;
    }
  }
  // Only create a MemoryAllocatorDump if there is at least one idle socket
  if (socket_count > 0) {
    base::trace_event::MemoryAllocatorDump* socket_pool_dump =
        pmd->CreateAllocatorDump(base::StringPrintf(
            "%s/socket_pool", parent_dump_absolute_name.c_str()));
    socket_pool_dump->AddScalar(
        base::trace_event::MemoryAllocatorDump::kNameSize,
        base::trace_event::MemoryAllocatorDump::kUnitsBytes, total_size);
    socket_pool_dump->AddScalar(
        base::trace_event::MemoryAllocatorDump::kNameObjectCount,
        base::trace_event::MemoryAllocatorDump::kUnitsObjects, socket_count);
    socket_pool_dump->AddScalar(
        "buffer_size", base::trace_event::MemoryAllocatorDump::kUnitsBytes,
        buffer_size);
    socket_pool_dump->AddScalar(
        "cert_count", base::trace_event::MemoryAllocatorDump::kUnitsObjects,
        cert_count);
    socket_pool_dump->AddScalar(
        "cert_size", base::trace_event::MemoryAllocatorDump::kUnitsBytes,
        cert_size);
  }
}

bool ClientSocketPoolBaseHelper::IdleSocket::IsUsable() const {
  if (socket->WasEverUsed())
    return socket->IsConnectedAndIdle();
  return socket->IsConnected();
}

void ClientSocketPoolBaseHelper::CleanupIdleSockets(bool force) {
  if (idle_socket_count_ == 0)
    return;

  // Current time value. Retrieving it once at the function start rather than
  // inside the inner loop, since it shouldn't change by any meaningful amount.
  base::TimeTicks now = base::TimeTicks::Now();

  for (auto i = group_map_.begin(); i != group_map_.end();) {
    Group* group = i->second;
    CleanupIdleSocketsInGroup(force, group, now);
    // Delete group if no longer needed.
    if (group->IsEmpty()) {
      auto old = i++;
      RemoveGroup(old);
    } else {
      ++i;
    }
  }
}

void ClientSocketPoolBaseHelper::CleanupIdleSocketsInGroup(
    bool force,
    Group* group,
    const base::TimeTicks& now) {
  auto idle_socket_it = group->mutable_idle_sockets()->begin();
  while (idle_socket_it != group->idle_sockets().end()) {
    base::TimeDelta timeout = idle_socket_it->socket->WasEverUsed()
                                  ? used_idle_socket_timeout_
                                  : unused_idle_socket_timeout_;
    bool timed_out = (now - idle_socket_it->start_time) >= timeout;
    bool should_clean_up = force || timed_out || !idle_socket_it->IsUsable();
    if (should_clean_up) {
      delete idle_socket_it->socket;
      idle_socket_it = group->mutable_idle_sockets()->erase(idle_socket_it);
      DecrementIdleCount();
    } else {
      ++idle_socket_it;
    }
  }
}

ClientSocketPoolBaseHelper::Group* ClientSocketPoolBaseHelper::GetOrCreateGroup(
    const std::string& group_name) {
  auto it = group_map_.find(group_name);
  if (it != group_map_.end())
    return it->second;
  Group* group = new Group;
  group_map_[group_name] = group;
  return group;
}

void ClientSocketPoolBaseHelper::RemoveGroup(const std::string& group_name) {
  auto it = group_map_.find(group_name);
  CHECK(it != group_map_.end());

  RemoveGroup(it);
}

void ClientSocketPoolBaseHelper::RemoveGroup(GroupMap::iterator it) {
  delete it->second;
  group_map_.erase(it);
}

// static
bool ClientSocketPoolBaseHelper::connect_backup_jobs_enabled() {
  return g_connect_backup_jobs_enabled;
}

// static
bool ClientSocketPoolBaseHelper::set_connect_backup_jobs_enabled(bool enabled) {
  bool old_value = g_connect_backup_jobs_enabled;
  g_connect_backup_jobs_enabled = enabled;
  return old_value;
}

void ClientSocketPoolBaseHelper::EnableConnectBackupJobs() {
  connect_backup_jobs_enabled_ = g_connect_backup_jobs_enabled;
}

void ClientSocketPoolBaseHelper::IncrementIdleCount() {
  ++idle_socket_count_;
}

void ClientSocketPoolBaseHelper::DecrementIdleCount() {
  --idle_socket_count_;
}

void ClientSocketPoolBaseHelper::ReleaseSocket(
    const std::string& group_name,
    std::unique_ptr<StreamSocket> socket,
    int id) {
  auto i = group_map_.find(group_name);
  CHECK(i != group_map_.end());

  Group* group = i->second;

  CHECK_GT(handed_out_socket_count_, 0);
  handed_out_socket_count_--;

  CHECK_GT(group->active_socket_count(), 0);
  group->DecrementActiveSocketCount();

  const bool can_reuse = socket->IsConnectedAndIdle() &&
      id == pool_generation_number_;
  if (can_reuse) {
    // Add it to the idle list.
    AddIdleSocket(std::move(socket), group);
    OnAvailableSocketSlot(group_name, group);
  } else {
    socket.reset();
  }

  CheckForStalledSocketGroups();
}

void ClientSocketPoolBaseHelper::CheckForStalledSocketGroups() {
  // Loop until there's nothing more to do.
  while (true) {
    // If we have idle sockets, see if we can give one to the top-stalled group.
    std::string top_group_name;
    Group* top_group = NULL;
    if (!FindTopStalledGroup(&top_group, &top_group_name)) {
      // There may still be a stalled group in a lower level pool.
      for (auto it = lower_pools_.begin(); it != lower_pools_.end(); ++it) {
        if ((*it)->IsStalled()) {
          CloseOneIdleSocket();
          break;
        }
      }
      return;
    }

    if (ReachedMaxSocketsLimit()) {
      if (idle_socket_count() > 0) {
        CloseOneIdleSocket();
      } else {
        // We can't activate more sockets since we're already at our global
        // limit.
        return;
      }
    }

    // Note that this may delete top_group.
    OnAvailableSocketSlot(top_group_name, top_group);
  }
}

// Search for the highest priority pending request, amongst the groups that
// are not at the |max_sockets_per_group_| limit. Note: for requests with
// the same priority, the winner is based on group hash ordering (and not
// insertion order).
bool ClientSocketPoolBaseHelper::FindTopStalledGroup(
    Group** group,
    std::string* group_name) const {
  CHECK((group && group_name) || (!group && !group_name));
  Group* top_group = NULL;
  const std::string* top_group_name = NULL;
  bool has_stalled_group = false;
  for (auto i = group_map_.begin(); i != group_map_.end(); ++i) {
    Group* curr_group = i->second;
    if (!curr_group->has_pending_requests())
      continue;
    if (curr_group->CanUseAdditionalSocketSlot(max_sockets_per_group_)) {
      if (!group)
        return true;
      has_stalled_group = true;
      bool has_higher_priority = !top_group ||
          curr_group->TopPendingPriority() > top_group->TopPendingPriority();
      if (has_higher_priority) {
        top_group = curr_group;
        top_group_name = &i->first;
      }
    }
  }

  if (top_group) {
    CHECK(group);
    *group = top_group;
    *group_name = *top_group_name;
  } else {
    CHECK(!has_stalled_group);
  }
  return has_stalled_group;
}

void ClientSocketPoolBaseHelper::OnConnectJobComplete(
    int result, ConnectJob* job) {
  DCHECK_NE(ERR_IO_PENDING, result);
  const std::string group_name = job->group_name();
  auto group_it = group_map_.find(group_name);
  CHECK(group_it != group_map_.end());
  Group* group = group_it->second;

  std::unique_ptr<StreamSocket> socket = job->PassSocket();

  // Copies of these are needed because |job| may be deleted before they are
  // accessed.
  NetLogWithSource job_log = job->net_log();
  LoadTimingInfo::ConnectTiming connect_timing = job->connect_timing();

  // RemoveConnectJob(job, _) must be called by all branches below;
  // otherwise, |job| will be leaked.

  if (result == OK) {
    DCHECK(socket.get());
    RemoveConnectJob(job, group);
    std::unique_ptr<Request> request = group->PopNextPendingRequest();
    if (request) {
      LogBoundConnectJobToRequest(job_log.source(), *request);
      HandOutSocket(std::move(socket), ClientSocketHandle::UNUSED,
                    connect_timing, request->handle(), base::TimeDelta(), group,
                    request->net_log());
      request->net_log().EndEvent(NetLogEventType::SOCKET_POOL);
      InvokeUserCallbackLater(request->handle(), request->release_callback(),
                              result, request->socket_tag());
    } else {
      AddIdleSocket(std::move(socket), group);
      OnAvailableSocketSlot(group_name, group);
      CheckForStalledSocketGroups();
    }
  } else {
    // If we got a socket, it must contain error information so pass that
    // up so that the caller can retrieve it.
    bool handed_out_socket = false;
    std::unique_ptr<Request> request = group->PopNextPendingRequest();
    if (request) {
      LogBoundConnectJobToRequest(job_log.source(), *request);
      job->GetAdditionalErrorState(request->handle());
      RemoveConnectJob(job, group);
      if (socket.get()) {
        handed_out_socket = true;
        HandOutSocket(std::move(socket), ClientSocketHandle::UNUSED,
                      connect_timing, request->handle(), base::TimeDelta(),
                      group, request->net_log());
      }
      request->net_log().EndEventWithNetErrorCode(NetLogEventType::SOCKET_POOL,
                                                  result);
      InvokeUserCallbackLater(request->handle(), request->release_callback(),
                              result, request->socket_tag());
    } else {
      RemoveConnectJob(job, group);
    }
    if (!handed_out_socket) {
      OnAvailableSocketSlot(group_name, group);
      CheckForStalledSocketGroups();
    }
  }
}

void ClientSocketPoolBaseHelper::OnIPAddressChanged() {
  FlushWithError(ERR_NETWORK_CHANGED);
}

void ClientSocketPoolBaseHelper::FlushWithError(int error) {
  pool_generation_number_++;
  CancelAllConnectJobs();
  CloseIdleSockets();
  CancelAllRequestsWithError(error);
}

void ClientSocketPoolBaseHelper::RemoveConnectJob(ConnectJob* job,
                                                  Group* group) {
  CHECK_GT(connecting_socket_count_, 0);
  connecting_socket_count_--;

  DCHECK(group);
  group->RemoveJob(job);
}

void ClientSocketPoolBaseHelper::OnAvailableSocketSlot(
    const std::string& group_name, Group* group) {
  DCHECK(base::ContainsKey(group_map_, group_name));
  if (group->IsEmpty()) {
    RemoveGroup(group_name);
  } else if (group->has_pending_requests()) {
    ProcessPendingRequest(group_name, group);
  }
}

void ClientSocketPoolBaseHelper::ProcessPendingRequest(
    const std::string& group_name, Group* group) {
  const Request* next_request = group->GetNextPendingRequest();
  DCHECK(next_request);

  // If the group has no idle sockets, and can't make use of an additional slot,
  // either because it's at the limit or because it's at the socket per group
  // limit, then there's nothing to do.
  if (group->idle_sockets().empty() &&
      !group->CanUseAdditionalSocketSlot(max_sockets_per_group_)) {
    return;
  }

  int rv = RequestSocketInternal(group_name, *next_request);
  if (rv != ERR_IO_PENDING) {
    std::unique_ptr<Request> request = group->PopNextPendingRequest();
    DCHECK(request);
    if (group->IsEmpty())
      RemoveGroup(group_name);

    request->net_log().EndEventWithNetErrorCode(NetLogEventType::SOCKET_POOL,
                                                rv);
    InvokeUserCallbackLater(request->handle(), request->release_callback(), rv,
                            request->socket_tag());
  }
}

void ClientSocketPoolBaseHelper::HandOutSocket(
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
  handle->set_pool_id(pool_generation_number_);
  handle->set_connect_timing(connect_timing);

  if (reuse_type == ClientSocketHandle::REUSED_IDLE) {
    net_log.AddEvent(
        NetLogEventType::SOCKET_POOL_REUSED_AN_EXISTING_SOCKET,
        NetLog::IntCallback("idle_ms",
                            static_cast<int>(idle_time.InMilliseconds())));
  }

  if (reuse_type != ClientSocketHandle::UNUSED) {
    // The socket being handed out is no longer considered idle, but was
    // considered idle until just before this method was called.
    UMA_HISTOGRAM_CUSTOM_COUNTS("Net.Socket.NumIdleSockets",
                                idle_socket_count() + 1, 1, 256, 50);
  }

  net_log.AddEvent(
      NetLogEventType::SOCKET_POOL_BOUND_TO_SOCKET,
      handle->socket()->NetLog().source().ToEventParametersCallback());

  handed_out_socket_count_++;
  group->IncrementActiveSocketCount();
}

void ClientSocketPoolBaseHelper::AddIdleSocket(
    std::unique_ptr<StreamSocket> socket,
    Group* group) {
  DCHECK(socket);
  IdleSocket idle_socket;
  idle_socket.socket = socket.release();
  idle_socket.start_time = base::TimeTicks::Now();

  group->mutable_idle_sockets()->push_back(idle_socket);
  IncrementIdleCount();
}

void ClientSocketPoolBaseHelper::CancelAllConnectJobs() {
  for (auto i = group_map_.begin(); i != group_map_.end();) {
    Group* group = i->second;
    connecting_socket_count_ -= group->jobs().size();
    group->RemoveAllJobs();

    // Delete group if no longer needed.
    if (group->IsEmpty()) {
      auto old = i++;
      RemoveGroup(old);
    } else {
      ++i;
    }
  }
  DCHECK_EQ(0, connecting_socket_count_);
}

void ClientSocketPoolBaseHelper::CancelAllRequestsWithError(int error) {
  for (auto i = group_map_.begin(); i != group_map_.end();) {
    Group* group = i->second;

    while (true) {
      std::unique_ptr<Request> request = group->PopNextPendingRequest();
      if (!request)
        break;
      InvokeUserCallbackLater(request->handle(), request->release_callback(),
                              error, request->socket_tag());
    }

    // Delete group if no longer needed.
    if (group->IsEmpty()) {
      auto old = i++;
      RemoveGroup(old);
    } else {
      ++i;
    }
  }
}

bool ClientSocketPoolBaseHelper::ReachedMaxSocketsLimit() const {
  // Each connecting socket will eventually connect and be handed out.
  int total = handed_out_socket_count_ + connecting_socket_count_ +
      idle_socket_count();
  // There can be more sockets than the limit since some requests can ignore
  // the limit
  if (total < max_sockets_)
    return false;
  return true;
}

bool ClientSocketPoolBaseHelper::CloseOneIdleSocket() {
  if (idle_socket_count() == 0)
    return false;
  return CloseOneIdleSocketExceptInGroup(NULL);
}

bool ClientSocketPoolBaseHelper::CloseOneIdleSocketExceptInGroup(
    const Group* exception_group) {
  CHECK_GT(idle_socket_count(), 0);

  for (auto i = group_map_.begin(); i != group_map_.end(); ++i) {
    Group* group = i->second;
    if (exception_group == group)
      continue;
    std::list<IdleSocket>* idle_sockets = group->mutable_idle_sockets();

    if (!idle_sockets->empty()) {
      delete idle_sockets->front().socket;
      idle_sockets->pop_front();
      DecrementIdleCount();
      if (group->IsEmpty())
        RemoveGroup(i);

      return true;
    }
  }

  return false;
}

bool ClientSocketPoolBaseHelper::CloseOneIdleConnectionInHigherLayeredPool() {
  // This pool doesn't have any idle sockets. It's possible that a pool at a
  // higher layer is holding one of this sockets active, but it's actually idle.
  // Query the higher layers.
  for (auto it = higher_pools_.begin(); it != higher_pools_.end(); ++it) {
    if ((*it)->CloseOneIdleConnection())
      return true;
  }
  return false;
}

void ClientSocketPoolBaseHelper::InvokeUserCallbackLater(
    ClientSocketHandle* handle,
    CompletionOnceCallback callback,
    int rv,
    const SocketTag& socket_tag) {
  CHECK(!base::ContainsKey(pending_callback_map_, handle));
  pending_callback_map_[handle] = CallbackResultPair(std::move(callback), rv);
  if (rv == OK) {
    handle->socket()->ApplySocketTag(socket_tag);
  }
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&ClientSocketPoolBaseHelper::InvokeUserCallback,
                            weak_factory_.GetWeakPtr(), handle));
}

void ClientSocketPoolBaseHelper::InvokeUserCallback(
    ClientSocketHandle* handle) {
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

void ClientSocketPoolBaseHelper::TryToCloseSocketsInLayeredPools() {
  while (IsStalled()) {
    // Closing a socket will result in calling back into |this| to use the freed
    // socket slot, so nothing else is needed.
    if (!CloseOneIdleConnectionInHigherLayeredPool())
      return;
  }
}

ClientSocketPoolBaseHelper::Group::Group()
    : unassigned_job_count_(0),
      pending_requests_(NUM_PRIORITIES),
      active_socket_count_(0) {}

ClientSocketPoolBaseHelper::Group::~Group() {
  DCHECK_EQ(0u, unassigned_job_count_);
}

void ClientSocketPoolBaseHelper::Group::StartBackupJobTimer(
    const std::string& group_name,
    ClientSocketPoolBaseHelper* pool) {
  // Only allow one timer to run at a time.
  if (BackupJobTimerIsRunning())
    return;

  // Unretained here is okay because |backup_job_timer_| is
  // automatically cancelled when it's destroyed.
  backup_job_timer_.Start(
      FROM_HERE, pool->ConnectRetryInterval(),
      base::Bind(&Group::OnBackupJobTimerFired, base::Unretained(this),
                 group_name, pool));
}

bool ClientSocketPoolBaseHelper::Group::BackupJobTimerIsRunning() const {
  return backup_job_timer_.IsRunning();
}

bool ClientSocketPoolBaseHelper::Group::TryToUseUnassignedConnectJob() {
  SanityCheck();

  if (unassigned_job_count_ == 0)
    return false;
  --unassigned_job_count_;
  return true;
}

void ClientSocketPoolBaseHelper::Group::AddJob(std::unique_ptr<ConnectJob> job,
                                               bool is_preconnect) {
  SanityCheck();

  if (is_preconnect)
    ++unassigned_job_count_;
  jobs_.push_back(std::move(job));
}

void ClientSocketPoolBaseHelper::Group::RemoveJob(ConnectJob* job) {
  SanityCheck();

  // Check that |job| is in the list.
  auto it = std::find_if(jobs_.begin(), jobs_.end(),
                         [job](const std::unique_ptr<ConnectJob>& ptr) {
                           return ptr.get() == job;
                         });
  DCHECK(it != jobs_.end());
  std::unique_ptr<ConnectJob> owned_job = std::move(*it);
  jobs_.erase(it);
  size_t job_count = jobs_.size();
  if (job_count < unassigned_job_count_)
    unassigned_job_count_ = job_count;

  // If we've got no more jobs for this group, then we no longer need a
  // backup job either.
  if (jobs_.empty())
    backup_job_timer_.Stop();
}

void ClientSocketPoolBaseHelper::Group::OnBackupJobTimerFired(
    std::string group_name,
    ClientSocketPoolBaseHelper* pool) {
  // If there are no more jobs pending, there is no work to do.
  // If we've done our cleanups correctly, this should not happen.
  if (jobs_.empty()) {
    NOTREACHED();
    return;
  }

  // If our old job is waiting on DNS, or if we can't create any sockets
  // right now due to limits, just reset the timer.
  if (pool->ReachedMaxSocketsLimit() ||
      !HasAvailableSocketSlot(pool->max_sockets_per_group_) ||
      (*jobs_.begin())->GetLoadState() == LOAD_STATE_RESOLVING_HOST) {
    StartBackupJobTimer(group_name, pool);
    return;
  }

  if (pending_requests_.empty())
    return;

  std::unique_ptr<ConnectJob> backup_job =
      pool->connect_job_factory_->NewConnectJob(
          group_name, *pending_requests_.FirstMax().value(), pool);
  backup_job->net_log().AddEvent(NetLogEventType::BACKUP_CONNECT_JOB_CREATED);
  int rv = backup_job->Connect();
  pool->connecting_socket_count_++;
  ConnectJob* raw_backup_job = backup_job.get();
  AddJob(std::move(backup_job), false);
  if (rv != ERR_IO_PENDING)
    pool->OnConnectJobComplete(rv, raw_backup_job);
}

void ClientSocketPoolBaseHelper::Group::SanityCheck() {
  DCHECK_LE(unassigned_job_count_, jobs_.size());
}

void ClientSocketPoolBaseHelper::Group::RemoveAllJobs() {
  SanityCheck();

  // Delete active jobs.
  jobs_.clear();
  unassigned_job_count_ = 0;

  // Stop backup job timer.
  backup_job_timer_.Stop();
}

const ClientSocketPoolBaseHelper::Request*
ClientSocketPoolBaseHelper::Group::GetNextPendingRequest() const {
  return
      pending_requests_.empty() ? NULL : pending_requests_.FirstMax().value();
}

bool ClientSocketPoolBaseHelper::Group::HasConnectJobForHandle(
    const ClientSocketHandle* handle) const {
  // Search the first |jobs_.size()| pending requests for |handle|.
  // If it's farther back in the deque than that, it doesn't have a
  // corresponding ConnectJob.
  size_t i = 0;
  for (RequestQueue::Pointer pointer = pending_requests_.FirstMax();
       !pointer.is_null() && i < jobs_.size();
       pointer = pending_requests_.GetNextTowardsLastMin(pointer), ++i) {
    if (pointer.value()->handle() == handle)
      return true;
  }
  return false;
}

void ClientSocketPoolBaseHelper::Group::InsertPendingRequest(
    std::unique_ptr<Request> request) {
  // This value must be cached before we release |request|.
  RequestPriority priority = request->priority();
  if (request->respect_limits() == ClientSocketPool::RespectLimits::DISABLED) {
    // Put requests with RespectLimits::DISABLED (which should have
    // priority == MAXIMUM_PRIORITY) ahead of other requests with
    // MAXIMUM_PRIORITY.
    DCHECK_EQ(priority, MAXIMUM_PRIORITY);
    pending_requests_.InsertAtFront(request.release(), priority);
  } else {
    pending_requests_.Insert(request.release(), priority);
  }
}

std::unique_ptr<ClientSocketPoolBaseHelper::Request>
ClientSocketPoolBaseHelper::Group::PopNextPendingRequest() {
  if (pending_requests_.empty())
    return std::unique_ptr<ClientSocketPoolBaseHelper::Request>();
  return RemovePendingRequest(pending_requests_.FirstMax());
}

std::unique_ptr<ClientSocketPoolBaseHelper::Request>
ClientSocketPoolBaseHelper::Group::FindAndRemovePendingRequest(
    ClientSocketHandle* handle) {
  for (RequestQueue::Pointer pointer = pending_requests_.FirstMax();
       !pointer.is_null();
       pointer = pending_requests_.GetNextTowardsLastMin(pointer)) {
    if (pointer.value()->handle() == handle) {
      DCHECK_EQ(static_cast<RequestPriority>(pointer.priority()),
                pointer.value()->priority());
      std::unique_ptr<Request> request = RemovePendingRequest(pointer);
      return request;
    }
  }
  return std::unique_ptr<ClientSocketPoolBaseHelper::Request>();
}

void ClientSocketPoolBaseHelper::Group::SetPriority(ClientSocketHandle* handle,
                                                    RequestPriority priority) {
  for (RequestQueue::Pointer pointer = pending_requests_.FirstMax();
       !pointer.is_null();
       pointer = pending_requests_.GetNextTowardsLastMin(pointer)) {
    if (pointer.value()->handle() == handle) {
      if (pointer.value()->priority() == priority)
        return;

      std::unique_ptr<Request> request = RemovePendingRequest(pointer);

      // Requests that ignore limits much be created and remain at the highest
      // priority, and should not be reprioritized.
      DCHECK_EQ(request->respect_limits(),
                ClientSocketPool::RespectLimits::ENABLED);

      request->set_priority(priority);
      InsertPendingRequest(std::move(request));
      return;
    }
  }

  // This function must be called with a valid ClientSocketHandle.
  NOTREACHED();
}

std::unique_ptr<ClientSocketPoolBaseHelper::Request>
ClientSocketPoolBaseHelper::Group::RemovePendingRequest(
    const RequestQueue::Pointer& pointer) {
  // TODO(eroman): Temporary for debugging http://crbug.com/467797.
  CHECK(!pointer.is_null());
  std::unique_ptr<Request> request(pointer.value());
  pending_requests_.Erase(pointer);
  // If there are no more requests, kill the backup timer.
  if (pending_requests_.empty())
    backup_job_timer_.Stop();
  request->CrashIfInvalid();
  return request;
}

}  // namespace internal

}  // namespace net
