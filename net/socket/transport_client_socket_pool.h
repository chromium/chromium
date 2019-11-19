// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_TRANSPORT_CLIENT_SOCKET_POOL_H_
#define NET_SOCKET_TRANSPORT_CLIENT_SOCKET_POOL_H_

#include <stddef.h>
#include <stdint.h>

#include <cstddef>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "net/base/address_list.h"
#include "net/base/completion_once_callback.h"
#include "net/base/load_states.h"
#include "net/base/load_timing_info.h"
#include "net/base/net_errors.h"
#include "net/base/net_export.h"
#include "net/base/network_change_notifier.h"
#include "net/base/priority_queue.h"
#include "net/base/proxy_server.h"
#include "net/base/request_priority.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/client_socket_pool.h"
#include "net/socket/connect_job.h"
#include "net/socket/connection_attempts.h"
#include "net/socket/socket_tag.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/stream_socket.h"

namespace base {
namespace trace_event {
class ProcessMemoryDump;
}
}  // namespace base

namespace net {

struct CommonConnectJobParams;
struct NetLogSource;
struct NetworkTrafficAnnotationTag;

// TransportClientSocketPool establishes network connections through using
// ConnectJobs, and maintains a list of idle persistent sockets available for
// reuse. It restricts the number of sockets open at a time, both globally, and
// for each unique GroupId, which rougly corresponds to origin and privacy mode
// setting. TransportClientSocketPools is designed to work with HTTP reuse
// semantics, handling each request serially, before reusable sockets are
// returned to the socket pool.
//
// In order to manage connection limits on a per-Proxy basis, separate
// TransportClientSocketPools are created for each proxy, and another for
// connections that have no proxy.
// TransportClientSocketPool is an internal class that implements almost all
// the functionality from ClientSocketPool.
class NET_EXPORT_PRIVATE TransportClientSocketPool
    : public ClientSocketPool,
      public NetworkChangeNotifier::IPAddressObserver,
      public SSLClientContext::Observer {
 public:
  using Flags = uint32_t;

  // Used to specify specific behavior for the ClientSocketPool.
  enum Flag {
    NORMAL = 0,             // Normal behavior.
    NO_IDLE_SOCKETS = 0x1,  // Do not return an idle socket. Create a new one.
  };

  class NET_EXPORT_PRIVATE Request {
   public:
    // If |proxy_auth_callback| is null, proxy auth challenges will
    // result in an error.
    Request(
        ClientSocketHandle* handle,
        CompletionOnceCallback callback,
        const ProxyAuthCallback& proxy_auth_callback,
        RequestPriority priority,
        const SocketTag& socket_tag,
        RespectLimits respect_limits,
        Flags flags,
        scoped_refptr<SocketParams> socket_params,
        const base::Optional<NetworkTrafficAnnotationTag>& proxy_annotation_tag,
        const NetLogWithSource& net_log);

    ~Request();

    ClientSocketHandle* handle() const { return handle_; }
    CompletionOnceCallback release_callback() { return std::move(callback_); }
    const ProxyAuthCallback& proxy_auth_callback() const {
      return proxy_auth_callback_;
    }
    RequestPriority priority() const { return priority_; }
    void set_priority(RequestPriority priority) { priority_ = priority; }
    RespectLimits respect_limits() const { return respect_limits_; }
    Flags flags() const { return flags_; }
    SocketParams* socket_params() const { return socket_params_.get(); }
    const base::Optional<NetworkTrafficAnnotationTag>& proxy_annotation_tag()
        const {
      return proxy_annotation_tag_;
    }
    const NetLogWithSource& net_log() const { return net_log_; }
    const SocketTag& socket_tag() const { return socket_tag_; }
    ConnectJob* job() const { return job_; }

    // Associates a ConnectJob with the request. Must be called on a request
    // that does not already have a job.
    void AssignJob(ConnectJob* job);

    // Unassigns the request's |job_| and returns it. Must be called on a
    // request with a job.
    ConnectJob* ReleaseJob();

   private:
    ClientSocketHandle* const handle_;
    CompletionOnceCallback callback_;
    const ProxyAuthCallback proxy_auth_callback_;
    RequestPriority priority_;
    const RespectLimits respect_limits_;
    const Flags flags_;
    const scoped_refptr<SocketParams> socket_params_;
    const base::Optional<NetworkTrafficAnnotationTag> proxy_annotation_tag_;
    const NetLogWithSource net_log_;
    const SocketTag socket_tag_;
    ConnectJob* job_;

    DISALLOW_COPY_AND_ASSIGN(Request);
  };

  class ConnectJobFactory {
   public:
    ConnectJobFactory() {}
    virtual ~ConnectJobFactory() {}

    virtual std::unique_ptr<ConnectJob> NewConnectJob(
        ClientSocketPool::GroupId group_id,
        scoped_refptr<ClientSocketPool::SocketParams> socket_params,
        const base::Optional<NetworkTrafficAnnotationTag>& proxy_annotation_tag,
        RequestPriority request_priority,
        SocketTag socket_tag,
        ConnectJob::Delegate* delegate) const = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(ConnectJobFactory);
  };

  TransportClientSocketPool(
      int max_sockets,
      int max_sockets_per_group,
      base::TimeDelta unused_idle_socket_timeout,
      const ProxyServer& proxy_server,
      bool is_for_websockets,
      const CommonConnectJobParams* common_connect_job_params);

  // Creates a socket pool with an alternative ConnectJobFactory, for use in
  // testing.
  //
  // |connect_backup_jobs_enabled| can be set to false to disable backup connect
  // jobs (Which are normally enabled).
  static std::unique_ptr<TransportClientSocketPool> CreateForTesting(
      int max_sockets,
      int max_sockets_per_group,
      base::TimeDelta unused_idle_socket_timeout,
      base::TimeDelta used_idle_socket_timeout,
      const ProxyServer& proxy_server,
      std::unique_ptr<ConnectJobFactory> connect_job_factory,
      SSLClientContext* ssl_client_context,
      bool connect_backup_jobs_enabled);

  ~TransportClientSocketPool() override;

  // See LowerLayeredPool::IsStalled for documentation on this function.
  bool IsStalled() const override;

  // See LowerLayeredPool for documentation on these functions. It is expected
  // in the destructor that no higher layer pools remain.
  void AddHigherLayeredPool(HigherLayeredPool* higher_pool) override;
  void RemoveHigherLayeredPool(HigherLayeredPool* higher_pool) override;

  // ClientSocketPool implementation:
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
                     int64_t group_generation) override;
  void FlushWithError(int error) override;
  void CloseIdleSockets() override;
  void CloseIdleSocketsInGroup(const GroupId& group_id) override;
  int IdleSocketCount() const override;
  size_t IdleSocketCountInGroup(const GroupId& group_id) const override;
  LoadState GetLoadState(const GroupId& group_id,
                         const ClientSocketHandle* handle) const override;
  base::Value GetInfoAsValue(const std::string& name,
                             const std::string& type) const override;
  void DumpMemoryStats(
      base::trace_event::ProcessMemoryDump* pmd,
      const std::string& parent_dump_absolute_name) const override;

