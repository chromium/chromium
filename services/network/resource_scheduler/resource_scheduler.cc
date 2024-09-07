// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/resource_scheduler/resource_scheduler.h"

#include <stdint.h>

#include <string>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/histogram_macros_local.h"
#include "base/strings/string_number_conversions.h"
#include "base/supports_user_data.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "base/trace_event/trace_event.h"
#include "base/unguessable_token.h"
#include "net/base/isolation_info.h"
#include "net/base/load_flags.h"
#include "net/base/request_priority.h"
#include "net/base/tracing.h"
#include "net/http/http_server_properties.h"
#include "net/log/net_log.h"
#include "net/nqe/effective_connection_type_observer.h"
#include "net/nqe/network_quality_estimator.h"
#include "net/nqe/peer_to_peer_connections_count_observer.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "url/scheme_host_port.h"

namespace network {

namespace {

enum StartMode { START_SYNC, START_ASYNC };

// Flags identifying various attributes of the request that are used
// when making scheduling decisions.
using RequestAttributes = uint8_t;
const RequestAttributes kAttributeNone = 0x00;
const RequestAttributes kAttributeInFlight = 0x01;
const RequestAttributes kAttributeDelayable = 0x02;

// Reasons why pending requests may be started.  For logging only.
enum class RequestStartTrigger {
  NONE,
  COMPLETION_PRE_BODY,
  COMPLETION_POST_BODY,
  BODY_REACHED,
  CLIENT_KILL,
  SPDY_PROXY_DETECTED,
  REQUEST_REPRIORITIZED,
  LONG_QUEUED_REQUESTS_TIMER_FIRED,
  EFFECTIVE_CONNECTION_TYPE_CHANGED,
  PEER_TO_PEER_CONNECTIONS_COUNT_CHANGED,
};

const char* RequestStartTriggerString(RequestStartTrigger trigger) {
  switch (trigger) {
    case RequestStartTrigger::NONE:
      return "NONE";
    case RequestStartTrigger::COMPLETION_PRE_BODY:
      return "COMPLETION_PRE_BODY";
    case RequestStartTrigger::COMPLETION_POST_BODY:
      return "COMPLETION_POST_BODY";
    case RequestStartTrigger::BODY_REACHED:
      return "BODY_REACHED";
    case RequestStartTrigger::CLIENT_KILL:
      return "CLIENT_KILL";
    case RequestStartTrigger::SPDY_PROXY_DETECTED:
      return "SPDY_PROXY_DETECTED";
    case RequestStartTrigger::REQUEST_REPRIORITIZED:
      return "REQUEST_REPRIORITIZED";
    case RequestStartTrigger::LONG_QUEUED_REQUESTS_TIMER_FIRED:
      return "LONG_QUEUED_REQUESTS_TIMER_FIRED";
    case RequestStartTrigger::EFFECTIVE_CONNECTION_TYPE_CHANGED:
      return "EFFECTIVE_CONNECTION_TYPE_CHANGED";
    case RequestStartTrigger::PEER_TO_PEER_CONNECTIONS_COUNT_CHANGED:
      return "PEER_TO_PEER_CONNECTIONS_COUNT_CHANGED";
  }
}

uint64_t CalculateTrackId(ResourceScheduler* scheduler) {
  static uint32_t sNextId = 0;
  CHECK(scheduler);
  return (reinterpret_cast<uint64_t>(scheduler) << 32) | sNextId++;
}

}  // namespace

// The maximum number of requests to allow be in-flight at any point in time per
// host. This limit does not apply to hosts that support request prioritization
// when |delay_requests_on_multiplexed_connections| is true.
static const size_t kMaxNumDelayableRequestsPerHostPerClient = 6;

// The priority level below which resources are considered to be delayable.
static const net::RequestPriority kDelayablePriorityThreshold = net::MEDIUM;

// Returns the duration after which the timer to dispatch queued requests should
// fire.
base::TimeDelta GetQueuedRequestsDispatchPeriodicity() {
  // This primarily affects two types of requests:
  // (i) Requests that have been queued for too long, and firing of timer may
  // result in some of those requests being dispatched to the network.
  // Such requests need to be queued for at least 15 seconds before they can be
  // dispatched.
  // (ii) Requests that were throttled proactively in anticipation of arrival
  // of higher priority requests. These requests need to be unthrottled after
  // their queuing duration expires. The queuing duration is a function of HTTP
  // RTT and can be of the order of 100 milliseconds.
  //
  // Note that the timer is active and fires periodically only if
  // there is at least one queued request.

  // When kProactivelyThrottleLowPriorityRequests is not enabled, it's
  // sufficient to choose a longer periodicity (implying that timer will be
  // fired less frequently) since the requests are queued for at least 15
  // seconds anyways. Firing the timer a bit later is not going to delay the
  // dispatch of the request by a significant amount.
  if (!base::FeatureList::IsEnabled(
          features::kProactivelyThrottleLowPriorityRequests)) {
    return base::Seconds(5);
  }

  // Choosing 100 milliseconds as the checking interval ensurs that the
  // queue is not checked too frequently. The interval is also not too long, so
  // we do not expect too many requests to go on the network at the
  // same time.
  return base::Milliseconds(base::GetFieldTrialParamByFeatureAsInt(
      features::kProactivelyThrottleLowPriorityRequests,
      "queued_requests_dispatch_periodicity_ms", 100));
}

// static
ResourceScheduler::ClientId ResourceScheduler::ClientId::Create(
    const std::optional<base::UnguessableToken>& token) {
  static uint64_t next_client_id = 0;
  return ClientId(next_client_id++, token);
}

struct ResourceScheduler::RequestPriorityParams {
  RequestPriorityParams()
      : priority(net::DEFAULT_PRIORITY), intra_priority(0) {}

  RequestPriorityParams(net::RequestPriority priority, int intra_priority)
      : priority(priority), intra_priority(intra_priority) {}

  bool operator==(const RequestPriorityParams& other) const {
    return (priority == other.priority) &&
           (intra_priority == other.intra_priority);
  }

  bool operator!=(const RequestPriorityParams& other) const {
    return !(*this == other);
  }

  bool GreaterThan(const RequestPriorityParams& other) const {
    if (priority != other.priority)
      return priority > other.priority;
    return intra_priority > other.intra_priority;
  }

  net::RequestPriority priority;
  int intra_priority;
};

class ResourceScheduler::RequestQueue {
 public:
  using NetQueue =
      std::multiset<ScheduledResourceRequestImpl*, ScheduledResourceSorter>;

