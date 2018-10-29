// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_FRAME_SCHEDULER_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_FRAME_SCHEDULER_IMPL_H_

#include <array>
#include <memory>
#include <utility>

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
#include "third_party/blink/renderer/platform/scheduler/main_thread/frame_origin_type.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/frame_task_queue_controller.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_task_queue.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/page_visibility_state.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
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
  static std::unique_ptr<FrameSchedulerImpl> Create(
      PageSchedulerImpl* page_scheduler,
      FrameScheduler::Delegate* delegate,
      base::trace_event::BlameContext* blame_context,
      FrameScheduler::FrameType frame_type);
  ~FrameSchedulerImpl() override;

  // FrameScheduler implementation:
  void SetFrameVisible(bool frame_visible) override;
  bool IsFrameVisible() const override;

  bool IsPageVisible() const override;
  bool IsAudioPlaying() const;

  void SetPaused(bool frame_paused) override;

  void SetCrossOrigin(bool cross_origin) override;
  bool IsCrossOrigin() const override;

  void SetIsAdFrame() override;
  bool IsAdFrame() const override;

  void TraceUrlChange(const String& url) override;
  FrameScheduler::FrameType GetFrameType() const override;
  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner(TaskType) override;

  // Returns a wrapper around an instance of MainThreadTaskQueue which is
  // maintained in |resource_loading_task_queues_| map. The main thread task
  // queue is removed from the map and detached from both the main thread and
  // the frame schedulers when the wrapper instance goes out of scope.
  std::unique_ptr<WebResourceLoadingTaskRunnerHandle>
  CreateResourceLoadingTaskRunnerHandle() override;

  PageScheduler* GetPageScheduler() const override;
  void DidStartProvisionalLoad(bool is_main_frame) override;
  void DidCommitProvisionalLoad(bool is_web_history_inert_commit,
                                bool is_reload,
                                bool is_main_frame) override;
  WebScopedVirtualTimePauser CreateWebScopedVirtualTimePauser(
      const WTF::String& name,
      WebScopedVirtualTimePauser::VirtualTaskDuration duration) override;
  void OnFirstMeaningfulPaint() override;
  std::unique_ptr<ActiveConnectionHandle> OnActiveConnectionCreated() override;
  void AsValueInto(base::trace_event::TracedValue* state) const;
  bool IsExemptFromBudgetBasedThrottling() const override;
  std::unique_ptr<blink::mojom::blink::PauseSubresourceLoadingHandle>
  GetPauseSubresourceLoadingHandle() override;

  scoped_refptr<base::SingleThreadTaskRunner> ControlTaskRunner();

  void UpdatePolicy();

  bool has_active_connection() const { return has_active_connection_; }

  void OnTraceLogEnabled() { tracing_controller_.OnTraceLogEnabled(); }

  void SetPageVisibilityForTracing(PageVisibilityState page_visibility);
  void SetPageKeepActiveForTracing(bool keep_active);
  void SetPageFrozenForTracing(bool frozen);

  // Computes the priority of |task_queue| if it is associated to this frame
  // scheduler. Note that the main's thread policy should be upto date to
  // compute the correct priority.
  base::sequence_manager::TaskQueue::QueuePriority ComputePriority(
      MainThreadTaskQueue* task_queue) const;

  ukm::SourceId GetUkmSourceId() override;
  ukm::UkmRecorder* GetUkmRecorder();

  // FrameTaskQueueController::Delegate implementation.
  void OnTaskQueueCreated(
      MainThreadTaskQueue*,
      base::sequence_manager::TaskQueue::QueueEnabledVoter*) override;

  using FrameTaskTypeToQueueTraitsArray =
      std::array<base::Optional<MainThreadTaskQueue::QueueTraits>,
                 static_cast<size_t>(TaskType::kCount)>;

  // Initializes the mapping from TaskType to QueueTraits for frame-level tasks.
  // We control the policy and initialize this, but the map is stored with main
  // thread scheduling settings to avoid redundancy.
  static void InitializeTaskTypeQueueTraitsMap(
      FrameTaskTypeToQueueTraitsArray&);

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
  friend class ResourceLoadingTaskRunnerHandleImpl;

  class ActiveConnectionHandleImpl : public ActiveConnectionHandle {
   public:
    ActiveConnectionHandleImpl(FrameSchedulerImpl* frame_scheduler);
    ~ActiveConnectionHandleImpl() override;

   private:
    base::WeakPtr<FrameOrWorkerScheduler> frame_scheduler_;

    DISALLOW_COPY_AND_ASSIGN(ActiveConnectionHandleImpl);
  };

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
  void RemoveThrottleableQueueFromBackgroundCPUTimeBudgetPool(
      MainThreadTaskQueue*);
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

  void DidOpenActiveConnection();
  void DidCloseActiveConnection();

  void AddPauseSubresourceLoadingHandle();
  void RemovePauseSubresourceLoadingHandle();

  std::unique_ptr<ResourceLoadingTaskRunnerHandleImpl>
  CreateResourceLoadingTaskRunnerHandleImpl();

  FrameTaskQueueController* FrameTaskQueueControllerForTest() {
    return frame_task_queue_controller_.get();
  }

  // Create the QueueTraits for a specific TaskType. This returns base::nullopt
  // for loading tasks and non-frame-level tasks.
  static base::Optional<MainThreadTaskQueue::QueueTraits>
      CreateQueueTraitsForTaskType(TaskType);

  // Create QueueTraits for the default (non-finch) task queues.
  static MainThreadTaskQueue::QueueTraits ThrottleableTaskQueueTraits();
  static MainThreadTaskQueue::QueueTraits DeferrableTaskQueueTraits();
  static MainThreadTaskQueue::QueueTraits PausableTaskQueueTraits();
  static MainThreadTaskQueue::QueueTraits UnpausableTaskQueueTraits();
  static MainThreadTaskQueue::QueueTraits ForegroundOnlyTaskQueueTraits();

  const FrameScheduler::FrameType frame_type_;

  bool is_ad_frame_;

  TraceableVariableController tracing_controller_;
  std::unique_ptr<FrameTaskQueueController> frame_task_queue_controller_;

  using ResourceLoadingTaskQueuePriorityMap =
      WTF::HashMap<scoped_refptr<MainThreadTaskQueue>,
                   base::sequence_manager::TaskQueue::QueuePriority>;

  // Queue to priority map of resource loading task queues created by
  // |frame_task_queue_controller_| via CreateResourceLoadingTaskRunnerHandle.
  ResourceLoadingTaskQueuePriorityMap resource_loading_task_queue_priorities_;

  MainThreadSchedulerImpl* main_thread_scheduler_;  // NOT OWNED
  PageSchedulerImpl* parent_page_scheduler_;        // NOT OWNED
  FrameScheduler::Delegate* delegate_;              // NOT OWNED
  base::trace_event::BlameContext* blame_context_;  // NOT OWNED
  SchedulingLifecycleState throttling_state_;
  TraceableState<bool, kTracingCategoryNameInfo> frame_visible_;
  TraceableState<bool, kTracingCategoryNameInfo> frame_paused_;
  TraceableState<FrameOriginType, kTracingCategoryNameInfo> frame_origin_type_;
  TraceableState<bool, kTracingCategoryNameInfo> subresource_loading_paused_;
  StateTracer<kTracingCategoryNameInfo> url_tracer_;
  TraceableState<bool, kTracingCategoryNameInfo> task_queues_throttled_;
  // TODO(kraynov): https://crbug.com/827113
  // Trace active connection count.
  int active_connection_count_;
  size_t subresource_loading_pause_count_;
  TraceableState<bool, kTracingCategoryNameInfo> has_active_connection_;

  // These are the states of the Page.
  // They should be accessed via GetPageScheduler()->SetPageState().
  // they are here because we don't support page-level tracing yet.
  TraceableState<bool, kTracingCategoryNameInfo> page_frozen_for_tracing_;
  TraceableState<PageVisibilityState, kTracingCategoryNameInfo>
      page_visibility_for_tracing_;
  TraceableState<bool, kTracingCategoryNameInfo> page_keep_active_for_tracing_;

  base::WeakPtrFactory<FrameSchedulerImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FrameSchedulerImpl);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_FRAME_SCHEDULER_IMPL_H_