  bool RequestInGroupWithHandleHasJobForTesting(
      const GroupId& group_id,
      const ClientSocketHandle* handle) const {
    return group_map_.find(group_id)->second->RequestWithHandleHasJobForTesting(
        handle);
  }

  size_t NumNeverAssignedConnectJobsInGroupForTesting(
      const GroupId& group_id) const {
    return NumNeverAssignedConnectJobsInGroup(group_id);
  }

  size_t NumUnassignedConnectJobsInGroupForTesting(
      const GroupId& group_id) const {
    return NumUnassignedConnectJobsInGroup(group_id);
  }

  size_t NumConnectJobsInGroupForTesting(const GroupId& group_id) const {
    return NumConnectJobsInGroup(group_id);
  }

  int NumActiveSocketsInGroupForTesting(const GroupId& group_id) const {
    return NumActiveSocketsInGroup(group_id);
  }

  bool HasGroupForTesting(const GroupId& group_id) const {
    return HasGroup(group_id);
  }

  static bool connect_backup_jobs_enabled();
  static bool set_connect_backup_jobs_enabled(bool enabled);

  // NetworkChangeNotifier::IPAddressObserver methods:
  void OnIPAddressChanged() override;

  // SSLClientContext::Observer methods.
  void OnSSLConfigChanged(bool is_cert_database_change) override;
  void OnSSLConfigForServerChanged(const HostPortPair& server) override;