  RequestQueue() : fifo_ordering_ids_(0) {}
  ~RequestQueue() {}

  // Adds |request| to the queue with given |priority|.
  void Insert(ScheduledResourceRequestImpl* request);

  // Removes |request| from the queue.
  void Erase(ScheduledResourceRequestImpl* request);

  NetQueue::iterator GetNextHighestIterator() { return queue_.begin(); }

  NetQueue::iterator End() { return queue_.end(); }

  // Returns true if |request| is queued.
  bool IsQueued(ScheduledResourceRequestImpl* request) const {
    return base::Contains(pointers_, request);
  }

  // Returns true if no requests are queued.
  bool IsEmpty() const { return queue_.empty(); }

  size_t Size() const { return queue_.size(); }

 private:
  using PointerMap =
      std::map<ScheduledResourceRequestImpl*, NetQueue::iterator>;

  uint32_t MakeFifoOrderingId() {
    fifo_ordering_ids_ += 1;
    return fifo_ordering_ids_;
  }

  // Used to create an ordering ID for scheduled resources so that resources
  // with same priority/intra_priority stay in fifo order.
  uint32_t fifo_ordering_ids_;

  NetQueue queue_;
  PointerMap pointers_;
};

ResourceScheduler::ScheduledResourceRequest::ScheduledResourceRequest() {}
ResourceScheduler::ScheduledResourceRequest::~ScheduledResourceRequest() {}

void ResourceScheduler::ScheduledResourceRequest::RunResumeCallback() {
  std::move(resume_callback_).Run();
}

// This is the handle we return to the URLLoader so it can interact with the
// request.
class ResourceScheduler::ScheduledResourceRequestImpl
    : public ScheduledResourceRequest {
 public:
  ScheduledResourceRequestImpl(ClientId client_id,
                               net::URLRequest* request,
                               ResourceScheduler* scheduler,
                               bool visible,
                               bool is_async)
      : client_id_(client_id),
        trace_track_(perfetto::Track(CalculateTrackId(scheduler))),
        request_(request),
        ready_(false),
        deferred_(false),
        is_async_(is_async),
        attributes_(kAttributeNone),
        scheduler_(scheduler),
        priority_(request_->priority(), 0),
        preserved_priority_(priority_),
        fifo_ordering_(0),
        scheme_host_port_(request->url()) {
    const bool deprioritize =
        base::FeatureList::IsEnabled(
            features::kVisibilityAwareResourceScheduler) &&
        !(request->load_flags() & net::LOAD_IGNORE_LIMITS) && !visible;
    if (deprioritize) {
      // Deprioritize to IDLE if this is a background request.
      priority_.priority = net::RequestPriority::IDLE;
      request_->SetPriority(priority_.priority);
    }
    TRACE_EVENT_BEGIN("network.scheduler", "ScheduledResourceRequest",
                      trace_track_, "url", request->url(), "priority",
                      priority_.priority);

    DCHECK(!request_->GetUserData(kUserDataKey));
    request_->SetUserData(kUserDataKey, std::make_unique<UnownedPointer>(this));
  }

  ScheduledResourceRequestImpl(const ScheduledResourceRequestImpl&) = delete;
  ScheduledResourceRequestImpl& operator=(const ScheduledResourceRequestImpl&) =
      delete;

  ~ScheduledResourceRequestImpl() override {
    TRACE_EVENT_END("network.scheduler", trace_track_);
    request_->RemoveUserData(kUserDataKey);
    scheduler_->RemoveRequest(this);
  }

  static ScheduledResourceRequestImpl* ForRequest(net::URLRequest* request) {
    UnownedPointer* pointer =
        static_cast<UnownedPointer*>(request->GetUserData(kUserDataKey));
    return pointer ? pointer->get() : nullptr;
  }

  // Starts the request. If |start_mode| is START_ASYNC, the request will not
  // be started immediately.
  void Start(StartMode start_mode) {
    TRACE_EVENT_INSTANT("network.scheduler", "RequestStart", trace_track_,
                        "mode", start_mode == START_ASYNC ? "async" : "sync");
    DCHECK(!ready_);

    // If the request was deferred, need to start it.  Otherwise, will just not
    // defer starting it in the first place, and the value of |start_mode|
    // makes no difference.
    if (deferred_) {
      // If can't start the request synchronously, post a task to start the
      // request.
      if (start_mode == START_ASYNC) {
        scheduler_->task_runner()->PostTask(
            FROM_HERE,
            base::BindOnce(&ScheduledResourceRequestImpl::Start,
                           weak_ptr_factory_.GetWeakPtr(), START_SYNC));
        return;
      }
      deferred_ = false;
      RunResumeCallback();
    }

    ready_ = true;
  }

  void Reprioritize(const RequestPriorityParams& priority) {
    TRACE_EVENT_INSTANT("network.scheduler", "RequestReprioritize",
                        trace_track_, "old_priority", priority_.priority,
                        "new_priority", priority.priority);
    priority_ = priority;
  }

  void PreservePriority(const RequestPriorityParams& preserved_priority) {
    preserved_priority_ = preserved_priority;
  }

  const RequestPriorityParams& get_request_priority_params() const {
    return priority_;
  }
  const RequestPriorityParams& get_preserved_request_priority_params() const {
    return preserved_priority_;
  }
  ClientId client_id() const { return client_id_; }
  perfetto::Track trace_track() const { return trace_track_; }
  net::URLRequest* url_request() { return request_; }
  const net::URLRequest* url_request() const { return request_; }
  bool is_async() const { return is_async_; }
  uint32_t fifo_ordering() const {
    // Ensure that |fifo_ordering_| has been set before it's used.
    DCHECK_LT(0u, fifo_ordering_);
    return fifo_ordering_;
  }
  void set_fifo_ordering(uint32_t fifo_ordering) {
    fifo_ordering_ = fifo_ordering;
  }
  RequestAttributes attributes() const { return attributes_; }
  void set_attributes(RequestAttributes attributes) {
    attributes_ = attributes;
  }
  const url::SchemeHostPort& scheme_host_port() const {
    return scheme_host_port_;
  }

 private:
  class UnownedPointer : public base::SupportsUserData::Data {
   public:
    explicit UnownedPointer(ScheduledResourceRequestImpl* pointer)
        : pointer_(pointer) {}

    UnownedPointer(const UnownedPointer&) = delete;
    UnownedPointer& operator=(const UnownedPointer&) = delete;

    ScheduledResourceRequestImpl* get() const { return pointer_; }

   private:
    const raw_ptr<ScheduledResourceRequestImpl> pointer_;
  };

  static const void* const kUserDataKey;

  // ScheduledResourceRequest implemnetation
  void WillStartRequest(bool* defer) override {
    deferred_ = *defer = !ready_;
    TRACE_EVENT_INSTANT("network.scheduler", "RequestWillStart", trace_track_,
                        "defered", deferred_);
  }

  const ClientId client_id_;
  perfetto::Track trace_track_;
  raw_ptr<net::URLRequest> request_;
  bool ready_;
  bool deferred_;
  bool is_async_;
  RequestAttributes attributes_;
  raw_ptr<ResourceScheduler> scheduler_;
  RequestPriorityParams priority_;
  // Remembers the initial priority.
  RequestPriorityParams preserved_priority_;
  uint32_t fifo_ordering_;

  // Cached to excessive recomputation in ReachedMaxRequestsPerHostPerClient().
  const url::SchemeHostPort scheme_host_port_;

  base::WeakPtrFactory<ResourceScheduler::ScheduledResourceRequestImpl>
      weak_ptr_factory_{this};
};

const void* const
    ResourceScheduler::ScheduledResourceRequestImpl::kUserDataKey =
        &ResourceScheduler::ScheduledResourceRequestImpl::kUserDataKey;

bool ResourceScheduler::ScheduledResourceSorter::operator()(
    const ScheduledResourceRequestImpl* a,
    const ScheduledResourceRequestImpl* b) const {
  // Want the set to be ordered first by decreasing priority, then by
  // decreasing intra_priority.
  // ie. with (priority, intra_priority)
  // [(1, 0), (1, 0), (0, 100), (0, 0)]
  if (a->get_request_priority_params() != b->get_request_priority_params())
    return a->get_request_priority_params().GreaterThan(
        b->get_request_priority_params());

  // If priority/intra_priority is the same, fall back to fifo ordering.
  // std::multiset doesn't guarantee this until c++11.
  return a->fifo_ordering() < b->fifo_ordering();
}

void ResourceScheduler::RequestQueue::Insert(
    ScheduledResourceRequestImpl* request) {
  DCHECK(!base::Contains(pointers_, request));
  TRACE_EVENT_INSTANT("network.scheduler", "RequestEnqueue",
                      request->trace_track());
  request->set_fifo_ordering(MakeFifoOrderingId());
  pointers_[request] = queue_.insert(request);
}

void ResourceScheduler::RequestQueue::Erase(
    ScheduledResourceRequestImpl* request) {
  TRACE_EVENT_INSTANT("network.scheduler", "RequestDequeue",
                      request->trace_track());
  PointerMap::iterator it = pointers_.find(request);
  CHECK(it != pointers_.end());
  queue_.erase(it->second);
  pointers_.erase(it);
}

// Each client represents a tab.
class ResourceScheduler::Client
    : public net::EffectiveConnectionTypeObserver,
      public net::PeerToPeerConnectionsCountObserver {
 public:
  Client(IsBrowserInitiated is_browser_initiated,
         net::NetworkQualityEstimator* network_quality_estimator,
         ResourceScheduler* resource_scheduler,
         const base::TickClock* tick_clock)
      : is_browser_initiated_(is_browser_initiated),
        in_flight_delayable_count_(0),
        num_skipped_scans_due_to_scheduled_start_(0),
        network_quality_estimator_(network_quality_estimator),
        resource_scheduler_(resource_scheduler),
        tick_clock_(tick_clock) {
    DCHECK(tick_clock_);

    if (network_quality_estimator_) {
      effective_connection_type_ =
          network_quality_estimator_->GetEffectiveConnectionType();
      UpdateParamsForNetworkQuality();
      network_quality_estimator_->AddEffectiveConnectionTypeObserver(this);
      network_quality_estimator_->AddPeerToPeerConnectionsCountObserver(this);
    }
  }

  ~Client() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (network_quality_estimator_) {
      network_quality_estimator_->RemoveEffectiveConnectionTypeObserver(this);
      network_quality_estimator_->RemovePeerToPeerConnectionsCountObserver(
          this);
    }
  }

