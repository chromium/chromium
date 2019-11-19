// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_RESOURCE_LOAD_SCHEDULER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_RESOURCE_LOAD_SCHEDULER_H_

#include <map>
#include <set>

#include "base/time/time.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

namespace base {
class Clock;
}

namespace blink {

class DetachableConsoleLogger;
class DetachableResourceFetcherProperties;

// Client interface to use the throttling/scheduling functionality that
// ResourceLoadScheduler provides.
class PLATFORM_EXPORT ResourceLoadSchedulerClient
    : public GarbageCollectedMixin {
 public:
  // Called when the request is granted to run.
  virtual void Run() = 0;

  void Trace(blink::Visitor* visitor) override {}
};

// ResourceLoadScheduler provides a unified per-frame infrastructure to schedule
// loading requests. When Request() is called with a
// ResourceLoadSchedulerClient |client|, it calls |client|'s Run() method
// synchronously or asynchronously to notify that |client| can start loading.
//
// A ResourceLoadScheduler may initiate a new resource loading in the following
// cases:
// - When Request() is called
// - When LoosenThrottlingPolicy() is called
// - When SetPriority() is called
// - When Release() is called with kReleaseAndSchedule
// - When OnThrottlingStateChanged() is called
//
// A ResourceLoadScheduler determines if a request can be throttleable or not,
// and keeps track of pending throttleable requests with priority information
// (i.e., ResourceLoadPriority accompanied with an integer called
// "intra-priority"). Here are the general principles:
//  - A ResourceLoadScheduler does not throttle requests that cannot be
//    throttleable. It will call client's Run() method as soon as possible.
//  - A ResourceLoadScheduler determines whether a request can be throttleable
//    by seeing Request()'s ThrottleOption argument and requests' priority
//    information. Requests' priority information can be modified via
//    SetPriority().
//  - A ResourceLoadScheulder won't initiate a new resource loading which can
//    be throttleable when there are more active throttleable requests loading
//    activities more than its internal threshold (i.e., what
//    GetOutstandingLimit() returns)".
//
//  ResourceLoadScheduler has two modes each of which has its own threshold.
//   - Tight mode (used until the frame sees a <body> element):
//     ResourceLoadScheduler considers a request throttleable if its priority
//     is less than |kHigh|.
//   - Normal mode:
//     ResourceLoadScheduler considers a request throttleable if its priority
//     is less than |kMedium|.
//
// Here are running experiments (as of M65):
//  - "ResourceLoadScheduler"
//   - Resource loading requests are not at throttled when the frame is in
//     the foreground tab.
//   - Resource loading requests are throttled when the frame is in a
//     background tab. It has different thresholds for the main frame
//     and sub frames. When the frame has been background for more than five
//     minutes, all throttleable resource loading requests are throttled
//     indefinitely (i.e., threshold is zero in such a circumstance).
class PLATFORM_EXPORT ResourceLoadScheduler final
    : public GarbageCollected<ResourceLoadScheduler>,
      public FrameScheduler::Observer {
 public:
  // An option to use in calling Request(). If kCanNotBeStoppedOrThrottled is
  // specified, the request should be granted and Run() should be called
  // synchronously. If kStoppable is specified, Run() will be called immediately
  // unless resource loading is stopped. Otherwise, OnRequestGranted() could be
  // called later when other outstanding requests are finished.
  enum class ThrottleOption {
    kThrottleable = 0,
    kStoppable = 1,
    kCanNotBeStoppedOrThrottled = 2,
  };

  // An option to use in calling Release(). If kReleaseOnly is specified,
  // the specified request should be released, but no other requests should
  // be scheduled within the call.
  enum class ReleaseOption { kReleaseOnly, kReleaseAndSchedule };

  // A class to pass traffic report hints on calling Release().
  class TrafficReportHints {
   public:
    // |encoded_data_length| is payload size in bytes sent over the network.
    // |decoded_body_length| is received resource data size in bytes.
    TrafficReportHints(int64_t encoded_data_length, int64_t decoded_body_length)
        : valid_(true),
          encoded_data_length_(encoded_data_length),
          decoded_body_length_(decoded_body_length) {}

    // Returns the instance that represents an invalid report, which can be
    // used when a caller don't want to report traffic, i.e. on a failure.
    static PLATFORM_EXPORT TrafficReportHints InvalidInstance() {
      return TrafficReportHints();
    }

    bool IsValid() const { return valid_; }

    int64_t encoded_data_length() const {
      DCHECK(valid_);
      return encoded_data_length_;
    }
    int64_t decoded_body_length() const {
      DCHECK(valid_);
      return decoded_body_length_;
    }

   private:
    // Default constructor makes an invalid instance that won't be recorded.
    TrafficReportHints() = default;

    bool valid_ = false;
    int64_t encoded_data_length_ = 0;
    int64_t decoded_body_length_ = 0;
  };

  // ResourceLoadScheduler has two policies: |kTight| and |kNormal|. Currently
  // this is used to support aggressive throttling while the corresponding frame
  // is in layout-blocking phase. There is only one state transition,
  // |kTight| => |kNormal|, which is done by |LoosenThrottlingPolicy|.
  enum class ThrottlingPolicy { kTight, kNormal };

  // Returned on Request(). Caller should need to return it via Release().
  using ClientId = uint64_t;

  static constexpr ClientId kInvalidClientId = 0u;

  static constexpr size_t kOutstandingUnlimited =
      std::numeric_limits<size_t>::max();

  ResourceLoadScheduler(ThrottlingPolicy initial_throttling_poilcy,
                        const DetachableResourceFetcherProperties&,
                        FrameScheduler*,
                        DetachableConsoleLogger& console_logger);
  ~ResourceLoadScheduler() override;

  void Trace(blink::Visitor*);

  // Changes the policy from |kTight| to |kNormal|. This function can be called
  // multiple times, and does nothing when the scheduler is already working with
  // the normal policy. This function may initiate a new resource loading.
  void LoosenThrottlingPolicy();

  // Stops all operations including observing throttling signals.
  // ResourceLoadSchedulerClient::Run() will not be called once this method is
  // called. This method can be called multiple times safely.
  void Shutdown();

  // Makes a request. This may synchronously call
  // ResourceLoadSchedulerClient::Run(), but it is guaranteed that ClientId is
  // populated before ResourceLoadSchedulerClient::Run() is called, so that the
  // caller can call Release() with the assigned ClientId correctly.
  void Request(ResourceLoadSchedulerClient*,
               ThrottleOption,
               ResourceLoadPriority,
               int intra_priority,
               ClientId*);

  // Updates the priority information of the given client. This function may
  // initiate a new resource loading.
  void SetPriority(ClientId, ResourceLoadPriority, int intra_priority);

  // ResourceLoadSchedulerClient should call this method when the loading is
  // finished, or canceled. This method can be called in a pre-finalization
  // step, bug the ReleaseOption must be kReleaseOnly in such a case.
  // TrafficReportHints is for reporting histograms.
  // TrafficReportHints::InvalidInstance() can be used to omit reporting.
  bool Release(ClientId, ReleaseOption, const TrafficReportHints&);

  // Checks if the specified client was already scheduled to call Run(), but
  // haven't call Release() yet.
  bool IsRunning(ClientId id) { return running_requests_.Contains(id); }

  // Sets outstanding limit for testing.
  void SetOutstandingLimitForTesting(size_t limit) {
    SetOutstandingLimitForTesting(limit, limit);
  }
  void SetOutstandingLimitForTesting(size_t tight_limit, size_t normal_limit);

  // FrameScheduler::Observer overrides:
  void OnLifecycleStateChanged(scheduler::SchedulingLifecycleState) override;

  // The caller is the owner of the |clock|. The |clock| must outlive the
  // ResourceLoadScheduler.
  void SetClockForTesting(const base::Clock* clock);

 private:
  class TrafficMonitor;

  class ClientIdWithPriority {
   public:
    struct Compare {
      bool operator()(const ClientIdWithPriority& x,
                      const ClientIdWithPriority& y) const {
        if (x.priority != y.priority)
          return x.priority > y.priority;
        if (x.intra_priority != y.intra_priority)
          return x.intra_priority > y.intra_priority;
        return x.client_id < y.client_id;
      }
    };

    ClientIdWithPriority(ClientId client_id,
                         WebURLRequest::Priority priority,
                         int intra_priority)
        : client_id(client_id),
          priority(priority),
          intra_priority(intra_priority) {}

    const ClientId client_id;
    const WebURLRequest::Priority priority;
    const int intra_priority;
  };

  struct ClientInfo : public GarbageCollected<ClientInfo> {
    ClientInfo(ResourceLoadSchedulerClient* client,
               ThrottleOption option,
               ResourceLoadPriority priority,
               int intra_priority)
        : client(client),
          option(option),
          priority(priority),
          intra_priority(intra_priority) {}

    void Trace(blink::Visitor* visitor) { visitor->Trace(client); }

    Member<ResourceLoadSchedulerClient> client;
    ThrottleOption option;
    ResourceLoadPriority priority;
    int intra_priority;
  };

  // Checks if |pending_requests_| for the specified option is effectively
  // empty, that means it does not contain any request that is still alive in
  // |pending_request_map_|.
  bool IsPendingRequestEffectivelyEmpty(ThrottleOption option);

  // Gets the highest priority pending request that is allowed to be run.
  bool GetNextPendingRequest(ClientId* id);

  // Returns whether we can throttle a request with the given client info based
  // on life cycle state.
  bool IsClientDelayable(const ClientIdWithPriority& info,
                         ThrottleOption option) const;

  // Generates the next ClientId.
  ClientId GenerateClientId();

  // Picks up one client if there is a budget and route it to run.
  void MaybeRun();

  // Grants a client to run,
  void Run(ClientId, ResourceLoadSchedulerClient*, bool throttleable);

  size_t GetOutstandingLimit() const;

  void ShowConsoleMessageIfNeeded();

  const Member<const DetachableResourceFetcherProperties>
      resource_fetcher_properties_;

  // A flag to indicate an internal running state.
  // TODO(toyoshim): We may want to use enum once we start to have more states.
  bool is_shutdown_ = false;

  ThrottlingPolicy policy_ = ThrottlingPolicy::kNormal;

  // ResourceLoadScheduler threshold values for various circumstances. Some
  // conditions can overlap, and ResourceLoadScheduler chooses the smallest
  // value in such cases.

  // Used when |policy_| is |kTight|.
  size_t tight_outstanding_limit_ = kOutstandingUnlimited;

  // Used when |policy_| is |kNormal|.
  size_t normal_outstanding_limit_ = kOutstandingUnlimited;

  // Used when |frame_scheduler_throttling_state_| is |kThrottled|.
  const size_t outstanding_limit_for_throttled_frame_scheduler_;

  // The last used ClientId to calculate the next.
  ClientId current_id_ = kInvalidClientId;

  // Holds clients that were granted and are running.
  HashSet<ClientId> running_requests_;

  HashSet<ClientId> running_throttleable_requests_;

  // Holds a flag to omit repeating console messages.
  bool is_console_info_shown_ = false;

  scheduler::SchedulingLifecycleState frame_scheduler_lifecycle_state_ =
      scheduler::SchedulingLifecycleState::kNotThrottled;

  // Holds clients that haven't been granted, and are waiting for a grant.
  HeapHashMap<ClientId, Member<ClientInfo>> pending_request_map_;

  // We use std::set here because WTF doesn't have its counterpart.
  // This tracks two sets of requests, throttleable and stoppable.
  std::map<ThrottleOption,
           std::set<ClientIdWithPriority, ClientIdWithPriority::Compare>>
      pending_requests_;

  // Remembers elapsed times in seconds when the top request in each queue is
  // processed.
  std::map<ThrottleOption, base::Time> pending_queue_update_times_;

  // Holds an internal class instance to monitor and report traffic.
  std::unique_ptr<TrafficMonitor> traffic_monitor_;

  // Handle to throttling observer.
  std::unique_ptr<FrameScheduler::LifecycleObserverHandle>
      scheduler_observer_handle_;

  const Member<DetachableConsoleLogger> console_logger_;

  const base::Clock* clock_;

  DISALLOW_COPY_AND_ASSIGN(ResourceLoadScheduler);
};

}  // namespace blink

#endif