 private:
  class ConnectJobFactoryImpl;

  // Entry for a persistent socket which became idle at time |start_time|.
  struct IdleSocket {
    IdleSocket() : socket(nullptr) {}

    // An idle socket can't be used if it is disconnected or has been used
    // before and has received data unexpectedly (hence no longer idle).  The
    // unread data would be mistaken for the beginning of the next response if
    // we were to use the socket for a new request.
    //
    // Note that a socket that has never been used before (like a preconnected
    // socket) may be used even with unread data.  This may be, e.g., a SPDY
    // SETTINGS frame.
    bool IsUsable() const;

    StreamSocket* socket;
    base::TimeTicks start_time;
  };

  using RequestQueue = PriorityQueue<std::unique_ptr<Request>>;

  // A Group is allocated per GroupId when there are idle sockets, unbound
  // request, or bound requests. Otherwise, the Group object is removed from the
  // map.
  //
  // A request is "bound" to a ConnectJob when an unbound ConnectJob encounters
  // a proxy HTTP auth challenge, and the auth challenge is presented to that
  // request. Once a request and ConnectJob are bound together:
  // * All auth challenges the ConnectJob sees will be sent to that request.
  // * Cancelling the request will cancel the ConnectJob.
  // * The final result of the ConnectJob, and any returned socket, will only be
  //   sent to that bound request, though if the returned socket is returned to
  //   the socket pool, it can then be used to service any request.
  //
  // "assigned" jobs are unbound ConnectJobs that have a corresponding Request.
  // If there are 5 Jobs and 10 Requests, the 5 highest priority requests are
  // each assigned a Job. If there are 10 Jobs and 5 Requests, the first 5 Jobs
  // are each assigned to a request. Assignment is determined by order in their
  // corresponding arrays. The assignment concept is used to deal with
  // reprioritizing Jobs, and computing a Request's LoadState.
  //
  // |active_socket_count| tracks the number of sockets held by clients.
  // SanityCheck() will always be true, except during the invocation of a
  // method.  So all public methods expect the Group to pass SanityCheck() when
  // invoked.
  class NET_EXPORT_PRIVATE Group : public ConnectJob::Delegate {
   public:
    using JobList = std::list<std::unique_ptr<ConnectJob>>;

    struct BoundRequest {
      BoundRequest();
      BoundRequest(std::unique_ptr<ConnectJob> connect_job,
                   std::unique_ptr<Request> request,
                   int64_t generation);
      BoundRequest(BoundRequest&& other);
      BoundRequest& operator=(BoundRequest&& other);
      ~BoundRequest();

      std::unique_ptr<ConnectJob> connect_job;
      std::unique_ptr<Request> request;

      // Generation of |connect_job|. If it doesn't match the current
      // generation, ConnectJob will be destroyed, and a new one created on
      // completion.
      int64_t generation;

      // It's not safe to fail a request in a |CancelAllRequestsWithError| call
      // while it's waiting on user input, as the request may have raw pointers
      // to objects owned by |connect_job| that it could racily write to after
      // |connect_job| is destroyed. Instead, just track an error in that case,
      // and fail the request once the ConnectJob completes.
      int pending_error;
    };