  void ScheduleRequest(const net::URLRequest& url_request,
                       ScheduledResourceRequestImpl* request) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    SetRequestAttributes(request, DetermineRequestAttributes(request));
    ShouldStartReqResult should_start = ShouldStartRequest(request);
    if (should_start == START_REQUEST) {
      // New requests can be started synchronously without issue.
      StartRequest(request, START_SYNC, RequestStartTrigger::NONE);
      return;
    }

    pending_requests_.Insert(request);
  }

  void RemoveRequest(ScheduledResourceRequestImpl* request) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (pending_requests_.IsQueued(request)) {
      pending_requests_.Erase(request);
      DCHECK(!base::Contains(in_flight_requests_, request));
    } else {
      if (!RequestAttributesAreSet(request->attributes(), kAttributeDelayable))
        last_non_delayable_request_end_ = tick_clock_->NowTicks();
      EraseInFlightRequest(request);

      // Removing this request may have freed up another to load.
      LoadAnyStartablePendingRequests(
          RequestStartTrigger::COMPLETION_POST_BODY);
    }
  }

  RequestSet StartAndRemoveAllRequests() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    // First start any pending requests so that they will be moved into
    // in_flight_requests_. This may exceed the limits
    // kDefaultMaxNumDelayableRequestsPerClient and
    // kMaxNumDelayableRequestsPerHostPerClient, so this method must not do
    // anything that depends on those limits before calling
    // ClearInFlightRequests() below.
    while (!pending_requests_.IsEmpty()) {
      ScheduledResourceRequestImpl* request =
          *pending_requests_.GetNextHighestIterator();
      pending_requests_.Erase(request);
      // Starting requests asynchronously ensures no side effects, and avoids
      // starting a bunch of requests that may be about to be deleted.
      StartRequest(request, START_ASYNC, RequestStartTrigger::CLIENT_KILL);
    }
    RequestSet unowned_requests;
    for (RequestSet::iterator it = in_flight_requests_.begin();
         it != in_flight_requests_.end(); ++it) {
      unowned_requests.insert(*it);
      (*it)->set_attributes(kAttributeNone);
    }
    ClearInFlightRequests();
    return unowned_requests;
  }

  void ReprioritizeRequest(ScheduledResourceRequestImpl* request,
                           RequestPriorityParams old_priority_params,
                           RequestPriorityParams new_priority_params) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    request->url_request()->SetPriority(new_priority_params.priority);
    request->Reprioritize(new_priority_params);
    SetRequestAttributes(request, DetermineRequestAttributes(request));
    if (!pending_requests_.IsQueued(request)) {
      DCHECK(base::Contains(in_flight_requests_, request));
      // Request has already started.
      return;
    }

    pending_requests_.Erase(request);
    pending_requests_.Insert(request);

    if (new_priority_params.priority > old_priority_params.priority) {
      // Check if this request is now able to load at its new priority.
      ScheduleLoadAnyStartablePendingRequests(
          RequestStartTrigger::REQUEST_REPRIORITIZED);
    }
  }

  // Updates the params based on the current network quality estimate.
  void UpdateParamsForNetworkQuality() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    params_for_network_quality_ =
        resource_scheduler_->resource_scheduler_params_manager_
            .GetParamsForEffectiveConnectionType(
                network_quality_estimator_
                    ? effective_connection_type_
                    : net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN);
  }

  void OnLongQueuedRequestsDispatchTimerFired() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    LoadAnyStartablePendingRequests(
        RequestStartTrigger::LONG_QUEUED_REQUESTS_TIMER_FIRED);
  }

  bool HasNoPendingRequests() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return pending_requests_.IsEmpty();
  }

  bool IsActiveResourceSchedulerClient() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return (!pending_requests_.IsEmpty() || !in_flight_requests_.empty());
  }

  void OnVisibilityChanged(bool visible) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (visible == visible_) {
      return;
    }

    visible_ = visible;

    if (!base::FeatureList::IsEnabled(
            features::kVisibilityAwareResourceScheduler)) {
      return;
    }

    if (visible) {
      RestorePendingRequestPriorities();
    } else {
      DeprioritizePendingRequests();
    }
  }

  void RestorePendingRequestPriorities() {
    std::vector<ScheduledResourceRequestImpl*> requests_to_reprioritize;
    requests_to_reprioritize.reserve(pending_requests_.Size());
    for (RequestQueue::NetQueue::iterator it =
             pending_requests_.GetNextHighestIterator();
         it != pending_requests_.End(); ++it) {
      if ((*it)->get_request_priority_params() !=
          (*it)->get_preserved_request_priority_params()) {
        requests_to_reprioritize.emplace_back(*it);
      }
    }
    for (auto* request : requests_to_reprioritize) {
      ReprioritizeRequest(request, request->get_request_priority_params(),
                          request->get_preserved_request_priority_params());
    }
  }

  void DeprioritizePendingRequests() {
    std::vector<ScheduledResourceRequestImpl*> requests_to_reprioritize;
    requests_to_reprioritize.reserve(pending_requests_.Size());
    for (RequestQueue::NetQueue::iterator it =
             pending_requests_.GetNextHighestIterator();
         it != pending_requests_.End(); ++it) {
      if ((*it)->get_request_priority_params().priority >
              net::RequestPriority::IDLE &&
          !((*it)->url_request()->load_flags() & net::LOAD_IGNORE_LIMITS)) {
        requests_to_reprioritize.emplace_back(*it);
      } else {
        break;
      }
    }
    for (auto* request : requests_to_reprioritize) {
      const RequestPriorityParams& params =
          request->get_request_priority_params();
      RequestPriorityParams new_params = params;
      new_params.priority = net::RequestPriority::IDLE;
      ReprioritizeRequest(request, params, new_params);
    }
  }

  bool IsVisible() const { return visible_; }

 private:
  enum ShouldStartReqResult {
    DO_NOT_START_REQUEST_AND_STOP_SEARCHING,
    DO_NOT_START_REQUEST_AND_KEEP_SEARCHING,
    START_REQUEST
  };

  // net::EffectiveConnectionTypeObserver:
  void OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType effective_connection_type) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (effective_connection_type_ == effective_connection_type)
      return;

    effective_connection_type_ = effective_connection_type;
    UpdateParamsForNetworkQuality();

    // Change in network quality may allow |this| to dispatch more requests.
    LoadAnyStartablePendingRequests(
        RequestStartTrigger::EFFECTIVE_CONNECTION_TYPE_CHANGED);
  }

  // net::PeerToPeerConnectionsCountObserver:
  void OnPeerToPeerConnectionsCountChange(uint32_t count) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (p2p_connections_count_ == count)
      return;

    if (p2p_connections_count_ > 0 && count == 0) {
      // If the count of P2P connections went down to 0, start a timer. When the
      // timer fires, the queued browser-initiated requests would be dispatched.
      p2p_connections_count_end_timestamp_ = tick_clock_->NowTicks();
      p2p_connections_count_ended_timer_.Stop();
      p2p_connections_count_ended_timer_.Start(
          FROM_HERE,
          resource_scheduler_->resource_scheduler_params_manager_
              .TimeToPauseHeavyBrowserInitiatedRequestsAfterEndOfP2PConnections(),
          this,
          &ResourceScheduler::Client::OnP2PConnectionsCountEndedTimerFired);
    }

    p2p_connections_count_ = count;

    if (p2p_connections_count_ > 0 &&
        !p2p_connections_count_active_timestamp_.has_value()) {
      p2p_connections_count_active_timestamp_ = base::TimeTicks::Now();
    }

    if (p2p_connections_count_ == 0 &&
        p2p_connections_count_active_timestamp_.has_value()) {
      p2p_connections_count_active_timestamp_ = std::nullopt;
    }

    LoadAnyStartablePendingRequests(
        RequestStartTrigger::PEER_TO_PEER_CONNECTIONS_COUNT_CHANGED);
  }

  void OnP2PConnectionsCountEndedTimerFired() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    LoadAnyStartablePendingRequests(
        RequestStartTrigger::PEER_TO_PEER_CONNECTIONS_COUNT_CHANGED);
  }

  void InsertInFlightRequest(ScheduledResourceRequestImpl* request) {
    in_flight_requests_.insert(request);
    SetRequestAttributes(request, DetermineRequestAttributes(request));
  }

  void EraseInFlightRequest(ScheduledResourceRequestImpl* request) {
    size_t erased = in_flight_requests_.erase(request);
    DCHECK_EQ(1u, erased);
    // Clear any special state that we were tracking for this request.
    SetRequestAttributes(request, kAttributeNone);
  }

  void ClearInFlightRequests() {
    in_flight_requests_.clear();
    in_flight_delayable_count_ = 0;
  }

  size_t CountRequestsWithAttributes(
      const RequestAttributes attributes,
      ScheduledResourceRequestImpl* current_request) {
    size_t matching_request_count = 0;
    for (RequestSet::const_iterator it = in_flight_requests_.begin();
         it != in_flight_requests_.end(); ++it) {
      if (RequestAttributesAreSet((*it)->attributes(), attributes))
        matching_request_count++;
    }
    if (!RequestAttributesAreSet(attributes, kAttributeInFlight)) {
      bool current_request_is_pending = false;
      for (RequestQueue::NetQueue::const_iterator it =
               pending_requests_.GetNextHighestIterator();
           it != pending_requests_.End(); ++it) {
        if (RequestAttributesAreSet((*it)->attributes(), attributes))
          matching_request_count++;
        if (*it == current_request)
          current_request_is_pending = true;
      }
      // Account for the current request if it is not in one of the lists yet.
      if (current_request &&
          !base::Contains(in_flight_requests_, current_request) &&
          !current_request_is_pending) {
        if (RequestAttributesAreSet(current_request->attributes(), attributes))
          matching_request_count++;
      }
    }
    return matching_request_count;
  }

  bool RequestAttributesAreSet(RequestAttributes request_attributes,
                               RequestAttributes matching_attributes) const {
    return (request_attributes & matching_attributes) == matching_attributes;
  }

  void SetRequestAttributes(ScheduledResourceRequestImpl* request,
                            RequestAttributes attributes) {
    RequestAttributes old_attributes = request->attributes();
    if (old_attributes == attributes)
      return;

    if (RequestAttributesAreSet(old_attributes,
                                kAttributeInFlight | kAttributeDelayable)) {
      in_flight_delayable_count_--;
    }

    if (RequestAttributesAreSet(attributes,
                                kAttributeInFlight | kAttributeDelayable)) {
      in_flight_delayable_count_++;
    }

    request->set_attributes(attributes);
    DCHECK_EQ(CountRequestsWithAttributes(
                  kAttributeInFlight | kAttributeDelayable, request),
              in_flight_delayable_count_);
  }

  RequestAttributes DetermineRequestAttributes(
      ScheduledResourceRequestImpl* request) {
    RequestAttributes attributes = kAttributeNone;

    if (base::Contains(in_flight_requests_, request))
      attributes |= kAttributeInFlight;

    if (request->url_request()->priority() < kDelayablePriorityThreshold) {
      if (params_for_network_quality_
              .delay_requests_on_multiplexed_connections) {
        // Resources below the delayable priority threshold that are considered
        // delayable.
        attributes |= kAttributeDelayable;
      } else {
        // Resources below the delayable priority threshold that are being
        // requested from a server that does not support native prioritization
        // are considered delayable.
        net::HttpServerProperties& http_server_properties =
            *request->url_request()->context()->http_server_properties();
        if (!http_server_properties.SupportsRequestPriority(
                request->scheme_host_port(),
                request->url_request()
                    ->isolation_info()
                    .network_anonymization_key())) {
          attributes |= kAttributeDelayable;
        }
      }
    }

    return attributes;
  }

  bool ReachedMaxRequestsPerHostPerClient(
      const url::SchemeHostPort& active_request_host,
      bool supports_priority) const {
    // This method should not be called for requests to origins that support
    // prioritization (aka multiplexing) unless one of the experiments to
    // throttle priority requests is enabled.
    DCHECK(
        !supports_priority ||
        params_for_network_quality_.delay_requests_on_multiplexed_connections);

    // kMaxNumDelayableRequestsPerHostPerClient limit does not apply to servers
    // that support request priorities when
    // |delay_requests_on_multiplexed_connections| is true. If
    // |delay_requests_on_multiplexed_connections| is false, then
    // kMaxNumDelayableRequestsPerHostPerClient limit still applies to other
    // experiments that delay priority requests.
    if (supports_priority &&
        params_for_network_quality_.delay_requests_on_multiplexed_connections) {
      return false;
    }

    size_t same_host_count = 0;
    for (const ScheduledResourceRequestImpl* in_flight_request :
         in_flight_requests_) {
      if (active_request_host == in_flight_request->scheme_host_port()) {
        same_host_count++;
        if (same_host_count >= kMaxNumDelayableRequestsPerHostPerClient) {
          return true;
        }
      }
    }
    return false;
  }

  void StartRequest(ScheduledResourceRequestImpl* request,
                    StartMode start_mode,
                    RequestStartTrigger trigger) {
    const base::TimeTicks ticks_now = tick_clock_->NowTicks();
    // Only log on requests that were blocked by the ResourceScheduler.
    if (start_mode == START_ASYNC) {
      DCHECK_NE(RequestStartTrigger::NONE, trigger);
      request->url_request()->net_log().AddEventWithStringParams(
          net::NetLogEventType::RESOURCE_SCHEDULER_REQUEST_STARTED, "trigger",
          RequestStartTriggerString(trigger));
    }

    DCHECK(!request->url_request()->creation_time().is_null());
    base::TimeDelta queuing_duration =
        ticks_now - request->url_request()->creation_time();
    base::UmaHistogramMediumTimes(
        "ResourceScheduler.RequestQueuingDuration.Priority" +
            base::NumberToString(
                request->get_request_priority_params().priority),
        queuing_duration);

    // Update the start time of the non-delayble request.
    if (!RequestAttributesAreSet(request->attributes(), kAttributeDelayable))
      last_non_delayable_request_start_ = ticks_now;

    InsertInFlightRequest(request);
    request->Start(start_mode);
  }

  // Returns true if |request| should be throttled to avoid network contention
  // with active P2P connections.
  bool ShouldThrottleBrowserInitiatedRequestDueToP2PConnections(
      const ScheduledResourceRequestImpl& request) const {
    DCHECK(is_browser_initiated_);

    if (!base::FeatureList::IsEnabled(
            features::kPauseBrowserInitiatedHeavyTrafficForP2P)) {
      return false;
    }

    if (p2p_connections_count_ == 0) {
      // If the current count of P2P connections is 0, then check when was the
      // last time when there was an active P2P connection. If that timestamp is
      // recent, it's a heuristic indication that a new P2P connection may
      // start soon. To avoid network contention for that connection, keep
      // throttling browser-initiated requests.
      if (!p2p_connections_count_end_timestamp_.has_value())
        return false;

      bool p2p_connections_ended_long_back =
          (tick_clock_->NowTicks() -
           p2p_connections_count_end_timestamp_.value()) >=
          resource_scheduler_->resource_scheduler_params_manager_
              .TimeToPauseHeavyBrowserInitiatedRequestsAfterEndOfP2PConnections();
      if (p2p_connections_ended_long_back)
        return false;

      // If there are no currently active P2P connections, and the last
      // connection ended recently, then |p2p_connections_count_ended_timer_|
      // must be running. When |p2p_connections_count_ended_timer_| fires,
      // queued browser-initiated requests would be dispatched.
      DCHECK(p2p_connections_count_ended_timer_.IsRunning());
    }

    // Only throttle when effective connection type is between Slow2G and 3G.
    if (effective_connection_type_ <= net::EFFECTIVE_CONNECTION_TYPE_OFFLINE ||
        effective_connection_type_ >= net::EFFECTIVE_CONNECTION_TYPE_4G) {
      return false;
    }

    if (request.url_request()->priority() == net::HIGHEST)
      return false;

    if (p2p_connections_count_ > 0) {
      base::TimeDelta time_since_p2p_connections_active =
          tick_clock_->NowTicks() -
          p2p_connections_count_active_timestamp_.value();

      std::optional<base::TimeDelta> max_wait_time_p2p_connections =
          resource_scheduler_->resource_scheduler_params_manager_
              .max_wait_time_p2p_connections();

      if (time_since_p2p_connections_active >
          max_wait_time_p2p_connections.value()) {
        return false;
      }
    }

    // Check other request specific constraints.
    const net::NetworkTrafficAnnotationTag& traffic_annotation =
        request.url_request()->traffic_annotation();
    const int32_t unique_id_hash_code = traffic_annotation.unique_id_hash_code;

    if (!resource_scheduler_->resource_scheduler_params_manager_
             .CanThrottleNetworkTrafficAnnotationHash(unique_id_hash_code)) {
      return false;
    }

    return true;
  }

  // Records metrics on browser initiated requests. Called when the request is
  // dispatched to the network.
  void RecordMetricsForBrowserInitiatedRequestsOnNetworkDispatch(
      const ScheduledResourceRequestImpl& request) const {
    const net::NetworkTrafficAnnotationTag& traffic_annotation =
        request.url_request()->traffic_annotation();
    const int32_t unique_id_hash_code = traffic_annotation.unique_id_hash_code;

    // Metrics are recorded only for browser initiated requests that are
    // eligible for throttling.
    if (!resource_scheduler_->resource_scheduler_params_manager_
             .CanThrottleNetworkTrafficAnnotationHash(unique_id_hash_code)) {
      return;
    }
  }

  // ShouldStartRequest is the main scheduling algorithm.
  //
  // Requests are evaluated on five attributes:
  //
  // 1. Non-delayable requests:
  //   * Synchronous requests.
  //   * Non-HTTP[S] requests.
  //
  // 2. Requests to request-priority-capable origin servers (HTTP/2 and 3).
  //
  // 3. High-priority requests:
  //   * Higher priority requests (>= net::LOW).
  //
  // 4. Low priority requests
  //
  //  The following rules are followed:
  //   * Non-delayable, High-priority and request-priority capable requests are
  //     issued immediately.
  //   * Low priority requests are delayable.
  //   * Never exceed 10 delayable requests in flight per client.
  //   * Never exceed 6 delayable requests for a given host.
  //   * Browser-issued requests on low-speed connections may be delayed while
  //     there are active P2P connections
  //   * Requests delayed beyond a configured timeout may be unblocked.

  ShouldStartReqResult ShouldStartRequest(
      ScheduledResourceRequestImpl* request) const {
    // Browser requests are treated differently since they are not user-facing.
    if (is_browser_initiated_) {
      if (ShouldThrottleBrowserInitiatedRequestDueToP2PConnections(*request)) {
        return DO_NOT_START_REQUEST_AND_KEEP_SEARCHING;
      }

      RecordMetricsForBrowserInitiatedRequestsOnNetworkDispatch(*request);

      return START_REQUEST;
    }

    const net::URLRequest& url_request = *request->url_request();
    // Syncronous requests could block the entire render, which could impact
    // user-observable Clients.
    if (!request->is_async())
      return START_REQUEST;

    // Non-HTTP[S] requests.
    if (!url_request.url().SchemeIsHTTPOrHTTPS())
      return START_REQUEST;

    if (params_for_network_quality_.max_queuing_time &&
        tick_clock_->NowTicks() - url_request.creation_time() >=
            params_for_network_quality_.max_queuing_time) {
      return START_REQUEST;
    }

    bool priority_delayable =
        params_for_network_quality_.delay_requests_on_multiplexed_connections;

    bool supports_priority =
        url_request.context()
            ->http_server_properties()
            ->SupportsRequestPriority(
                request->scheme_host_port(),
                url_request.isolation_info().network_anonymization_key());

    // Requests on a connection that supports prioritization and multiplexing.
    if (!priority_delayable && supports_priority)
      return START_REQUEST;

    // Non-delayable requests.
    if (!RequestAttributesAreSet(request->attributes(), kAttributeDelayable))
      return START_REQUEST;

    // Delayable requests per client limit (10).
    DCHECK_GE(in_flight_requests_.size(), in_flight_delayable_count_);
    size_t num_non_delayable_requests_weighted = static_cast<size_t>(
        params_for_network_quality_.non_delayable_weight *
        (in_flight_requests_.size() - in_flight_delayable_count_));
    if ((in_flight_delayable_count_ + num_non_delayable_requests_weighted >=
         params_for_network_quality_.max_delayable_requests)) {
      return DO_NOT_START_REQUEST_AND_STOP_SEARCHING;
    }

    // Delayable requests per host limit (6).
    if (ReachedMaxRequestsPerHostPerClient(request->scheme_host_port(),
                                           supports_priority)) {
      // There may be other requests for other hosts that may be allowed,
      // so keep checking.
      return DO_NOT_START_REQUEST_AND_KEEP_SEARCHING;
    }

    if (IsNonDelayableRequestAnticipated()) {
      return DO_NOT_START_REQUEST_AND_STOP_SEARCHING;
    }

    return START_REQUEST;
  }

  // Returns true if a non-delayable request is expected to arrive soon.
  bool IsNonDelayableRequestAnticipated() const {
    std::optional<double> http_rtt_multiplier =
        params_for_network_quality_
            .http_rtt_multiplier_for_proactive_throttling;

    if (!http_rtt_multiplier.has_value())
      return false;

    if (http_rtt_multiplier <= 0)
      return false;

    // Currently, the heuristic for predicting the arrival of a non-delayable
    // request makes a prediction only if a non-delayable request has started
    // previously in this resource scheduler client.
    if (!last_non_delayable_request_start_.has_value())
      return false;

    std::optional<base::TimeDelta> http_rtt =
        network_quality_estimator_->GetHttpRTT();
    if (!http_rtt.has_value())
      return false;

    base::TimeDelta threshold_for_proactive_throttling =
        http_rtt.value() * http_rtt_multiplier.value();
    base::TimeDelta time_since_last_non_delayable_request_start =
        tick_clock_->NowTicks() - last_non_delayable_request_start_.value();

    if (time_since_last_non_delayable_request_start >=
        threshold_for_proactive_throttling) {
      // Last non-delayable request started more than
      // |threshold_for_proactive_throttling| back. The algorithm estimates that
      // by this time any non-delayable that were triggered by requests that
      // started long time back would have arrived at resource scheduler by now.
      //
      // On the other hand, if the last non-delayable request started recently,
      // then it's likely that the parsing of the response from that recently
      // started request would trigger additional non-delayable requests.
      return false;
    }
    return true;
  }

  // It is common for a burst of messages to come from the renderer which
  // trigger starting pending requests. Naively, this would result in O(n*m)
  // behavior for n pending requests and m <= n messages, as
  // LoadAnyStartablePendingRequest is O(n) for n pending requests. To solve
  // this, just post a task to the end of the queue to call the method,
  // coalescing the m messages into a single call to
  // LoadAnyStartablePendingRequests.
  // TODO(csharrison): Reconsider this if IPC batching becomes an easy to use
  // pattern.
  void ScheduleLoadAnyStartablePendingRequests(RequestStartTrigger trigger) {
    if (num_skipped_scans_due_to_scheduled_start_ == 0) {
      TRACE_EVENT0("loading", "ScheduleLoadAnyStartablePendingRequests");
      resource_scheduler_->task_runner()->PostTask(
          FROM_HERE, base::BindOnce(&Client::LoadAnyStartablePendingRequests,
                                    weak_ptr_factory_.GetWeakPtr(), trigger));
    }
    num_skipped_scans_due_to_scheduled_start_ += 1;
  }

  void LoadAnyStartablePendingRequests(RequestStartTrigger trigger) {
    // We iterate through all the pending requests, starting with the highest
    // priority one. For each entry, one of three things can happen:
    // 1) We start the request, remove it from the list, and keep checking.
    // 2) We do NOT start the request, but ShouldStartRequest() signals us that
    //     there may be room for other requests, so we keep checking and leave
    //     the previous request still in the list.
    // 3) We do not start the request, same as above, but StartRequest() tells
    //     us there's no point in checking any further requests.
    TRACE_EVENT0("loading", "LoadAnyStartablePendingRequests");
    num_skipped_scans_due_to_scheduled_start_ = 0;
    RequestQueue::NetQueue::iterator request_iter =
        pending_requests_.GetNextHighestIterator();

    while (request_iter != pending_requests_.End()) {
      ScheduledResourceRequestImpl* request = *request_iter;
      ShouldStartReqResult query_result = ShouldStartRequest(request);

      if (query_result == START_REQUEST) {
        pending_requests_.Erase(request);
        StartRequest(request, START_ASYNC, trigger);

        // StartRequest can modify the pending list, so we (re)start evaluation
        // from the currently highest priority request. Avoid copying a singular
        // iterator, which would trigger undefined behavior.
        if (pending_requests_.GetNextHighestIterator() ==
            pending_requests_.End())
          break;
        request_iter = pending_requests_.GetNextHighestIterator();
      } else if (query_result == DO_NOT_START_REQUEST_AND_KEEP_SEARCHING) {
        ++request_iter;
        continue;
      } else {
        DCHECK(query_result == DO_NOT_START_REQUEST_AND_STOP_SEARCHING);
        break;
      }
    }
  }

  RequestQueue pending_requests_;
  RequestSet in_flight_requests_;

  // True if |this| client is created for browser initiated requests.
  const IsBrowserInitiated is_browser_initiated_;

  // The number of delayable in-flight requests.
  size_t in_flight_delayable_count_;

  // The number of LoadAnyStartablePendingRequests scans that were skipped due
  // to smarter task scheduling around reprioritization.
  int num_skipped_scans_due_to_scheduled_start_;

  // Network quality estimator for network aware resource scheudling. This may
  // be null.
  raw_ptr<net::NetworkQualityEstimator> network_quality_estimator_;

  // Resource scheduling params computed for the current network quality.
  // These are recomputed every time an |OnNavigate| event is triggered.
  ResourceSchedulerParamsManager::ParamsForNetworkQuality
      params_for_network_quality_;

  // A pointer to the resource scheduler which contains the resource scheduling
  // configuration.
  raw_ptr<ResourceScheduler> resource_scheduler_;

  // Guaranteed to be non-null.
  raw_ptr<const base::TickClock> tick_clock_;

  // Time when the last non-delayble request started in this client.
  std::optional<base::TimeTicks> last_non_delayable_request_start_;

  // Time when the last non-delayble request ended in this client.
  std::optional<base::TimeTicks> last_non_delayable_request_end_;

  // Current estimated value of the effective connection type.
  net::EffectiveConnectionType effective_connection_type_ =
      net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN;

  // Current count of active peer to peer connections.
  uint32_t p2p_connections_count_ = 0u;

  // Earliest timestamp since when there is at least one active peer to peer
  // connection. Set to current timestamp when |p2p_connections_count_|
  // changes from 0 to a non-zero value. Reset to null when
  // |p2p_connections_count_| becomes 0.
  std::optional<base::TimeTicks> p2p_connections_count_active_timestamp_;

  // Earliest timestamp since when the count of active peer to peer
  // connection counts dropped from a non-zero value to zero. Set to current
  // timestamp when |p2p_connections_count_| changes from a non-zero value to 0.
  std::optional<base::TimeTicks> p2p_connections_count_end_timestamp_;

  base::OneShotTimer p2p_connections_count_ended_timer_;

  bool visible_ = true;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ResourceScheduler::Client> weak_ptr_factory_{this};
};

