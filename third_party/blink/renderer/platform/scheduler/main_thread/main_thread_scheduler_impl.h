// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_MAIN_THREAD_SCHEDULER_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_MAIN_THREAD_SCHEDULER_IMPL_H_

#include <map>
#include <memory>
#include <random>
#include <set>
#include <string>

#include "base/atomicops.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/single_sample_metrics.h"
#include "base/optional.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/task/sequence_manager/task_queue.h"
#include "base/task/sequence_manager/task_time_observer.h"
#include "base/trace_event/trace_log.h"
#include "build/build_config.h"
#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/scheduler/common/idle_canceled_delayed_task_sweeper.h"
#include "third_party/blink/renderer/platform/scheduler/common/idle_helper.h"
#include "third_party/blink/renderer/platform/scheduler/common/pollable_thread_safe_flag.h"
#include "third_party/blink/renderer/platform/scheduler/common/thread_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/common/tracing_helper.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/auto_advancing_virtual_time_domain.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/deadline_task_runner.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/idle_time_estimator.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_metrics_helper.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_helper.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_task_queue.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/page_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/prioritize_compositing_after_input_experiment.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/queueing_time_estimator.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/render_widget_signals.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/task_cost_estimator.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/use_case.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/user_model.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"

namespace base {
namespace trace_event {
class ConvertableToTraceFormat;
}
}  // namespace base