    Group(const GroupId& group_id,
          TransportClientSocketPool* client_socket_pool);
    ~Group() override;

    // ConnectJob::Delegate methods:
    void OnConnectJobComplete(int result, ConnectJob* job) override;
    void OnNeedsProxyAuth(const HttpResponseInfo& response,
                          HttpAuthController* auth_controller,
                          base::OnceClosure restart_with_auth_callback,
                          ConnectJob* job) override;

    bool IsEmpty() const {
      return active_socket_count_ == 0 && idle_sockets_.empty() &&
             jobs_.empty() && unbound_requests_.empty() &&
             bound_requests_.empty();
    }

    bool HasAvailableSocketSlot(int max_sockets_per_group) const {
      return NumActiveSocketSlots() < max_sockets_per_group;
    }

    int NumActiveSocketSlots() const {
      return active_socket_count_ + static_cast<int>(jobs_.size()) +
             static_cast<int>(idle_sockets_.size()) +
             static_cast<int>(bound_requests_.size());
    }

    // Returns true if the group could make use of an additional socket slot, if
    // it were given one.
    bool CanUseAdditionalSocketSlot(int max_sockets_per_group) const {
      return HasAvailableSocketSlot(max_sockets_per_group) &&
             unbound_requests_.size() > jobs_.size();
    }

    // Returns the priority of the top of the unbound request queue
    // (which may be less than the maximum priority over the entire
    // queue, due to how we prioritize requests with |respect_limits|
    // DISABLED over others).
    RequestPriority TopPendingPriority() const {
      // NOTE: FirstMax().value()->priority() is not the same as
      // FirstMax().priority()!
      return unbound_requests_.FirstMax().value()->priority();
    }

    // Set a timer to create a backup job if it takes too long to
    // create one and if a timer isn't already running.
    void StartBackupJobTimer(const GroupId& group_id);

    bool BackupJobTimerIsRunning() const;

    // If there's a ConnectJob that's never been assigned to Request,
    // decrements |never_assigned_job_count_| and returns true.
    // Otherwise, returns false.
    bool TryToUseNeverAssignedConnectJob();

    void AddJob(std::unique_ptr<ConnectJob> job, bool is_preconnect);
    // Remove |job| from this group, which must already own |job|. Returns the
    // removed ConnectJob.
    std::unique_ptr<ConnectJob> RemoveUnboundJob(ConnectJob* job);
    void RemoveAllUnboundJobs();

    bool has_unbound_requests() const { return !unbound_requests_.empty(); }

    size_t unbound_request_count() const { return unbound_requests_.size(); }

    size_t ConnectJobCount() const;

    // Returns the connect job correspding to |handle|. In particular, if
    // |handle| is bound to a ConnectJob, returns that job. If |handle| is
    // "assigned" a ConnectJob, return that job. Otherwise, returns nullptr.
    ConnectJob* GetConnectJobForHandle(const ClientSocketHandle* handle) const;

    // Inserts the request into the queue based on priority
    // order. Older requests are prioritized over requests of equal
    // priority.
    void InsertUnboundRequest(std::unique_ptr<Request> request);

    // Gets (but does not remove) the next unbound request. Returns
    // NULL if there are no unbound requests.
    const Request* GetNextUnboundRequest() const;

    // Gets and removes the next unbound request. Returns NULL if
    // there are no unbound requests.
    std::unique_ptr<Request> PopNextUnboundRequest();

    // Finds the unbound request for |handle| and removes it. Returns
    // the removed unbound request, or NULL if there was none.
    std::unique_ptr<Request> FindAndRemoveUnboundRequest(
        ClientSocketHandle* handle);

    // Sets a pending error for all bound requests. Bound requests may be in the
    // middle of a callback, so can't be failed at arbitrary points in time.
    void SetPendingErrorForAllBoundRequests(int pending_error);