ResourceScheduler::ResourceScheduler(const base::TickClock* tick_clock)
    : tick_clock_(tick_clock ? tick_clock
                             : base::DefaultTickClock::GetInstance()),
      queued_requests_dispatch_periodicity_(
          GetQueuedRequestsDispatchPeriodicity()),
      task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()) {
  DCHECK(tick_clock_);

  StartLongQueuedRequestsDispatchTimerIfNeeded();
}

ResourceScheduler::~ResourceScheduler() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(unowned_requests_.empty());
  DCHECK(client_map_.empty());
}

std::unique_ptr<ResourceScheduler::ScheduledResourceRequest>
ResourceScheduler::ScheduleRequest(ClientId client_id,
                                   bool is_async,
                                   net::URLRequest* url_request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(url_request);

  ClientMap::iterator it = client_map_.find(client_id);
  const bool visible = it == client_map_.end() || it->second->IsVisible();
  auto request = std::make_unique<ScheduledResourceRequestImpl>(
      client_id, url_request, this, visible, is_async);

  if (it == client_map_.end()) {
    // There are several ways this could happen:
    // 1. <a ping> requests don't have a route_id.
    // 2. Most unittests don't send the IPCs needed to register Clients.
    // 3. The tab is closed while a RequestResource IPC is in flight.
    unowned_requests_.insert(request.get());
    request->Start(START_SYNC);
    return std::move(request);
  }

  Client* client = it->second.get();
  client->ScheduleRequest(*url_request, request.get());

  if (!IsLongQueuedRequestsDispatchTimerRunning())
    StartLongQueuedRequestsDispatchTimerIfNeeded();

  return std::move(request);
}

