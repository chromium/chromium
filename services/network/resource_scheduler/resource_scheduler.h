// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_RESOURCE_SCHEDULER_RESOURCE_SCHEDULER_H_
#define SERVICES_NETWORK_RESOURCE_SCHEDULER_RESOURCE_SCHEDULER_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/unguessable_token.h"
#include "net/base/priority_queue.h"
#include "net/base/request_priority.h"
#include "net/nqe/effective_connection_type.h"
#include "services/network/is_browser_initiated.h"
#include "services/network/resource_scheduler/resource_scheduler_params_manager.h"

namespace base {
class SequencedTaskRunner;
class TickClock;
}  // namespace base

namespace net {
class URLRequest;
class NetworkQualityEstimator;
}  // namespace net

namespace network {

// There is one ResourceScheduler. All renderer-initiated HTTP requests are
// expected to pass through it.
//
// There are two types of input to the scheduler:
// 1. Requests to start, cancel, or finish fetching a resource.
// 2. Notifications for renderer events, such as new tabs, navigation and
//    painting.
//
// These input come from different threads, so they may not be in sync. The UI
// thread is considered the authority on renderer lifetime, which means some
// IPCs may be meaningless if they arrive after the UI thread signals a renderer
// has been deleted.
//
// The ResourceScheduler tracks many Clients, which should correlate with tabs.
// A client is uniquely identified by an opaque identifier.
//
// Each Client may have many Requests in flight. Requests are uniquely
// identified within a Client by its ScheduledResourceRequest.
//
// Users should call ScheduleRequest() to notify this ResourceScheduler of a new
// request. The returned ResourceThrottle should be destroyed when the load
// finishes or is canceled, before the net::URLRequest.
//
// The scheduler may defer issuing the request via the ResourceThrottle
// interface or it may alter the request's priority by calling set_priority() on
// the URLRequest.
class COMPONENT_EXPORT(NETWORK_SERVICE) ResourceScheduler final {
 public:
  class COMPONENT_EXPORT(NETWORK_SERVICE) ClientId final {
   public:
    // Creates a new client id. Optional `token` is used to identify the client
    // associated to the created id.
    static ClientId Create(
        const std::optional<base::UnguessableToken>& token = std::nullopt);

    ClientId(const ClientId& that) = default;
    ClientId& operator=(const ClientId& that) = default;

    ~ClientId() = default;

    bool operator<(const ClientId& that) const { return id_ < that.id_; }
    bool operator==(const ClientId& that) const {
      return id_ == that.id_ && token_ == that.token_;
    }

    const base::UnguessableToken& token() const { return token_; }

    static ClientId CreateForTest(uint64_t id) { return ClientId(id); }

   private:
    explicit ClientId(
        uint64_t id,
        const std::optional<base::UnguessableToken>& token = std::nullopt)
        : id_(id), token_(token.value_or(base::UnguessableToken::Create())) {}
    uint64_t id_;
    base::UnguessableToken token_;
  };

  class ScheduledResourceRequest {
   public:
    ScheduledResourceRequest();
    virtual ~ScheduledResourceRequest();
    virtual void WillStartRequest(bool* defer) = 0;

    void set_resume_callback(base::OnceClosure callback) {
      resume_callback_ = std::move(callback);
    }
    void RunResumeCallback();

   private:
    base::OnceClosure resume_callback_;
  };

  explicit ResourceScheduler(const base::TickClock* tick_clock = nullptr);

  ResourceScheduler(const ResourceScheduler&) = delete;
  ResourceScheduler& operator=(const ResourceScheduler&) = delete;

  ~ResourceScheduler();

  // Requests that this ResourceScheduler schedule, and eventually loads, the
  // specified |url_request|. Caller should delete the returned ResourceThrottle
  // when the load completes or is canceled, before |url_request| is deleted.
  std::unique_ptr<ScheduledResourceRequest> ScheduleRequest(
      ClientId client_id,
      bool is_async,
      net::URLRequest* url_request);

  // Signals from the UI thread, posted as tasks on the IO thread:

  // Called when a renderer is created. |network_quality_estimator| is allowed
  // to be null.
  void OnClientCreated(ClientId client_id,
                       IsBrowserInitiated is_browser_initiated,
                       net::NetworkQualityEstimator* network_quality_estimator);

  // Called when a renderer is destroyed.
  void OnClientDeleted(ClientId client_id);

  // Called when a client has changed its visibility.
  virtual void OnClientVisibilityChanged(
      const base::UnguessableToken& client_token,
      bool visible);

  // Client functions:

  // Updates the priority for |request|. Modifies request->priority(), and may
  // start the request loading if it wasn't already started.
  // If the scheduler does not know about the request, |new_priority| is set but
  // |intra_priority_value| is ignored.
  void ReprioritizeRequest(net::URLRequest* request,
                           net::RequestPriority new_priority,
                           int intra_priority_value);

  // Returns true if the timer that dispatches long queued requests is running.
  bool IsLongQueuedRequestsDispatchTimerRunning() const;

  base::SequencedTaskRunner* task_runner();

  // Testing setters
  void SetTaskRunnerForTesting(
      scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner) {
    task_runner_ = std::move(sequenced_task_runner);
  }

  void SetResourceSchedulerParamsManagerForTests(
      const ResourceSchedulerParamsManager& resource_scheduler_params_manager);

  // Dispatch requests that have been queued for too long to network.
  void DispatchLongQueuedRequestsForTesting();

 private:
  class Client;
  class RequestQueue;
  class ScheduledResourceRequestImpl;
  struct RequestPriorityParams;
  struct ScheduledResourceSorter {
    bool operator()(const ScheduledResourceRequestImpl* a,
                    const ScheduledResourceRequestImpl* b) const;
  };

  using ClientMap = std::map<ClientId, std::unique_ptr<Client>>;
  using RequestSet =
      std::set<raw_ptr<ScheduledResourceRequestImpl, SetExperimental>>;

  // Called when a ScheduledResourceRequest is destroyed.
  void RemoveRequest(ScheduledResourceRequestImpl* request);

  // Returns the client for the given `client_id`.
  Client* GetClient(ClientId client_id);

  // May start the timer that dispatches long queued requests
  void StartLongQueuedRequestsDispatchTimerIfNeeded();

  // Called when |long_queued_requests_dispatch_timer_| is fired. May start any
  // pending requests that can be started.
  void OnLongQueuedRequestsDispatchTimerFired();

  ClientMap client_map_;
  RequestSet unowned_requests_;

  // Guaranteed to be non-null.
  raw_ptr<const base::TickClock> tick_clock_;

  // Timer to dispatch requests that may have been queued for too long.
  base::OneShotTimer long_queued_requests_dispatch_timer_;

  // Duration after which the timer to dispatch queued requests should fire.
  const base::TimeDelta queued_requests_dispatch_periodicity_;

  ResourceSchedulerParamsManager resource_scheduler_params_manager_;

  // The TaskRunner to post tasks on. Can be overridden for tests.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace network

#endif  // SERVICES_NETWORK_RESOURCE_SCHEDULER_RESOURCE_SCHEDULER_H_