    // Attempts to bind the highest priority unbound request to |connect_job|,
    // and returns the bound request. If the request has previously been bound
    // to |connect_job|, returns the previously bound request. If there are no
    // requests, or the highest priority request doesn't have a proxy auth
    // callback, returns nullptr.
    const Request* BindRequestToConnectJob(ConnectJob* connect_job);

    // Finds the request, if any, bound to |connect_job|, and returns the
    // BoundRequest or base::nullopt if there was none.
    base::Optional<BoundRequest> FindAndRemoveBoundRequestForConnectJob(
        ConnectJob* connect_job);

    // Finds the bound request, if any, corresponding to |client_socket_handle|
    // and returns it. Destroys the ConnectJob bound to the request, if there
    // was one.
    std::unique_ptr<Request> FindAndRemoveBoundRequest(
        ClientSocketHandle* client_socket_handle);

    // Change the priority of the request named by |*handle|.  |*handle|
    // must refer to a request currently present in the group.  If |priority|
    // is the same as the current priority of the request, this is a no-op.
    void SetPriority(ClientSocketHandle* handle, RequestPriority priority);

    void IncrementActiveSocketCount() { active_socket_count_++; }
    void DecrementActiveSocketCount() { active_socket_count_--; }

    void IncrementGeneration() { generation_++; }

    // Whether the request in |unbound_requests_| with a given handle has a job.
    bool RequestWithHandleHasJobForTesting(
        const ClientSocketHandle* handle) const;

    const GroupId& group_id() { return group_id_; }
    size_t unassigned_job_count() const { return unassigned_jobs_.size(); }
    const JobList& jobs() const { return jobs_; }
    const std::list<IdleSocket>& idle_sockets() const { return idle_sockets_; }
    int active_socket_count() const { return active_socket_count_; }
    std::list<IdleSocket>* mutable_idle_sockets() { return &idle_sockets_; }
    size_t never_assigned_job_count() const {
      return never_assigned_job_count_;
    }
    int64_t generation() const { return generation_; }

   private:
    // Returns the iterator's unbound request after removing it from
    // the queue. Expects the Group to pass SanityCheck() when called.
    std::unique_ptr<Request> RemoveUnboundRequest(
        const RequestQueue::Pointer& pointer);

    // Finds the Request which is associated with the given ConnectJob.
    // Returns nullptr if none is found. Expects the Group to pass SanityCheck()
    // when called.
    RequestQueue::Pointer FindUnboundRequestWithJob(
        const ConnectJob* job) const;

    // Finds the Request in |unbound_requests_| which is the first request
    // without a job. Returns a null pointer if all requests have jobs. Does not
    // expect the Group to pass SanityCheck() when called, but does expect all
    // jobs to either be assigned to a request or in |unassigned_jobs_|. Expects
    // that no requests with jobs come after any requests without a job.
    RequestQueue::Pointer GetFirstRequestWithoutJob() const;

    // Tries to assign an unassigned |job| to a request. If no requests need a
    // job, |job| is added to |unassigned_jobs_|.
    // When called, does not expect the Group to pass SanityCheck(), but does
    // expect it to have passed SanityCheck() before the given ConnectJob was
    // either created or had the request it was assigned to removed.
    void TryToAssignUnassignedJob(ConnectJob* job);

    // Tries to assign a job to the given request. If any unassigned jobs are
    // available, the first unassigned job is assigned to the request.
    // Otherwise, if the request is ahead of the last request with a job, the
    // job is stolen from the last request with a job.
    // When called, does not expect the Group to pass SanityCheck(), but does
    // expect that:
    //  - the request associated with |request_pointer| must not have
    //    an assigned ConnectJob,
    //  - the first min( jobs_.size(), unbound_requests_.size() - 1 ) Requests
    //    other than the given request must have ConnectJobs, i.e. the group
    //    must have passed SanityCheck() before the passed in Request was either
    //    added or had its job unassigned.
    void TryToAssignJobToRequest(RequestQueue::Pointer request_pointer);