void ResourceScheduler::RemoveRequest(ScheduledResourceRequestImpl* request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (base::Contains(unowned_requests_, request)) {
    unowned_requests_.erase(request);
    return;
  }

  ClientMap::iterator client_it = client_map_.find(request->client_id());
  if (client_it == client_map_.end())
    return;

  Client* client = client_it->second.get();
  client->RemoveRequest(request);
}

void ResourceScheduler::OnClientCreated(
    ClientId client_id,
    IsBrowserInitiated is_browser_initiated,
    net::NetworkQualityEstimator* network_quality_estimator) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!base::Contains(client_map_, client_id));

  client_map_[client_id] = std::make_unique<Client>(
      is_browser_initiated, network_quality_estimator, this, tick_clock_);
}

void ResourceScheduler::OnClientDeleted(ClientId client_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ClientMap::iterator it = client_map_.find(client_id);
  // TODO(crbug.com/41407881): Turns this CHECK to DCHECK once the investigation
  // is done.
  CHECK(it != client_map_.end());

  Client* client = it->second.get();
  // TODO(crbug.com/41407881): Remove this CHECK once the investigation is done.
  CHECK(client);
  DCHECK(client->HasNoPendingRequests() ||
         IsLongQueuedRequestsDispatchTimerRunning());
  // ResourceDispatcherHost cancels all requests except for cross-renderer
  // navigations, async revalidations and detachable requests after
  // OnClientDeleted() returns.
  RequestSet client_unowned_requests = client->StartAndRemoveAllRequests();
  for (RequestSet::iterator request_it = client_unowned_requests.begin();
       request_it != client_unowned_requests.end(); ++request_it) {
    unowned_requests_.insert(*request_it);
  }

  client_map_.erase(it);
}