namespace blink {
namespace scheduler {
namespace main_thread_scheduler_impl_unittest {
class MainThreadSchedulerImplForTest;
class MainThreadSchedulerImplTest;
FORWARD_DECLARE_TEST(MainThreadSchedulerImplTest, ShouldIgnoreTaskForUkm);
FORWARD_DECLARE_TEST(MainThreadSchedulerImplTest, Tracing);
}  // namespace main_thread_scheduler_impl_unittest
class PageSchedulerImpl;
class TaskQueueThrottler;
class WebRenderWidgetSchedulingState;

class PLATFORM_EXPORT MainThreadSchedulerImpl
    : public ThreadSchedulerImpl,
      public IdleHelper::Delegate,
      public MainThreadSchedulerHelper::Observer,
      public RenderWidgetSignals::Observer,
      public QueueingTimeEstimator::Client,
      public base::trace_event::TraceLog::AsyncEnabledStateObserver,
      public AutoAdvancingVirtualTimeDomain::Observer {
 public:
  // Don't use except for tracing.
  struct TaskDescriptionForTracing {
    TaskType task_type;
    base::Optional<MainThreadTaskQueue::QueueType> queue_type;

    // Required in order to wrap in TraceableState.
    constexpr bool operator!=(const TaskDescriptionForTracing& rhs) const {
      return task_type != rhs.task_type || queue_type != rhs.queue_type;
    }
  };

  struct SchedulingSettings {
    SchedulingSettings();

    // High priority input experiment.
    bool high_priority_input;

    // Background page priority experiment (crbug.com/848835).
    bool low_priority_background_page;
    bool best_effort_background_page;

    // Task and subframe priority experiment (crbug.com/852380).
    bool low_priority_subframe;
    bool low_priority_throttleable;
    bool low_priority_subframe_throttleable;
    bool low_priority_hidden_frame;

    // Used along with |low_priority_subframe|, |low_priority_throttleable|,
    // |low_priority_subframe_throttleable|, |low_priority_hidden_frame|
    // to enable one of these experiments during the loading phase only.
    bool use_frame_priorities_only_during_loading;

    // Ads priority experiment (crbug.com/856150).
    bool low_priority_ad_frame;
    bool best_effort_ad_frame;
    bool use_adframe_priorities_only_during_loading;

    // Origin type priority experiment (crbug.com/856158).
    bool low_priority_cross_origin;
    bool low_priority_cross_origin_only_during_loading;

    // Use resource fetch priority for resource loading tasks
    // (crbug.com/860545).
    bool use_resource_fetch_priority;
    bool use_resource_priorities_only_during_loading;

    // Contains a mapping from net::RequestPriority to TaskQueue::QueuePriority
    // when use_resource_fetch_priority is enabled.
    std::array<base::sequence_manager::TaskQueue::QueuePriority,
               net::RequestPrioritySize::NUM_PRIORITIES>
        net_to_blink_priority;

    using FrameTaskTypeToQueueTraitsArray =
        std::array<base::Optional<MainThreadTaskQueue::QueueTraits>,
                   static_cast<size_t>(TaskType::kCount)>;
    // Array of QueueTraits indexed by TaskType, containing TaskType::kCount
    // entries. This is initialized early with all valid entries. Entries that
    // aren't valid task types, i.e. non-frame level, are base::nullopt.
    FrameTaskTypeToQueueTraitsArray frame_task_types_to_queue_traits;

    bool disable_expensive_task_blocking;
    bool disable_non_touchstart_input_heuristics;
    bool disable_touchstart_input_heuristics;
  };

  static const char* UseCaseToString(UseCase use_case);
  static const char* RAILModeToString(v8::RAILMode rail_mode);
  static const char* VirtualTimePolicyToString(
      PageScheduler::VirtualTimePolicy);
  // The lowest bucket for fine-grained Expected Queueing Time reporting.
  static const int kMinExpectedQueueingTimeBucket = 1;
  // The highest bucket for fine-grained Expected Queueing Time reporting, in
  // microseconds.
  static const int kMaxExpectedQueueingTimeBucket = 30 * 1000 * 1000;
  // The number of buckets for fine-grained Expected Queueing Time reporting.
  static const int kNumberExpectedQueueingTimeBuckets = 50;

  // If |initial_virtual_time| is specified then the scheduler will be created
  // with virtual time enabled and paused with base::Time will be overridden to
  // start at |initial_virtual_time|.
  MainThreadSchedulerImpl(
      std::unique_ptr<base::sequence_manager::SequenceManager> sequence_manager,
      base::Optional<base::Time> initial_virtual_time);

  ~MainThreadSchedulerImpl() override;

  // WebThreadScheduler implementation:
  std::unique_ptr<Thread> CreateMainThread() override;
  scoped_refptr<SingleThreadIdleTaskRunner> IdleTaskRunner() override;
  // Note: this is also shared by the ThreadScheduler interface.
  scoped_refptr<base::SingleThreadTaskRunner> IPCTaskRunner() override;
  scoped_refptr<base::SingleThreadTaskRunner> CleanupTaskRunner() override;
  std::unique_ptr<WebRenderWidgetSchedulingState>
  NewRenderWidgetSchedulingState() override;
  void WillBeginFrame(const viz::BeginFrameArgs& args) override;
  void BeginFrameNotExpectedSoon() override;
  void BeginMainFrameNotExpectedUntil(base::TimeTicks time) override;
  void DidCommitFrameToCompositor() override;
  void DidHandleInputEventOnCompositorThread(
      const WebInputEvent& web_input_event,
      InputEventState event_state) override;
  void DidHandleInputEventOnMainThread(const WebInputEvent& web_input_event,
                                       WebInputEventResult result) override;
  void DidAnimateForInputOnCompositorThread() override;
  void SetRendererHidden(bool hidden) override;
  void SetRendererBackgrounded(bool backgrounded) override;
  void SetSchedulerKeepActive(bool keep_active) override;
  bool SchedulerKeepActive();
  void OnMainFrameRequestedForInput() override;
#if defined(OS_ANDROID)
  void PauseTimersForAndroidWebView() override;
  void ResumeTimersForAndroidWebView() override;
#endif
  std::unique_ptr<ThreadScheduler::RendererPauseHandle> PauseRenderer() override
      WARN_UNUSED_RESULT;
  bool IsHighPriorityWorkAnticipated() override;
  bool ShouldYieldForHighPriorityWork() override;
  bool CanExceedIdleDeadlineIfRequired() const override;
  void AddTaskObserver(base::MessageLoop::TaskObserver* task_observer) override;
  void RemoveTaskObserver(
      base::MessageLoop::TaskObserver* task_observer) override;
  void Shutdown() override;
  void SetTopLevelBlameContext(
      base::trace_event::BlameContext* blame_context) override;
  void AddRAILModeObserver(WebRAILModeObserver* observer) override;
  void SetRendererProcessType(RendererProcessType type) override;
  WebScopedVirtualTimePauser CreateWebScopedVirtualTimePauser(
      const char* name,
      WebScopedVirtualTimePauser::VirtualTaskDuration duration) override;

  // ThreadScheduler implementation:
  void PostIdleTask(const base::Location&, Thread::IdleTask) override;
  void PostNonNestableIdleTask(const base::Location&,
                               Thread::IdleTask) override;
  scoped_refptr<base::SingleThreadTaskRunner> V8TaskRunner() override;
  scoped_refptr<base::SingleThreadTaskRunner> CompositorTaskRunner() override;
  // IPCTaskRunner() is implemented above in the WebThreadScheduler section.
  std::unique_ptr<PageScheduler> CreatePageScheduler(
      PageScheduler::Delegate*) override;
  std::unique_ptr<ThreadScheduler::RendererPauseHandle> PauseScheduler()
      override;
  base::TimeTicks MonotonicallyIncreasingVirtualTime() override;
  WebThreadScheduler* GetWebMainThreadSchedulerForTest() override;
  NonMainThreadSchedulerImpl* AsNonMainThreadScheduler() override {
    return nullptr;
  }

  // WebThreadScheduler implementation:
  scoped_refptr<base::SingleThreadTaskRunner> DefaultTaskRunner() override;
  scoped_refptr<base::SingleThreadTaskRunner> InputTaskRunner() override;

  // The following functions are defined in both WebThreadScheduler and
  // ThreadScheduler, and have the same function signatures -- see above.
  // This class implements those functions for both base classes.
  //
  // void Shutdown() override;
  //
  // TODO(yutak): Reduce the overlaps and simplify.

  // AutoAdvancingVirtualTimeDomain::Observer implementation:
  void OnVirtualTimeAdvanced() override;

  // RenderWidgetSignals::Observer implementation:
  void SetAllRenderWidgetsHidden(bool hidden) override;
  void SetHasVisibleRenderWidgetWithTouchHandler(
      bool has_visible_render_widget_with_touch_handler) override;

  // SchedulerHelper::Observer implementation:
  void OnBeginNestedRunLoop() override;
  void OnExitNestedRunLoop() override;

  // QueueingTimeEstimator::Client implementation:
  void OnQueueingTimeForWindowEstimated(base::TimeDelta queueing_time,
                                        bool is_disjoint_window) override;
  void OnReportFineGrainedExpectedQueueingTime(
      const char* split_description,
      base::TimeDelta queueing_time) override;

  // ThreadSchedulerImpl implementation:
  scoped_refptr<base::SingleThreadTaskRunner> ControlTaskRunner() override;
  void RegisterTimeDomain(
      base::sequence_manager::TimeDomain* time_domain) override;
  void UnregisterTimeDomain(
      base::sequence_manager::TimeDomain* time_domain) override;
  base::sequence_manager::TimeDomain* GetActiveTimeDomain() override;
  const base::TickClock* GetTickClock() override;

  scoped_refptr<base::SingleThreadTaskRunner> VirtualTimeControlTaskRunner();

  // Returns a new task queue created with given params.
  scoped_refptr<MainThreadTaskQueue> NewTaskQueue(
      const MainThreadTaskQueue::QueueCreationParams& params);

  // Returns a new loading task queue. This queue is intended for tasks related
  // to resource dispatch, foreground HTML parsing, etc...
  // Note: Tasks posted to kFrameLoadingControl queues must execute quickly.
  scoped_refptr<MainThreadTaskQueue> NewLoadingTaskQueue(
      MainThreadTaskQueue::QueueType queue_type,
      FrameSchedulerImpl* frame_scheduler);

  // Returns a new timer task queue. This queue is intended for DOM Timers.
  scoped_refptr<MainThreadTaskQueue> NewTimerTaskQueue(
      MainThreadTaskQueue::QueueType queue_type,
      FrameSchedulerImpl* frame_scheduler);

  using VirtualTimePolicy = PageScheduler::VirtualTimePolicy;
  using VirtualTimeObserver = PageScheduler::VirtualTimeObserver;

  using BaseTimeOverridePolicy =
      AutoAdvancingVirtualTimeDomain::BaseTimeOverridePolicy;

  // Tells the scheduler that all TaskQueues should use virtual time. Depending
  // on the initial time, picks the policy to be either overriding or not.
  base::TimeTicks EnableVirtualTime();

  // Tells the scheduler that all TaskQueues should use virtual time. Returns
  // the TimeTicks that virtual time offsets will be relative to.
  base::TimeTicks EnableVirtualTime(BaseTimeOverridePolicy policy);
  bool IsVirtualTimeEnabled() const;

  // Migrates all task queues to real time.
  void DisableVirtualTimeForTesting();

  // Returns true if virtual time is not paused.
  bool VirtualTimeAllowedToAdvance() const;
  void SetVirtualTimePolicy(VirtualTimePolicy virtual_time_policy);
  void SetInitialVirtualTime(base::Time time);
  void SetInitialVirtualTimeOffset(base::TimeDelta offset);
  void SetMaxVirtualTimeTaskStarvationCount(int max_task_starvation_count);
  void AddVirtualTimeObserver(VirtualTimeObserver*);
  void RemoveVirtualTimeObserver(VirtualTimeObserver*);
  base::TimeTicks IncrementVirtualTimePauseCount();
  void DecrementVirtualTimePauseCount();
  void MaybeAdvanceVirtualTime(base::TimeTicks new_virtual_time);

  void AddPageScheduler(PageSchedulerImpl*);
  void RemovePageScheduler(PageSchedulerImpl*);

  void AddTaskTimeObserver(base::sequence_manager::TaskTimeObserver*);
  void RemoveTaskTimeObserver(base::sequence_manager::TaskTimeObserver*);

  // Snapshots this MainThreadSchedulerImpl for tracing.
  void CreateTraceEventObjectSnapshot() const;

  // Called when one of associated page schedulers has changed audio state.
  void OnAudioStateChanged();

  // Tells the scheduler that a provisional load has committed. Must be called
  // from the main thread.
  void DidStartProvisionalLoad(bool is_main_frame);

  // Tells the scheduler that a provisional load has committed. The scheduler
  // may reset the task cost estimators and the UserModel. Must be called from
  // the main thread.
  void DidCommitProvisionalLoad(bool is_web_history_inert_commit,
                                bool is_reload,
                                bool is_main_frame);

  // Note that the main's thread policy should be upto date to compute
  // the correct priority.
  base::sequence_manager::TaskQueue::QueuePriority ComputePriority(
      MainThreadTaskQueue* task_queue) const;

  // Test helpers.
  MainThreadSchedulerHelper* GetSchedulerHelperForTesting();
  TaskCostEstimator* GetLoadingTaskCostEstimatorForTesting();
  TaskCostEstimator* GetTimerTaskCostEstimatorForTesting();
  IdleTimeEstimator* GetIdleTimeEstimatorForTesting();
  base::TimeTicks CurrentIdleTaskDeadlineForTesting() const;
  void RunIdleTasksForTesting(base::OnceClosure callback);
  void EndIdlePeriodForTesting(base::OnceClosure callback,
                               base::TimeTicks time_remaining);
  bool PolicyNeedsUpdateForTesting();
  WakeUpBudgetPool* GetWakeUpBudgetPoolForTesting();

  const base::TickClock* tick_clock() const;

  base::sequence_manager::TimeDomain* real_time_domain() const {
    return helper_.real_time_domain();
  }

  AutoAdvancingVirtualTimeDomain* GetVirtualTimeDomain();

  TaskQueueThrottler* task_queue_throttler() const {
    return task_queue_throttler_.get();
  }

  void OnFirstMeaningfulPaint();

  void OnShutdownTaskQueue(const scoped_refptr<MainThreadTaskQueue>& queue);

  void OnTaskStarted(
      MainThreadTaskQueue* queue,
      const base::sequence_manager::Task& task,
      const base::sequence_manager::TaskQueue::TaskTiming& task_timing);

  void OnTaskCompleted(
      MainThreadTaskQueue* queue,
      const base::sequence_manager::Task& task,
      const base::sequence_manager::TaskQueue::TaskTiming& task_timing);

  bool IsAudioPlaying() const;

  // base::trace_event::TraceLog::EnabledStateObserver implementation:
  void OnTraceLogEnabled() override;
  void OnTraceLogDisabled() override;

  UseCase current_use_case() const;

  const SchedulingSettings& scheduling_settings() const;

  void SetShouldPrioritizeCompositing(bool should_prioritize_compositing);

  base::WeakPtr<MainThreadSchedulerImpl> GetWeakPtr();

 protected:
  scoped_refptr<MainThreadTaskQueue> ControlTaskQueue();
  scoped_refptr<MainThreadTaskQueue> DefaultTaskQueue();
  scoped_refptr<MainThreadTaskQueue> CompositorTaskQueue();
  scoped_refptr<MainThreadTaskQueue> InputTaskQueue();
  scoped_refptr<MainThreadTaskQueue> V8TaskQueue();
  // A control task queue which also respects virtual time. Only available if
  // virtual time has been enabled.
  scoped_refptr<MainThreadTaskQueue> VirtualTimeControlTaskQueue();

  // `current_use_case` will be overwritten by the next call to UpdatePolicy.
  // Thus, this function should be only used for testing purposes.
  void SetCurrentUseCaseForTest(UseCase use_case) {
    main_thread_only().current_use_case = use_case;
  }

  void SetHaveSeenABlockingGestureForTesting(bool status);

 private:
  friend class WebRenderWidgetSchedulingState;
  friend class MainThreadMetricsHelper;

  friend class MainThreadMetricsHelperTest;
  friend class main_thread_scheduler_impl_unittest::
      MainThreadSchedulerImplForTest;
  friend class main_thread_scheduler_impl_unittest::MainThreadSchedulerImplTest;

  FRIEND_TEST_ALL_PREFIXES(
      main_thread_scheduler_impl_unittest::MainThreadSchedulerImplTest,
      ShouldIgnoreTaskForUkm);
  FRIEND_TEST_ALL_PREFIXES(
      main_thread_scheduler_impl_unittest::MainThreadSchedulerImplTest,
      Tracing);

  enum class ExpensiveTaskPolicy { kRun, kBlock, kThrottle };

  enum class TimeDomainType {
    kReal,
    kThrottled,
    kVirtual,
  };

  static const char* TimeDomainTypeToString(TimeDomainType domain_type);

  bool ContainsLocalMainFrame();

  struct TaskQueuePolicy {
    // Default constructor of TaskQueuePolicy should match behaviour of a
    // newly-created task queue.
    TaskQueuePolicy()
        : is_enabled(true),
          is_paused(false),
          is_throttled(false),
          is_deferred(false),
          use_virtual_time(false) {}

    bool is_enabled;
    bool is_paused;
    bool is_throttled;
    bool is_deferred;
    bool use_virtual_time;

    bool IsQueueEnabled(MainThreadTaskQueue* task_queue) const;

    TimeDomainType GetTimeDomainType(MainThreadTaskQueue* task_queue) const;

    bool operator==(const TaskQueuePolicy& other) const {
      return is_enabled == other.is_enabled && is_paused == other.is_paused &&
             is_throttled == other.is_throttled &&
             is_deferred == other.is_deferred &&
             use_virtual_time == other.use_virtual_time;
    }

    void AsValueInto(base::trace_event::TracedValue* state) const;
  };

  class Policy {
   public:
    Policy()
        : rail_mode_(v8::PERFORMANCE_ANIMATION),
          should_disable_throttling_(false),
          frozen_when_backgrounded_(false),
          compositor_priority_(base::sequence_manager::TaskQueue::
                                   QueuePriority::kNormalPriority),
          use_case_(UseCase::kNone) {}

    ~Policy() = default;

    TaskQueuePolicy& compositor_queue_policy() {
      return policies_[static_cast<size_t>(
          MainThreadTaskQueue::QueueClass::kCompositor)];
    }
    const TaskQueuePolicy& compositor_queue_policy() const {
      return policies_[static_cast<size_t>(
          MainThreadTaskQueue::QueueClass::kCompositor)];
    }

    TaskQueuePolicy& loading_queue_policy() {
      return policies_[static_cast<size_t>(
          MainThreadTaskQueue::QueueClass::kLoading)];
    }
    const TaskQueuePolicy& loading_queue_policy() const {
      return policies_[static_cast<size_t>(
          MainThreadTaskQueue::QueueClass::kLoading)];
    }

    TaskQueuePolicy& timer_queue_policy() {
      return policies_[static_cast<size_t>(
          MainThreadTaskQueue::QueueClass::kTimer)];
    }
    const TaskQueuePolicy& timer_queue_policy() const {
      return policies_[static_cast<size_t>(
          MainThreadTaskQueue::QueueClass::kTimer)];
    }

    TaskQueuePolicy& default_queue_policy() {
      return policies_[static_cast<size_t>(
          MainThreadTaskQueue::QueueClass::kNone)];
    }
    const TaskQueuePolicy& default_queue_policy() const {
      return policies_[static_cast<size_t>(
          MainThreadTaskQueue::QueueClass::kNone)];
    }

    const TaskQueuePolicy& GetQueuePolicy(
        MainThreadTaskQueue::QueueClass queue_class) const {
      return policies_[static_cast<size_t>(queue_class)];
    }

    v8::RAILMode& rail_mode() { return rail_mode_; }
    v8::RAILMode rail_mode() const { return rail_mode_; }

    bool& should_disable_throttling() { return should_disable_throttling_; }
    bool should_disable_throttling() const {
      return should_disable_throttling_;
    }

    bool& frozen_when_backgrounded() { return frozen_when_backgrounded_; }
    bool frozen_when_backgrounded() const { return frozen_when_backgrounded_; }

    base::sequence_manager::TaskQueue::QueuePriority& compositor_priority() {
      return compositor_priority_;
    }
    base::sequence_manager::TaskQueue::QueuePriority compositor_priority()
        const {
      return compositor_priority_;
    }

    UseCase& use_case() { return use_case_; }
    UseCase use_case() const { return use_case_; }

    bool operator==(const Policy& other) const {
      return policies_ == other.policies_ && rail_mode_ == other.rail_mode_ &&
             should_disable_throttling_ == other.should_disable_throttling_ &&
             frozen_when_backgrounded_ == other.frozen_when_backgrounded_ &&
             compositor_priority_ == other.compositor_priority_ &&
             use_case_ == other.use_case_;
    }

    void AsValueInto(base::trace_event::TracedValue* state) const;

   private:
    v8::RAILMode rail_mode_;
    bool should_disable_throttling_;
    bool frozen_when_backgrounded_;

    // Priority of task queues belonging to the compositor class (Check
    // MainThread::QueueClass).
    base::sequence_manager::TaskQueue::QueuePriority compositor_priority_;

    UseCase use_case_;

    std::array<TaskQueuePolicy,
               static_cast<size_t>(MainThreadTaskQueue::QueueClass::kCount)>
        policies_;
  };

  class PollableNeedsUpdateFlag {
   public:
    explicit PollableNeedsUpdateFlag(base::Lock* write_lock);
    ~PollableNeedsUpdateFlag();

    // Set the flag. May only be called if |write_lock| is held.
    void SetWhileLocked(bool value);

    // Returns true iff the flag is set to true.
    bool IsSet() const;

   private:
    base::subtle::Atomic32 flag_;
    base::Lock* write_lock_;  // Not owned.

    DISALLOW_COPY_AND_ASSIGN(PollableNeedsUpdateFlag);
  };

  class TaskDurationMetricTracker;

  class RendererPauseHandleImpl : public ThreadScheduler::RendererPauseHandle {
   public:
    explicit RendererPauseHandleImpl(MainThreadSchedulerImpl* scheduler);
    ~RendererPauseHandleImpl() override;

   private:
    MainThreadSchedulerImpl* scheduler_;  // NOT OWNED
  };

  // ThreadSchedulerImpl implementation:
  SchedulerHelper* GetHelper() override;

  // IdleHelper::Delegate implementation:
  bool CanEnterLongIdlePeriod(
      base::TimeTicks now,
      base::TimeDelta* next_long_idle_period_delay_out) override;
  void IsNotQuiescent() override {}
  void OnIdlePeriodStarted() override;
  void OnIdlePeriodEnded() override;
  void OnPendingTasksChanged(bool has_tasks) override;

  void DispatchRequestBeginMainFrameNotExpected(bool has_tasks);

  void EndIdlePeriod();

  // Update a policy which increases priority for the next beginMainFrame after
  // an input event.
  void UpdatePrioritizeCompositingAfterInputAfterTaskCompleted(
      MainThreadTaskQueue* queue);

  // Returns the serialized scheduler state for tracing.
  std::unique_ptr<base::trace_event::ConvertableToTraceFormat> AsValue(
      base::TimeTicks optional_now) const;
  std::unique_ptr<base::trace_event::ConvertableToTraceFormat> AsValueLocked(
      base::TimeTicks optional_now) const;
  void CreateTraceEventObjectSnapshotLocked() const;

  static bool ShouldPrioritizeInputEvent(const WebInputEvent& web_input_event);

  // The amount of time which idle periods can continue being scheduled when the
  // renderer has been hidden, before going to sleep for good.
  static const int kEndIdleWhenHiddenDelayMillis = 10000;

  // The amount of time in milliseconds we have to respond to user input as
  // defined by RAILS.
  static const int kRailsResponseTimeMillis = 50;

  // The time we should stay in a priority-escalated mode after a call to
  // DidAnimateForInputOnCompositorThread().
  static const int kFlingEscalationLimitMillis = 100;

  // Schedules an immediate PolicyUpdate, if there isn't one already pending and
  // sets |policy_may_need_update_|. Note |any_thread_lock_| must be
  // locked.
  void EnsureUrgentPolicyUpdatePostedOnMainThread(
      const base::Location& from_here);

  // Update the policy if a new signal has arrived. Must be called from the main
  // thread.
  void MaybeUpdatePolicy();

  // Locks |any_thread_lock_| and updates the scheduler policy.  May early
  // out if the policy is unchanged. Must be called from the main thread.
  void UpdatePolicy();

  // Like UpdatePolicy, except it doesn't early out.
  void ForceUpdatePolicy();

  enum class UpdateType {
    kMayEarlyOutIfPolicyUnchanged,
    kForceUpdate,
  };

  // The implementation of UpdatePolicy & ForceUpdatePolicy.  It is allowed to
  // early out if |update_type| is kMayEarlyOutIfPolicyUnchanged.
  virtual void UpdatePolicyLocked(UpdateType update_type);

  // Helper for computing the use case. |expected_usecase_duration| will be
  // filled with the amount of time after which the use case should be updated
  // again. If the duration is zero, a new use case update should not be
  // scheduled. Must be called with |any_thread_lock_| held. Can be called from
  // any thread.
  UseCase ComputeCurrentUseCase(
      base::TimeTicks now,
      base::TimeDelta* expected_use_case_duration) const;

  std::unique_ptr<base::SingleSampleMetric> CreateMaxQueueingTimeMetric();

  // An input event of some sort happened, the policy may need updating.
  void UpdateForInputEventOnCompositorThread(const WebInputEvent& event,
                                             InputEventState input_event_state);

  // The task cost estimators and the UserModel need to be reset upon page
  // nagigation. This function does that. Must be called from the main thread.
  void ResetForNavigationLocked();

  // Estimates the maximum task length that won't cause a jank based on the
  // current system state. Must be called from the main thread.
  base::TimeDelta EstimateLongestJankFreeTaskDuration() const;

  // Report an intervention to all WebViews in this process.
  void BroadcastIntervention(const std::string& message);

  void ApplyTaskQueuePolicy(
      MainThreadTaskQueue* task_queue,
      base::sequence_manager::TaskQueue::QueueEnabledVoter*
          task_queue_enabled_voter,
      const TaskQueuePolicy& old_task_queue_policy,
      const TaskQueuePolicy& new_task_queue_policy) const;

  static const char* ExpensiveTaskPolicyToString(
      ExpensiveTaskPolicy expensive_task_policy);

  void AddQueueToWakeUpBudgetPool(MainThreadTaskQueue* queue);

  void PauseRendererImpl();
  void ResumeRendererImpl();

  void NotifyVirtualTimePaused();
  void SetVirtualTimeStopped(bool virtual_time_stopped);
  void ApplyVirtualTimePolicy();

  // Pauses the timer queues by inserting a fence that blocks any tasks posted
  // after this point from running. Orthogonal to PauseTimerQueue. Care must
  // be taken when using this API to avoid fighting with the TaskQueueThrottler.
  void VirtualTimePaused();

  // Removes the fence added by VirtualTimePaused allowing timers to execute
  // normally. Care must be taken when using this API to avoid fighting with the
  // TaskQueueThrottler.
  void VirtualTimeResumed();

  // Returns true if there is a change in the main thread's policy that should
  // trigger a priority update.
  bool ShouldUpdateTaskQueuePriorities(Policy new_policy) const;

  static void RunIdleTask(Thread::IdleTask, base::TimeTicks deadline);

  // Probabilistically record all task metadata for the current task.
  // If task belongs to a per-frame queue, this task is attributed to
  // a particular Page, otherwise it's attributed to all Pages in the process.
  void RecordTaskUkm(
      MainThreadTaskQueue* queue,
      const base::sequence_manager::Task& task,
      const base::sequence_manager::TaskQueue::TaskTiming& task_timing);

  UkmRecordingStatus RecordTaskUkmImpl(
      MainThreadTaskQueue* queue,
      const base::sequence_manager::Task& task,
      const base::sequence_manager::TaskQueue::TaskTiming& task_timing,
      FrameSchedulerImpl* frame_scheduler,
      bool precise_attribution);

  void InitWakeUpBudgetPoolIfNeeded();

  void SetNumberOfCompositingTasksToPrioritize(int number_of_tasks);

  // Indicates that scheduler has been shutdown.
  // It should be accessed only on the main thread, but couldn't be a member
  // of MainThreadOnly struct because last might be destructed before we
  // have to check this flag during scheduler's destruction.
  bool was_shutdown_ = false;

  // This controller should be initialized before any TraceableVariables
  // because they require one to initialize themselves.
  TraceableVariableController tracing_controller_;

  // Used for experiments on finch. On main thread instantiation, we cache
  // the values of base::Feature flags using this struct, since calling
  // base::Feature::IsEnabled is a relatively expensive operation.
  //
  // Note that it is important to keep this as the first member to ensure it is
  // initialized first and can be used everywhere.
  const SchedulingSettings scheduling_settings_;

  MainThreadSchedulerHelper helper_;
  IdleHelper idle_helper_;
  IdleCanceledDelayedTaskSweeper idle_canceled_delayed_task_sweeper_;
  std::unique_ptr<TaskQueueThrottler> task_queue_throttler_;
  RenderWidgetSignals render_widget_scheduler_signals_;

  const scoped_refptr<MainThreadTaskQueue> control_task_queue_;
  const scoped_refptr<MainThreadTaskQueue> compositor_task_queue_;
  const scoped_refptr<MainThreadTaskQueue> input_task_queue_;
  scoped_refptr<MainThreadTaskQueue> virtual_time_control_task_queue_;
  std::unique_ptr<base::sequence_manager::TaskQueue::QueueEnabledVoter>
      compositor_task_queue_enabled_voter_;
  std::unique_ptr<base::sequence_manager::TaskQueue::QueueEnabledVoter>
      input_task_queue_enabled_voter_;

  using TaskQueueVoterMap = std::map<
      scoped_refptr<MainThreadTaskQueue>,
      std::unique_ptr<base::sequence_manager::TaskQueue::QueueEnabledVoter>>;

  TaskQueueVoterMap task_runners_;

  scoped_refptr<MainThreadTaskQueue> v8_task_queue_;
  scoped_refptr<MainThreadTaskQueue> ipc_task_queue_;
  scoped_refptr<MainThreadTaskQueue> cleanup_task_queue_;

  scoped_refptr<base::SingleThreadTaskRunner> v8_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> control_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> input_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> ipc_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> cleanup_task_runner_;

  // Note |virtual_time_domain_| is lazily created.
  std::unique_ptr<AutoAdvancingVirtualTimeDomain> virtual_time_domain_;

  base::Closure update_policy_closure_;
  DeadlineTaskRunner delayed_update_policy_runner_;
  CancelableClosureHolder end_renderer_hidden_idle_period_closure_;

  QueueingTimeEstimator queueing_time_estimator_;

  // We have decided to improve thread safety at the cost of some boilerplate
  // (the accessors) for the following data members.

  struct MainThreadOnly {
    MainThreadOnly(
        MainThreadSchedulerImpl* main_thread_scheduler_impl,
        const scoped_refptr<MainThreadTaskQueue>& compositor_task_runner,
        const base::TickClock* time_source,
        base::TimeTicks now);
    ~MainThreadOnly();

    TaskCostEstimator loading_task_cost_estimator;
    TaskCostEstimator timer_task_cost_estimator;
    IdleTimeEstimator idle_time_estimator;
    TraceableState<UseCase, kTracingCategoryNameDefault> current_use_case;
    Policy current_policy;
    base::TimeTicks current_policy_expiration_time;
    base::TimeTicks estimated_next_frame_begin;
    base::TimeTicks current_task_start_time;
    base::TimeDelta compositor_frame_interval;
    TraceableCounter<base::TimeDelta, kTracingCategoryNameDebug>
        longest_jank_free_task_duration;
    TraceableCounter<int, kTracingCategoryNameInfo>
        renderer_pause_count;  // Renderer is paused if non-zero.
    TraceableState<ExpensiveTaskPolicy, kTracingCategoryNameInfo>
        expensive_task_policy;
    TraceableState<v8::RAILMode, kTracingCategoryNameInfo>
        rail_mode_for_tracing;  // Don't use except for tracing.
    TraceableState<bool, kTracingCategoryNameDebug> renderer_hidden;
    TraceableState<bool, kTracingCategoryNameTopLevel> renderer_backgrounded;
    TraceableState<bool, kTracingCategoryNameDefault>
        keep_active_fetch_or_worker;
    TraceableCounter<base::TimeDelta, kTracingCategoryNameInfo>
        loading_task_estimated_cost;
    TraceableCounter<base::TimeDelta, kTracingCategoryNameInfo>
        timer_task_estimated_cost;
    TraceableState<bool, kTracingCategoryNameInfo> loading_tasks_seem_expensive;
    TraceableState<bool, kTracingCategoryNameInfo> timer_tasks_seem_expensive;
    TraceableState<bool, kTracingCategoryNameDefault>
        blocking_input_expected_soon;
    TraceableState<bool, kTracingCategoryNameDebug>
        have_seen_a_begin_main_frame;
    TraceableState<bool, kTracingCategoryNameDebug>
        have_reported_blocking_intervention_in_current_policy;
    TraceableState<bool, kTracingCategoryNameDebug>
        have_reported_blocking_intervention_since_navigation;
    TraceableState<bool, kTracingCategoryNameDebug>
        has_visible_render_widget_with_touch_handler;
    TraceableState<bool, kTracingCategoryNameDebug>
        begin_frame_not_expected_soon;
    TraceableState<bool, kTracingCategoryNameDebug> in_idle_period_for_testing;
    TraceableState<bool, kTracingCategoryNameInfo> use_virtual_time;
    TraceableState<bool, kTracingCategoryNameTopLevel> is_audio_playing;
    TraceableState<bool, kTracingCategoryNameDebug>
        compositor_will_send_main_frame_not_expected;
    TraceableState<bool, kTracingCategoryNameDebug> has_navigated;
    TraceableState<bool, kTracingCategoryNameDebug> pause_timers_for_webview;
    std::unique_ptr<base::SingleSampleMetric> max_queueing_time_metric;
    base::TimeDelta max_queueing_time;
    base::TimeTicks background_status_changed_at;
    std::set<PageSchedulerImpl*> page_schedulers;                 // Not owned.
    base::ObserverList<WebRAILModeObserver>::Unchecked
        rail_mode_observers;                                      // Not owned.
    WakeUpBudgetPool* wake_up_budget_pool;                        // Not owned.
    MainThreadMetricsHelper metrics_helper;
    TraceableState<RendererProcessType, kTracingCategoryNameTopLevel>
        process_type;
    TraceableState<base::Optional<TaskDescriptionForTracing>,
                   kTracingCategoryNameInfo>
        task_description_for_tracing;  // Don't use except for tracing.
    TraceableState<
        base::Optional<base::sequence_manager::TaskQueue::QueuePriority>,
        kTracingCategoryNameInfo>
        task_priority_for_tracing;  // Only used for tracing.
    base::ObserverList<VirtualTimeObserver>::Unchecked virtual_time_observers;
    base::Time initial_virtual_time;
    base::TimeTicks initial_virtual_time_ticks;

    // This is used for cross origin navigations to account for virtual time
    // advancing in the previous renderer.
    base::TimeDelta initial_virtual_time_offset;
    VirtualTimePolicy virtual_time_policy;

    // In VirtualTimePolicy::kDeterministicLoading virtual time is only allowed
    // to advance if this is zero.
    int virtual_time_pause_count;

    // The maximum number amount of delayed task starvation we will allow in
    // VirtualTimePolicy::kAdvance or VirtualTimePolicy::kDeterministicLoading
    // unless the run_loop is nested (in which case infinite starvation is
    // allowed). NB a value of 0 allows infinite starvation.
    int max_virtual_time_task_starvation_count;
    bool virtual_time_stopped;

    // Holds task queues that are currently running.
    // The queue for the inmost task is at the top of stack when there are
    // nested RunLoops.
    std::stack<scoped_refptr<MainThreadTaskQueue>,
               std::vector<scoped_refptr<MainThreadTaskQueue>>>
        running_queues;

    // True if a nested RunLoop is running.
    bool nested_runloop;

    // High-priority for compositing events after input experiment.
    PrioritizeCompositingAfterInputExperiment compositing_experiment;
    bool should_prioritize_compositing;
  };

  struct AnyThread {
    explicit AnyThread(MainThreadSchedulerImpl* main_thread_scheduler_impl);
    ~AnyThread();

    base::TimeTicks last_idle_period_end_time;
    base::TimeTicks fling_compositor_escalation_deadline;
    UserModel user_model;
    TraceableState<bool, kTracingCategoryNameInfo>
        awaiting_touch_start_response;
    TraceableState<bool, kTracingCategoryNameInfo> in_idle_period;
    TraceableState<bool, kTracingCategoryNameInfo>
        begin_main_frame_on_critical_path;
    TraceableState<bool, kTracingCategoryNameInfo>
        last_gesture_was_compositor_driven;
    TraceableState<bool, kTracingCategoryNameInfo> default_gesture_prevented;
    TraceableState<bool, kTracingCategoryNameInfo> have_seen_a_blocking_gesture;
    TraceableState<bool, kTracingCategoryNameInfo> waiting_for_meaningful_paint;
    TraceableState<bool, kTracingCategoryNameInfo>
        have_seen_input_since_navigation;
  };

  struct CompositorThreadOnly {
    CompositorThreadOnly();
    ~CompositorThreadOnly();

    WebInputEvent::Type last_input_type;
    std::unique_ptr<base::ThreadChecker> compositor_thread_checker;

    void CheckOnValidThread() {
#if DCHECK_IS_ON()
      // We don't actually care which thread this called from, just so long as
      // its consistent.
      if (!compositor_thread_checker)
        compositor_thread_checker.reset(new base::ThreadChecker());
      DCHECK(compositor_thread_checker->CalledOnValidThread());
#endif
    }
  };

  // Don't access main_thread_only_, instead use MainThreadOnly().
  MainThreadOnly main_thread_only_;
  MainThreadOnly& main_thread_only() {
    helper_.CheckOnValidThread();
    return main_thread_only_;
  }
  const struct MainThreadOnly& main_thread_only() const {
    helper_.CheckOnValidThread();
    return main_thread_only_;
  }

  mutable base::Lock any_thread_lock_;
  // Don't access any_thread_, instead use AnyThread().
  AnyThread any_thread_;
  AnyThread& any_thread() {
    any_thread_lock_.AssertAcquired();
    return any_thread_;
  }
  const struct AnyThread& any_thread() const {
    any_thread_lock_.AssertAcquired();
    return any_thread_;
  }

  // Don't access compositor_thread_only_, instead use CompositorThreadOnly().
  CompositorThreadOnly compositor_thread_only_;
  CompositorThreadOnly& GetCompositorThreadOnly() {
    compositor_thread_only_.CheckOnValidThread();
    return compositor_thread_only_;
  }

  PollableThreadSafeFlag policy_may_need_update_;

  base::WeakPtrFactory<MainThreadSchedulerImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MainThreadSchedulerImpl);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_MAIN_THREAD_SCHEDULER_IMPL_H_