    // Transfers the associated ConnectJob from one Request to another. Expects
    // the source request to have a job, and the destination request to not have
    // a job. Does not expect the Group to pass SanityCheck() when called.
    void TransferJobBetweenRequests(Request* source, Request* dest);

    // Called when the backup socket timer fires.
    void OnBackupJobTimerFired(const GroupId& group_id);

    // Checks that:
    //  - |unassigned_jobs_| is empty iff there are at least as many requests
    //    as jobs.
    //  - Exactly the first |jobs_.size() - unassigned_jobs_.size()| requests
    //    have ConnectJobs.
    //  - No requests are assigned a ConnectJob in |unassigned_jobs_|.
    //  - No requests are assigned a ConnectJob not in |jobs_|.
    //  - No two requests are assigned the same ConnectJob.
    //  - All entries in |unassigned_jobs_| are also in |jobs_|.
    //  - There are no duplicate entries in |unassigned_jobs_|.
    void SanityCheck() const;

    const GroupId group_id_;
    TransportClientSocketPool* const client_socket_pool_;

    // Total number of ConnectJobs that have never been assigned to a Request.
    // Since jobs use late binding to requests, which ConnectJobs have or have
    // not been assigned to a request are not tracked.  This is incremented on
    // preconnect and decremented when a preconnect is assigned, or when there
    // are fewer than |never_assigned_job_count_| ConnectJobs.  Not incremented
    // when a request is cancelled.
    size_t never_assigned_job_count_;

    std::list<IdleSocket> idle_sockets_;
    JobList jobs_;  // For bookkeeping purposes, there is a copy of the raw
                    // pointer of each element of |jobs_| stored either in
                    // |unassigned_jobs_|, or as the associated |job_| of an
                    // element of |unbound_requests_|.
    std::list<ConnectJob*> unassigned_jobs_;
    RequestQueue unbound_requests_;
    int active_socket_count_;  // number of active sockets used by clients
    // A timer for when to start the backup job.
    base::OneShotTimer backup_job_timer_;

    // List of Requests bound to ConnectJobs currently undergoing proxy auth.
    // The Requests and ConnectJobs in this list do not appear in
    // |unbound_requests_| or |jobs_|.
    std::vector<BoundRequest> bound_requests_;

    // An id for the group.  It gets incremented every time we FlushWithError()
    // the socket pool, or refresh the group.  This is so that when sockets get
    // released back to the group, we can make sure that they are discarded
    // rather than reused. Destroying a group will reset the generation number,
    // but as that only happens once there are no outstanding sockets or
    // requests associated with the group, that's harmless.
    int64_t generation_;
  };

  using GroupMap = std::map<GroupId, Group*>;

  struct CallbackResultPair {
    CallbackResultPair();
    CallbackResultPair(CompletionOnceCallback callback_in, int result_in);
    CallbackResultPair(CallbackResultPair&& other);
    CallbackResultPair& operator=(CallbackResultPair&& other);
    ~CallbackResultPair();

    CompletionOnceCallback callback;
    int result;
  };

  using PendingCallbackMap =
      std::map<const ClientSocketHandle*, CallbackResultPair>;

  TransportClientSocketPool(
      int max_sockets,
      int max_sockets_per_group,
      base::TimeDelta unused_idle_socket_timeout,
      base::TimeDelta used_idle_socket_timeout,
      const ProxyServer& proxy_server,
      std::unique_ptr<ConnectJobFactory> connect_job_factory,
      SSLClientContext* ssl_client_context,
      bool connect_backup_jobs_enabled);

  base::TimeDelta ConnectRetryInterval() const {
    // TODO(mbelshe): Make this tuned dynamically based on measured RTT.
    //                For now, just use the max retry interval.
    return base::TimeDelta::FromMilliseconds(kMaxConnectRetryIntervalMs);
  }

  // TODO(mmenke): de-inline these.
  size_t NumNeverAssignedConnectJobsInGroup(const GroupId& group_id) const {
    return group_map_.find(group_id)->second->never_assigned_job_count();
  }