void ResourceScheduler::OnClientVisibilityChanged(
    const base::UnguessableToken& client_token,
    bool visible) {
  for (auto& client : client_map_) {
    if (client.first.token() == client_token) {
      client.second->OnVisibilityChanged(visible);
    }
  }
}

ResourceScheduler::Client* ResourceScheduler::GetClient(ClientId client_id) {
  ClientMap::iterator client_it = client_map_.find(client_id);
  if (client_it == client_map_.end())
    return nullptr;
  return client_it->second.get();
}

void ResourceScheduler::StartLongQueuedRequestsDispatchTimerIfNeeded() {
  bool pending_request_found = false;
  for (const auto& client : client_map_) {
    if (!client.second->HasNoPendingRequests()) {
      pending_request_found = true;
      break;
    }
  }

  // If there are no pending requests, then do not start the timer. This ensures
  // that we are not running the periodic timer when Chrome is not being
  // actively used (e.g., it's in background).
  if (!pending_request_found)
    return;

  long_queued_requests_dispatch_timer_.Start(
      FROM_HERE, queued_requests_dispatch_periodicity_, this,
      &ResourceScheduler::OnLongQueuedRequestsDispatchTimerFired);
}

void ResourceScheduler::OnLongQueuedRequestsDispatchTimerFired() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (auto& client : client_map_)
    client.second->OnLongQueuedRequestsDispatchTimerFired();

  StartLongQueuedRequestsDispatchTimerIfNeeded();
}

