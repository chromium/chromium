// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_FRAME_SCHEDULER_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_FRAME_SCHEDULER_IMPL_H_

#include <array>
#include <bitset>
#include <memory>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/single_thread_task_runner.h"
#include "base/task/sequence_manager/task_queue.h"
#include "base/trace_event/trace_event.h"
#include "net/base/request_priority.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/scheduler/common/tracing_helper.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/agent_group_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/frame_origin_type.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/frame_task_queue_controller.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_task_queue.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/page_visibility_state.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_priority.h"
#include "third_party/blink/renderer/platform/scheduler/worker/worker_scheduler_proxy.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"

namespace base {
namespace sequence_manager {
class TaskQueue;
}  // namespace sequence_manager
namespace trace_event {
class BlameContext;
class TracedValue;
}  // namespace trace_event
}  // namespace base

namespace ukm {
class UkmRecorder;
}

namespace blink {

class MainThreadSchedulerTest;
class WebSchedulingTaskQueue;

namespace scheduler {

class MainThreadSchedulerImpl;
class MainThreadTaskQueue;
class PageSchedulerImpl;
class ResourceLoadingTaskRunnerHandleImpl;

namespace main_thread_scheduler_impl_unittest {
class MainThreadSchedulerImplTest;
}

namespace frame_scheduler_impl_unittest {
class FrameSchedulerImplTest;
}

namespace page_scheduler_impl_unittest {
class PageSchedulerImplTest;
}

class PLATFORM_EXPORT FrameSchedulerImpl : public FrameScheduler,
                                           FrameTaskQueueController::Delegate {
 public:
  FrameSchedulerImpl(PageSchedulerImpl* page_scheduler,
                     FrameScheduler::Delegate* delegate,
                     base::trace_event::BlameContext* blame_context,
                     FrameScheduler::FrameType frame_type);
  ~FrameSchedulerImpl() override;

  // FrameOrWorkerScheduler implementation:
  void SetPreemptedForCooperativeScheduling(Preempted) override;

  // FrameScheduler implementation:
  void SetFrameVisible(bool frame_visible) override;
  bool IsFrameVisible() const override;

  bool IsPageVisible() const override;

  void SetPaused(bool frame_paused) override;
  void SetShouldReportPostedTasksWhenDisabled(bool should_report) override;

  void SetCrossOriginToMainFrame(bool cross_origin) override;
  bool IsCrossOriginToMainFrame() const override;

  void SetIsAdFrame() override;
  bool IsAdFrame() const override;

  void TraceUrlChange(const String& url) override;
  void AddTaskTime(base::TimeDelta time) override;
  FrameScheduler::FrameType GetFrameType() const override;
  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner(TaskType) override;

  // Returns a wrapper around an instance of MainThreadTaskQueue which is
  // maintained in |resource_loading_task_queues_| map. The main thread task
  // queue is removed from the map and detached from both the main thread and
  // the frame schedulers when the wrapper instance goes out of scope.
  std::unique_ptr<WebResourceLoadingTaskRunnerHandle>
  CreateResourceLoadingTaskRunnerHandle() override;

  AgentGroupSchedulerImpl* GetAgentGroupScheduler();
  PageScheduler* GetPageScheduler() const override;
  void DidStartProvisionalLoad(bool is_main_frame) override;
  void DidCommitProvisionalLoad(bool is_web_history_inert_commit,
                                NavigationType navigation_type) override;
  WebScopedVirtualTimePauser CreateWebScopedVirtualTimePauser(
      const WTF::String& name,
      WebScopedVirtualTimePauser::VirtualTaskDuration duration) override;

  void OnFirstContentfulPaint() override;
  void OnFirstMeaningfulPaint() override;
  void OnLoad() override;
  bool IsWaitingForContentfulPaint() const;
  bool IsWaitingForMeaningfulPaint() const;

  // An "ordinary" FrameScheduler is responsible for a frame whose parent page
  // is a fully-featured page owned by a web view (as opposed to, e.g.: a Page
  // created by an SVGImage). Virtual for testing.
  virtual bool IsOrdinary() const;

  void AsValueInto(base::trace_event::TracedValue* state) const;
  bool IsExemptFromBudgetBasedThrottling() const override;
  std::unique_ptr<blink::mojom::blink::PauseSubresourceLoadingHandle>
  GetPauseSubresourceLoadingHandle() override;

  void OnStartedUsingFeature(SchedulingPolicy::Feature feature,
                             const SchedulingPolicy& policy) override;
  void OnStoppedUsingFeature(SchedulingPolicy::Feature feature,
                             const SchedulingPolicy& policy) override;

  base::WeakPtr<FrameScheduler> GetWeakPtr() override;
  base::WeakPtr<const FrameSchedulerImpl> GetWeakPtr() const;
  base::WeakPtr<FrameSchedulerImpl> GetInvalidatingOnBFCacheRestoreWeakPtr();

  void ReportActiveSchedulerTrackedFeatures() override;

  scoped_refptr<base::SingleThreadTaskRunner> ControlTaskRunner();

  void UpdatePolicy();

  // Whether the frame is opted-out from any kind of throttling.
  bool opted_out_from_all_throttling() const {
    return opted_out_from_all_throttling_;
  }
  // Whether the frame is opted-out from CPU time throttling and intensive wake
  // up throttling.
  bool opted_out_from_aggressive_throttling() const {
    return opted_out_from_all_throttling_ ||
           opted_out_from_aggressive_throttling_;
  }

  void OnTraceLogEnabled() { tracing_controller_.OnTraceLogEnabled(); }

  void SetPageVisibilityForTracing(PageVisibilityState page_visibility);
  void SetPageKeepActiveForTracing(bool keep_active);
  void SetPageFrozenForTracing(bool frozen);

  // Computes the priority of |task_queue| if it is associated to this frame
  // scheduler. Note that the main thread's policy should be upto date to
  // compute the correct priority.
  base::sequence_manager::TaskQueue::QueuePriority ComputePriority(
      MainThreadTaskQueue* task_queue) const;

  ukm::SourceId GetUkmSourceId() override;
  ukm::UkmRecorder* GetUkmRecorder();

  // FrameTaskQueueController::Delegate implementation.
  void OnTaskQueueCreated(
      MainThreadTaskQueue*,
      base::sequence_manager::TaskQueue::QueueEnabledVoter*) override;

  void SetOnIPCTaskPostedWhileInBackForwardCacheHandler();
  void DetachOnIPCTaskPostedWhileInBackForwardCacheHandler();
  void OnIPCTaskPostedWhileInBackForwardCache(uint32_t ipc_hash,
                                              const char* ipc_interface_name);

  // Returns the list of active features which currently tracked by the
  // scheduler for back-forward cache metrics.
  WTF::HashSet<SchedulingPolicy::Feature>
  GetActiveFeaturesTrackedForBackForwardCacheMetrics() override;

  // Notifies the delegate about the change in the set of active features.
  // The scheduler calls this function when needed after each task finishes,
  // grouping multiple OnStartedUsingFeature/OnStoppedUsingFeature into
  // one call to the delegate (which is generally expected to upload them to
  // the browser process).
  // No calls will be issued to the delegate if the set of features didn't
  // change since the previous call.
  void ReportFeaturesToDelegate();

  std::unique_ptr<WebSchedulingTaskQueue> CreateWebSchedulingTaskQueue(
      WebSchedulingPriority) override;
  void OnWebSchedulingTaskQueuePriorityChanged(MainThreadTaskQueue*);

  const base::UnguessableToken& GetAgentClusterId() const;

 protected:
  FrameSchedulerImpl(MainThreadSchedulerImpl* main_thread_scheduler,
                     PageSchedulerImpl* parent_page_scheduler,
                     FrameScheduler::Delegate* delegate,
                     base::trace_event::BlameContext* blame_context,
                     FrameScheduler::FrameType frame_type);

  // This will construct a subframe that is not linked to any main thread or
  // page scheduler. Should be used only for testing purposes.
  FrameSchedulerImpl();

  void OnShutdownResourceLoadingTaskQueue(
      scoped_refptr<MainThreadTaskQueue> task_queue);

  void DidChangeResourceLoadingPriority(
      scoped_refptr<MainThreadTaskQueue> task_queue,
      net::RequestPriority priority);

  scoped_refptr<MainThreadTaskQueue> GetTaskQueue(TaskType);

 private:
  friend class PageSchedulerImpl;
  friend class main_thread_scheduler_impl_unittest::MainThreadSchedulerImplTest;
  friend class frame_scheduler_impl_unittest::FrameSchedulerImplTest;
  friend class page_scheduler_impl_unittest::PageSchedulerImplTest;
  friend class PerAgentSchedulingBaseTest;
  friend class ResourceLoadingTaskRunnerHandleImpl;
  friend class ::blink::MainThreadSchedulerTest;

  // A class that adds and removes itself from the passed in weak pointer. While
  // one exists, resource loading is paused.
  class PauseSubresourceLoadingHandleImpl
      : public blink::mojom::blink::PauseSubresourceLoadingHandle {
   public:
    // This object takes a reference in |frame_scheduler| preventing resource
    // loading in the frame until the reference is released during destruction.
    explicit PauseSubresourceLoadingHandleImpl(
        base::WeakPtr<FrameSchedulerImpl> frame_scheduler);
    ~PauseSubresourceLoadingHandleImpl() override;

   private:
    base::WeakPtr<FrameSchedulerImpl> frame_scheduler_;

    DISALLOW_COPY_AND_ASSIGN(PauseSubresourceLoadingHandleImpl);
  };

  void DetachFromPageScheduler();
  void RemoveThrottleableQueueFromBudgetPools(MainThreadTaskQueue*);
  void ApplyPolicyToThrottleableQueue();
  bool ShouldThrottleTaskQueues() const;
  SchedulingLifecycleState CalculateLifecycleState(
      ObserverType type) const override;
  void UpdateQueuePolicy(
      MainThreadTaskQueue* queue,
      base::sequence_manager::TaskQueue::QueueEnabledVoter* voter);
  // Update throttling for |task_queue|. This changes the throttling ref counts
  // and should only be called for new queues if throttling is enabled, or if
  // the throttling state changes.
  void UpdateTaskQueueThrottling(MainThreadTaskQueue* task_queue,
                                 bool should_throttle);

  void AddPauseSubresourceLoadingHandle();
  void RemovePauseSubresourceLoadingHandle();

  void OnAddedAllThrottlingOptOut();
  void OnRemovedAllThrottlingOptOut();

  void OnAddedAggressiveThrottlingOptOut();
  void OnRemovedAggressiveThrottlingOptOut();

  void OnAddedBackForwardCacheOptOut(SchedulingPolicy::Feature feature);
  void OnRemovedBackForwardCacheOptOut(SchedulingPolicy::Feature feature);

  std::unique_ptr<ResourceLoadingTaskRunnerHandleImpl>
  CreateResourceLoadingTaskRunnerHandleImpl();

  FrameTaskQueueController* FrameTaskQueueControllerForTest() {
    return frame_task_queue_controller_.get();
  }

  // Create the QueueTraits for a specific TaskType. This returns base::nullopt
  // for loading tasks and non-frame-level tasks.
  static MainThreadTaskQueue::QueueTraits CreateQueueTraitsForTaskType(
      TaskType);

  // Reset the state which should not persist across navigations.
  void ResetForNavigation();

  // Same as GetActiveFeaturesTrackedForBackForwardCacheMetrics, but returns
  // a mask instead of a set.
  uint64_t GetActiveFeaturesTrackedForBackForwardCacheMetricsMask() const;

  base::WeakPtr<FrameOrWorkerScheduler> GetDocumentBoundWeakPtr() override;

  void NotifyDelegateAboutFeaturesAfterCurrentTask();

  // Create QueueTraits for the default (non-finch) task queues.
  static MainThreadTaskQueue::QueueTraits ThrottleableTaskQueueTraits();
  static MainThreadTaskQueue::QueueTraits DeferrableTaskQueueTraits();
  static MainThreadTaskQueue::QueueTraits PausableTaskQueueTraits();
  static MainThreadTaskQueue::QueueTraits UnpausableTaskQueueTraits();
  static MainThreadTaskQueue::QueueTraits FreezableTaskQueueTraits();
  static MainThreadTaskQueue::QueueTraits ForegroundOnlyTaskQueueTraits();
  static MainThreadTaskQueue::QueueTraits
  DoesNotUseVirtualTimeTaskQueueTraits();
  static MainThreadTaskQueue::QueueTraits LoadingTaskQueueTraits();
  static MainThreadTaskQueue::QueueTraits LoadingControlTaskQueueTraits();
  static MainThreadTaskQueue::QueueTraits FindInPageTaskQueueTraits();

  const FrameScheduler::FrameType frame_type_;

  bool is_ad_frame_;

  // A running tally of (wall) time spent in tasks for this frame.
  // This is periodically forwarded and zeroed out.
  base::TimeDelta task_time_;

  TraceableVariableController tracing_controller_;
  std::unique_ptr<FrameTaskQueueController> frame_task_queue_controller_;

  using ResourceLoadingTaskQueuePriorityMap =
      WTF::HashMap<scoped_refptr<MainThreadTaskQueue>,
                   base::sequence_manager::TaskQueue::QueuePriority>;

  // Queue to priority map of resource loading task queues created by
  // |frame_task_queue_controller_| via CreateResourceLoadingTaskRunnerHandle.
  ResourceLoadingTaskQueuePriorityMap resource_loading_task_queue_priorities_;

  MainThreadSchedulerImpl* const main_thread_scheduler_;  // NOT OWNED
  PageSchedulerImpl* parent_page_scheduler_;              // NOT OWNED
  FrameScheduler::Delegate* delegate_;                    // NOT OWNED
  base::trace_event::BlameContext* blame_context_;        // NOT OWNED
  SchedulingLifecycleState throttling_state_;
  TraceableState<bool, TracingCategoryName::kInfo> frame_visible_;
  TraceableState<bool, TracingCategoryName::kInfo> frame_paused_;
  TraceableState<FrameOriginType, TracingCategoryName::kInfo>
      frame_origin_type_;
  TraceableState<bool, TracingCategoryName::kInfo> subresource_loading_paused_;
  StateTracer<TracingCategoryName::kInfo> url_tracer_;
  TraceableState<bool, TracingCategoryName::kInfo> task_queues_throttled_;
  TraceableState<bool, TracingCategoryName::kInfo>
      preempted_for_cooperative_scheduling_;
  // TODO(https://crbug.com/827113): Trace the count of opt-outs.
  int all_throttling_opt_out_count_;
  int aggressive_throttling_opt_out_count_;
  TraceableState<bool, TracingCategoryName::kInfo>
      opted_out_from_all_throttling_;
  TraceableState<bool, TracingCategoryName::kInfo>
      opted_out_from_aggressive_throttling_;
  size_t subresource_loading_pause_count_;
  base::flat_map<SchedulingPolicy::Feature, int>
      back_forward_cache_opt_out_counts_;
  std::bitset<static_cast<size_t>(SchedulingPolicy::Feature::kMaxValue) + 1>
      back_forward_cache_opt_outs_;
  TraceableState<bool, TracingCategoryName::kInfo>
      opted_out_from_back_forward_cache_;
  // The last set of features passed to
  // Delegate::UpdateActiveSchedulerTrackedFeatures.
  uint64_t last_uploaded_active_features_ = 0;
  bool feature_report_scheduled_ = false;
  base::sequence_manager::TaskQueue::QueuePriority
      default_loading_task_priority_ =
          base::sequence_manager::TaskQueue::QueuePriority::kNormalPriority;

  // These are the states of the Page.
  // They should be accessed via GetPageScheduler()->SetPageState().
  // they are here because we don't support page-level tracing yet.
  TraceableState<bool, TracingCategoryName::kInfo> page_frozen_for_tracing_;
  TraceableState<PageVisibilityState, TracingCategoryName::kInfo>
      page_visibility_for_tracing_;
  TraceableState<bool, TracingCategoryName::kInfo>
      page_keep_active_for_tracing_;

  TraceableState<bool, TracingCategoryName::kInfo>
      waiting_for_contentful_paint_;
  TraceableState<bool, TracingCategoryName::kInfo>
      waiting_for_meaningful_paint_;

  // TODO(altimin): Remove after we have have 1:1 relationship between frames
  // and documents.
  base::WeakPtrFactory<FrameSchedulerImpl> document_bound_weak_factory_{this};

  // WeakPtrFactory for tracking IPCs posted to frames cached in the
  // back-forward cache. These weak pointers are invalidated when the page is
  // restored from the cache.
  base::WeakPtrFactory<FrameSchedulerImpl>
      invalidating_on_bfcache_restore_weak_factory_{this};

  mutable base::WeakPtrFactory<FrameSchedulerImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FrameSchedulerImpl);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_FRAME_SCHEDULER_IMPL_H_
