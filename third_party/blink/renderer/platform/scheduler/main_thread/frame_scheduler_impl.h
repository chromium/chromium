// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_FRAME_SCHEDULER_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_FRAME_SCHEDULER_IMPL_H_

#include <array>
#include <bitset>
#include <memory>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequence_manager/task_queue.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "net/base/request_priority.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/scheduler/common/back_forward_cache_disabling_feature_tracker.h"
#include "third_party/blink/renderer/platform/scheduler/common/task_priority.h"
#include "third_party/blink/renderer/platform/scheduler/common/tracing_helper.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/agent_group_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/frame_origin_type.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/frame_task_queue_controller.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_task_queue.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/page_visibility_state.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_priority.h"
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_queue_type.h"
#include "third_party/blink/renderer/platform/scheduler/worker/worker_scheduler_proxy.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value_forward.h"

namespace base {
namespace sequence_manager {
class TaskQueue;
}  // namespace sequence_manager
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
                     bool is_in_embedded_frame_tree,
                     FrameScheduler::FrameType frame_type);
  FrameSchedulerImpl(const FrameSchedulerImpl&) = delete;
  FrameSchedulerImpl& operator=(const FrameSchedulerImpl&) = delete;
  ~FrameSchedulerImpl() override;

  // FrameOrWorkerScheduler implementation:
  void SetPreemptedForCooperativeScheduling(Preempted) override;

  // FrameScheduler implementation:
  void SetFrameVisible(bool frame_visible) override;
  bool IsFrameVisible() const override;

  bool IsPageVisible() const override;

  void SetPaused(bool frame_paused) override;
  void SetShouldReportPostedTasksWhenDisabled(bool should_report) override;

  void SetCrossOriginToNearestMainFrame(bool cross_origin) override;
  bool IsCrossOriginToNearestMainFrame() const override;

  void SetIsAdFrame(bool is_ad_frame) override;
  bool IsAdFrame() const override;

  bool IsInEmbeddedFrameTree() const override;

  void TraceUrlChange(const String& url) override;
  void AddTaskTime(base::TimeDelta time) override;
  void OnTaskCompleted(TaskQueue::TaskTiming*,
                       base::TimeTicks desired_execution_time);
  FrameScheduler::FrameType GetFrameType() const override;
  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner(TaskType) override;

  AgentGroupScheduler* GetAgentGroupScheduler() override;
  PageScheduler* GetPageScheduler() const override;
  void DidStartProvisionalLoad() override;
  void DidCommitProvisionalLoad(bool is_web_history_inert_commit,
                                NavigationType navigation_type) override;
  WebScopedVirtualTimePauser CreateWebScopedVirtualTimePauser(
      const WTF::String& name,
      WebScopedVirtualTimePauser::VirtualTaskDuration duration) override;
  scoped_refptr<base::SingleThreadTaskRunner> CompositorTaskRunner() override;

  void OnFirstContentfulPaintInMainFrame() override;
  void OnFirstMeaningfulPaint() override;
  void OnMainFrameInteractive() override;
  bool IsWaitingForContentfulPaint() const;
  bool IsWaitingForMeaningfulPaint() const;

  // An "ordinary" FrameScheduler is responsible for a frame whose parent page
  // is a fully-featured page owned by a web view (as opposed to, e.g.: a Page
  // created by an SVGImage). Virtual for testing.
  virtual bool IsOrdinary() const;

  bool IsExemptFromBudgetBasedThrottling() const override;
  std::unique_ptr<blink::mojom::blink::PauseSubresourceLoadingHandle>
  GetPauseSubresourceLoadingHandle() override;

  void OnStartedUsingNonStickyFeature(
      SchedulingPolicy::Feature feature,
      const SchedulingPolicy& policy,
      std::unique_ptr<SourceLocation> source_location,
      SchedulingAffectingFeatureHandle* handle) override;
  void OnStartedUsingStickyFeature(
      SchedulingPolicy::Feature feature,
      const SchedulingPolicy& policy,
      std::unique_ptr<SourceLocation> source_location) override;
  void OnStoppedUsingNonStickyFeature(
      SchedulingAffectingFeatureHandle* handle) override;

  base::WeakPtr<FrameScheduler> GetWeakPtr() override;
  base::WeakPtr<const FrameSchedulerImpl> GetWeakPtr() const;
  base::WeakPtr<FrameSchedulerImpl> GetInvalidatingOnBFCacheRestoreWeakPtr();

  void ReportActiveSchedulerTrackedFeatures() override;

  scoped_refptr<base::SingleThreadTaskRunner> ControlTaskRunner();

  void UpdatePolicy();

  // Whether the frame is opted-out from CPU time throttling and intensive wake
  // up throttling.
  bool opted_out_from_aggressive_throttling() const {
    return opted_out_from_aggressive_throttling_;
  }

  void OnTraceLogEnabled() { tracing_controller_.OnTraceLogEnabled(); }

  void SetPageVisibilityForTracing(PageVisibilityState page_visibility);
  void SetPageFrozenForTracing(bool frozen);

  // Computes the priority of |task_queue| if it is associated to this frame
  // scheduler. Note that the main thread's policy should be upto date to
  // compute the correct priority.
  TaskPriority ComputePriority(MainThreadTaskQueue* task_queue) const;

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

  std::unique_ptr<WebSchedulingTaskQueue> CreateWebSchedulingTaskQueue(
      WebSchedulingQueueType,
      WebSchedulingPriority) override;
  void OnWebSchedulingTaskQueuePriorityChanged(MainThreadTaskQueue*);
  void OnWebSchedulingTaskQueueDestroyed(MainThreadTaskQueue*);

  const base::UnguessableToken& GetAgentClusterId() const;

  void WriteIntoTrace(perfetto::TracedValue context) const;
  void WriteIntoTrace(perfetto::TracedProto<
                      perfetto::protos::pbzero::RendererMainThreadTaskExecution>
                          proto) const;

 protected:
  FrameSchedulerImpl(MainThreadSchedulerImpl* main_thread_scheduler,
                     PageSchedulerImpl* parent_page_scheduler,
                     FrameScheduler::Delegate* delegate,
                     bool is_in_embedded_frame_tree,
                     FrameScheduler::FrameType frame_type);

  // This will construct a subframe that is not linked to any main thread or
  // page scheduler. Should be used only for testing purposes.
  FrameSchedulerImpl();

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
    PauseSubresourceLoadingHandleImpl(
        const PauseSubresourceLoadingHandleImpl&) = delete;
    PauseSubresourceLoadingHandleImpl& operator=(
        const PauseSubresourceLoadingHandleImpl&) = delete;
    ~PauseSubresourceLoadingHandleImpl() override;

   private:
    base::WeakPtr<FrameSchedulerImpl> frame_scheduler_;
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

  void AddPauseSubresourceLoadingHandle();
  void RemovePauseSubresourceLoadingHandle();

  void OnAddedAggressiveThrottlingOptOut();
  void OnRemovedAggressiveThrottlingOptOut();

  FrameTaskQueueController* FrameTaskQueueControllerForTest() {
    return frame_task_queue_controller_.get();
  }

  // Create the QueueTraits for a specific TaskType. This returns absl::nullopt
  // for loading tasks and non-frame-level tasks.
  static MainThreadTaskQueue::QueueTraits CreateQueueTraitsForTaskType(
      TaskType);

  // Reset the state which should not persist across navigations.
  void ResetForNavigation();

  base::WeakPtr<FrameOrWorkerScheduler> GetFrameOrWorkerSchedulerWeakPtr()
      override;

  void MoveTaskQueuesToCorrectWakeUpBudgetPool();

  // Create QueueTraits for the default (non-finch) task queues.
  static MainThreadTaskQueue::QueueTraits ThrottleableTaskQueueTraits();
  static MainThreadTaskQueue::QueueTraits DeferrableTaskQueueTraits();
  static MainThreadTaskQueue::QueueTraits PausableTaskQueueTraits();
  static MainThreadTaskQueue::QueueTraits UnpausableTaskQueueTraits();
  static MainThreadTaskQueue::QueueTraits FreezableTaskQueueTraits();
  static MainThreadTaskQueue::QueueTraits ForegroundOnlyTaskQueueTraits();
  static MainThreadTaskQueue::QueueTraits
  CanRunWhenVirtualTimePausedTaskQueueTraits();
  static MainThreadTaskQueue::QueueTraits LoadingTaskQueueTraits();
  static MainThreadTaskQueue::QueueTraits UnfreezableLoadingTaskQueueTraits();
  static MainThreadTaskQueue::QueueTraits LoadingControlTaskQueueTraits();
  static MainThreadTaskQueue::QueueTraits FindInPageTaskQueueTraits();
  static MainThreadTaskQueue::QueueTraits InputBlockingQueueTraits();

  const FrameScheduler::FrameType frame_type_;

  // Whether this scheduler is created for a frame that is contained
  // inside an embedded frame tree. See /docs/frame_trees.md.
  const bool is_in_embedded_frame_tree_;

  bool is_ad_frame_ = false;

  // A running tally of (wall) time spent in tasks for this frame.
  // This is periodically forwarded and zeroed out.
  base::TimeDelta task_time_;

  TraceableVariableController tracing_controller_;
  std::unique_ptr<FrameTaskQueueController> frame_task_queue_controller_;

  MainThreadSchedulerImpl* const main_thread_scheduler_;  // NOT OWNED
  PageSchedulerImpl* parent_page_scheduler_;              // NOT OWNED
  FrameScheduler::Delegate* delegate_;                    // NOT OWNED
  SchedulingLifecycleState throttling_state_;
  TraceableState<bool, TracingCategory::kInfo> frame_visible_;
  TraceableState<bool, TracingCategory::kInfo> frame_paused_;
  TraceableState<FrameOriginType, TracingCategory::kInfo> frame_origin_type_;
  TraceableState<bool, TracingCategory::kInfo> subresource_loading_paused_;
  StateTracer<TracingCategory::kInfo> url_tracer_;
  TraceableState<bool, TracingCategory::kInfo> task_queues_throttled_;
  Vector<MainThreadTaskQueue::ThrottleHandle> throttled_task_queue_handles_;
  TraceableState<bool, TracingCategory::kInfo>
      preempted_for_cooperative_scheduling_;
  // TODO(https://crbug.com/827113): Trace the count of opt-outs.
  int aggressive_throttling_opt_out_count_;
  TraceableState<bool, TracingCategory::kInfo>
      opted_out_from_aggressive_throttling_;
  size_t subresource_loading_pause_count_;

  BackForwardCacheDisablingFeatureTracker
      back_forward_cache_disabling_feature_tracker_;

  TaskPriority default_loading_task_priority_ = TaskPriority::kNormalPriority;

  // These are the states of the Page.
  // They should be accessed via GetPageScheduler()->SetPageState().
  // they are here because we don't support page-level tracing yet.
  TraceableState<bool, TracingCategory::kInfo> page_frozen_for_tracing_;
  TraceableState<PageVisibilityState, TracingCategory::kInfo>
      page_visibility_for_tracing_;

  TraceableState<bool, TracingCategory::kInfo> waiting_for_contentful_paint_;
  TraceableState<bool, TracingCategory::kInfo> waiting_for_meaningful_paint_;

  // TODO(altimin): Remove after we have have 1:1 relationship between frames
  // and documents.
  base::WeakPtrFactory<FrameSchedulerImpl> document_bound_weak_factory_{this};

  // WeakPtrFactory for tracking IPCs posted to frames cached in the
  // back-forward cache. These weak pointers are invalidated when the page is
  // restored from the cache.
  base::WeakPtrFactory<FrameSchedulerImpl>
      invalidating_on_bfcache_restore_weak_factory_{this};

  mutable base::WeakPtrFactory<FrameSchedulerImpl> weak_factory_{this};
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_FRAME_SCHEDULER_IMPL_H_