  size_t NumUnassignedConnectJobsInGroup(const GroupId& group_id) const {
    return group_map_.find(group_id)->second->unassigned_job_count();
  }

  size_t NumConnectJobsInGroup(const GroupId& group_id) const {
    return group_map_.find(group_id)->second->ConnectJobCount();
  }

  int NumActiveSocketsInGroup(const GroupId& group_id) const {
    return group_map_.find(group_id)->second->active_socket_count();
  }

  bool HasGroup(const GroupId& group_id) const;

  // Closes all idle sockets if |force| is true.  Else, only closes idle
  // sockets that timed out or can't be reused.  Made public for testing.
  void CleanupIdleSockets(bool force);

  // Closes one idle socket.  Picks the first one encountered.
  // TODO(willchan): Consider a better algorithm for doing this.  Perhaps we
  // should keep an ordered list of idle sockets, and close them in order.
  // Requires maintaining more state.  It's not clear if it's worth it since
  // I'm not sure if we hit this situation often.
  bool CloseOneIdleSocket();

  // Checks higher layered pools to see if they can close an idle connection.
  bool CloseOneIdleConnectionInHigherLayeredPool();

  // Closes all idle sockets in |group| if |force| is true.  Else, only closes
  // idle sockets in |group| that timed out with respect to |now| or can't be
  // reused.
  void CleanupIdleSocketsInGroup(bool force,
                                 Group* group,
                                 const base::TimeTicks& now);

  Group* GetOrCreateGroup(const GroupId& group_id);
  void RemoveGroup(const GroupId& group_id);
  void RemoveGroup(GroupMap::iterator it);

  // Called when the number of idle sockets changes.
  void IncrementIdleCount();
  void DecrementIdleCount();

  // Scans the group map for groups which have an available socket slot and
  // at least one pending request. Returns true if any groups are stalled, and
  // if so (and if both |group| and |group_id| are not NULL), fills |group|
  // and |group_id| with data of the stalled group having highest priority.
  bool FindTopStalledGroup(Group** group, GroupId* group_id) const;

  // Removes |job| from |group|, which must already own |job|.
  void RemoveConnectJob(ConnectJob* job, Group* group);

  // Tries to see if we can handle any more requests for |group|.
  void OnAvailableSocketSlot(const GroupId& group_id, Group* group);

  // Process a pending socket request for a group.
  void ProcessPendingRequest(const GroupId& group_id, Group* group);

  // Assigns |socket| to |handle| and updates |group|'s counters appropriately.
  void HandOutSocket(std::unique_ptr<StreamSocket> socket,
                     ClientSocketHandle::SocketReuseType reuse_type,
                     const LoadTimingInfo::ConnectTiming& connect_timing,
                     ClientSocketHandle* handle,
                     base::TimeDelta time_idle,
                     Group* group,
                     const NetLogWithSource& net_log);

  // Adds |socket| to the list of idle sockets for |group|.
  void AddIdleSocket(std::unique_ptr<StreamSocket> socket, Group* group);

  // Iterates through |group_map_|, canceling all ConnectJobs and deleting
  // groups if they are no longer needed.
  void CancelAllConnectJobs();

  // Iterates through |group_map_|, posting |error| callbacks for all
  // requests, and then deleting groups if they are no longer needed.
  void CancelAllRequestsWithError(int error);

  // Returns true if we can't create any more sockets due to the total limit.
  bool ReachedMaxSocketsLimit() const;

  // This is the internal implementation of RequestSocket().  It differs in that
  // it does not handle logging into NetLog of the queueing status of
  // |request|.
  int RequestSocketInternal(const GroupId& group_id, const Request& request);

  // Assigns an idle socket for the group to the request.
  // Returns |true| if an idle socket is available, false otherwise.
  bool AssignIdleSocketToRequest(const Request& request, Group* group);