void ResourceScheduler::ReprioritizeRequest(net::URLRequest* request,
                                            net::RequestPriority new_priority,
                                            int new_intra_priority_value) {
  if (request->load_flags() & net::LOAD_IGNORE_LIMITS) {
    // Requests with the IGNORE_LIMITS flag must stay at MAXIMUM_PRIORITY.
    return;
  }

  auto* scheduled_resource_request =
      ScheduledResourceRequestImpl::ForRequest(request);

  // Downloads don't use the resource scheduler.
  if (!scheduled_resource_request) {
    request->SetPriority(new_priority);
    return;
  }

  RequestPriorityParams new_priority_params(new_priority,
                                            new_intra_priority_value);
  RequestPriorityParams old_priority_params =
      scheduled_resource_request->get_request_priority_params();

  if (old_priority_params == new_priority_params)
    return;

  scheduled_resource_request->PreservePriority(new_priority_params);

  ClientMap::iterator client_it =
      client_map_.find(scheduled_resource_request->client_id());
  if (client_it == client_map_.end()) {
    // The client was likely deleted shortly before we received this IPC.
    request->SetPriority(new_priority_params.priority);
    scheduled_resource_request->Reprioritize(new_priority_params);
    return;
  }

  Client* client = client_it->second.get();
  if (base::FeatureList::IsEnabled(
          (features::kVisibilityAwareResourceScheduler)) &&
      !client->IsVisible() &&
      new_priority_params.priority > net::RequestPriority::IDLE) {
    new_priority_params.priority = net::RequestPriority::IDLE;
  }
  client->ReprioritizeRequest(scheduled_resource_request, old_priority_params,
                              new_priority_params);
}

bool ResourceScheduler::IsLongQueuedRequestsDispatchTimerRunning() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return long_queued_requests_dispatch_timer_.IsRunning();
}

base::SequencedTaskRunner* ResourceScheduler::task_runner() {
  return task_runner_.get();
}

void ResourceScheduler::SetResourceSchedulerParamsManagerForTests(
    const ResourceSchedulerParamsManager& resource_scheduler_params_manager) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  resource_scheduler_params_manager_.Reset(resource_scheduler_params_manager);
  for (const auto& pair : client_map_) {
    pair.second->UpdateParamsForNetworkQuality();
  }
}

void ResourceScheduler::DispatchLongQueuedRequestsForTesting() {
  long_queued_requests_dispatch_timer_.Stop();
  OnLongQueuedRequestsDispatchTimerFired();
}

}  // namespace network