  static void LogBoundConnectJobToRequest(
      const NetLogSource& connect_job_source,
      const Request& request);

  // Same as CloseOneIdleSocket() except it won't close an idle socket in
  // |group|.  If |group| is NULL, it is ignored.  Returns true if it closed a
  // socket.
  bool CloseOneIdleSocketExceptInGroup(const Group* group);

  // Checks if there are stalled socket groups that should be notified
  // for possible wakeup.
  void CheckForStalledSocketGroups();

  // Posts a task to call InvokeUserCallback() on the next iteration through the
  // current message loop.  Inserts |callback| into |pending_callback_map_|,
  // keyed by |handle|. Apply |socket_tag| to the socket if socket successfully
  // created.
  void InvokeUserCallbackLater(ClientSocketHandle* handle,
                               CompletionOnceCallback callback,
                               int rv,
                               const SocketTag& socket_tag);

  // These correspond to ConnectJob::Delegate methods, and are invoked by the
  // Group a ConnectJob belongs to.
  void OnConnectJobComplete(Group* group, int result, ConnectJob* job);
  void OnNeedsProxyAuth(Group* group,
                        const HttpResponseInfo& response,
                        HttpAuthController* auth_controller,
                        base::OnceClosure restart_with_auth_callback,
                        ConnectJob* job);

  // Invokes the user callback for |handle|.  By the time this task has run,
  // it's possible that the request has been cancelled, so |handle| may not
  // exist in |pending_callback_map_|.  We look up the callback and result code
  // in |pending_callback_map_|.
  void InvokeUserCallback(ClientSocketHandle* handle);

  // Tries to close idle sockets in a higher level socket pool as long as this
  // this pool is stalled.
  void TryToCloseSocketsInLayeredPools();

  // Closes all idle sockets and cancels all unbound ConnectJobs associated with
  // |it->second|. Also increments the group's generation number, ensuring any
  // currently existing handed out socket will be silently closed when it is
  // returned to the socket pool. Bound ConnectJobs will only be destroyed on
  // once they complete, as they may be waiting on user input. No request
  // (including bound ones) will be failed as a result of this call - instead,
  // new ConnectJobs will be created.
  //
  // The group may be removed if this leaves the group empty. The caller must
  // call CheckForStalledSocketGroups() after all applicable groups have been
  // refreshed.
  void RefreshGroup(GroupMap::iterator it, const base::TimeTicks& now);

  GroupMap group_map_;

  // Map of the ClientSocketHandles for which we have a pending Task to invoke a
  // callback.  This is necessary since, before we invoke said callback, it's
  // possible that the request is cancelled.
  PendingCallbackMap pending_callback_map_;

  // The total number of idle sockets in the system.
  int idle_socket_count_;

  // Number of connecting sockets across all groups.
  int connecting_socket_count_;

  // Number of connected sockets we handed out across all groups.
  int handed_out_socket_count_;

  // The maximum total number of sockets. See ReachedMaxSocketsLimit.
  const int max_sockets_;

  // The maximum number of sockets kept per group.
  const int max_sockets_per_group_;

  // The time to wait until closing idle sockets.
  const base::TimeDelta unused_idle_socket_timeout_;
  const base::TimeDelta used_idle_socket_timeout_;

  const ProxyServer proxy_server_;

  const std::unique_ptr<ConnectJobFactory> connect_job_factory_;

  // TODO(vandebo) Remove when backup jobs move to TransportClientSocketPool
  bool connect_backup_jobs_enabled_;

  // Pools that create connections through |this|.  |this| will try to close
  // their idle sockets when it stalls.  Must be empty on destruction.
  std::set<HigherLayeredPool*> higher_pools_;

  SSLClientContext* const ssl_client_context_;

  base::WeakPtrFactory<TransportClientSocketPool> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(TransportClientSocketPool);
};

}  // namespace net

#endif  // NET_SOCKET_TRANSPORT_CLIENT_SOCKET_POOL_H_
