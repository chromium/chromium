// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_MAIN_THREAD_TASK_QUEUE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_MAIN_THREAD_TASK_QUEUE_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/task/sequence_manager/task_queue.h"
#include "base/task/sequence_manager/task_queue_impl.h"
#include "net/base/request_priority.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_priority.h"

namespace base {
namespace sequence_manager {
class SequenceManager;
}
}  // namespace base

namespace blink {
namespace scheduler {

namespace main_thread_scheduler_impl_unittest {
class MainThreadSchedulerImplTest;
}

namespace agent_interference_recorder_test {
class AgentInterferenceRecorderTest;
}

class FrameSchedulerImpl;
class MainThreadSchedulerImpl;

class PLATFORM_EXPORT MainThreadTaskQueue
    : public base::sequence_manager::TaskQueue {
 public:
  enum class QueueType {
    // Keep MainThreadTaskQueue::NameForQueueType in sync.
    // This enum is used for a histogram and it should not be re-numbered.
    // TODO(altimin): Clean up obsolete names and use a new histogram when
    // the situation settles.
    kControl = 0,
    kDefault = 1,

    // 2 was used for default loading task runner but this was deprecated.

    // 3 was used for default timer task runner but this was deprecated.

    // 4: kUnthrottled, obsolete.

    kFrameLoading = 5,
    // 6 : kFrameThrottleable, replaced with FRAME_THROTTLEABLE.
    // 7 : kFramePausable, replaced with kFramePausable
    kCompositor = 8,
    kIdle = 9,
    kTest = 10,
    kFrameLoadingControl = 11,
    kFrameThrottleable = 12,
    kFrameDeferrable = 13,
    kFramePausable = 14,
    kFrameUnpausable = 15,
    kV8 = 16,
    // 17 : kIPC, obsolete
    kInput = 18,

    // Detached is used in histograms for tasks which are run after frame
    // is detached and task queue is gracefully shutdown.
    // TODO(altimin): Move to the top when histogram is renumbered.
    kDetached = 19,

    // 20 : kCleanup, obsolete.
    // 21 : kWebSchedulingUserInteraction, obsolete.
    // 22 : kWebSchedulingBestEffort, obsolete.

    kWebScheduling = 24,
    kNonWaking = 25,

    // Used to group multiple types when calculating Expected Queueing Time.
    kOther = 23,
    kCount = 26
  };

  // Returns name of the given queue type. Returned string has application
  // lifetime.
  static const char* NameForQueueType(QueueType queue_type);

  // Returns true if task queues of the given queue type can be created on a
  // per-frame basis, and false if they are only created on a shared basis for
  // the entire main thread.
  static bool IsPerFrameTaskQueue(QueueType);

  using QueueTraitsKeyType = int;

  // QueueTraits represent the deferrable, throttleable, pausable, and freezable
  // properties of a MainThreadTaskQueue. For non-loading task queues, there
  // will be at most one task queue with a specific set of QueueTraits, and the
  // the QueueTraits determine which queues should be used to run which task
  // types.
  struct QueueTraits {
    QueueTraits()
        : can_be_deferred(false),
          can_be_throttled(false),
          can_be_paused(false),
          can_be_frozen(false),
          can_run_in_background(true),
          can_run_when_virtual_time_paused(true),
          can_be_paused_for_android_webview(false) {}

    // Separate enum class for handling prioritisation decisions in task queues.
    enum class PrioritisationType {
      kInternalScriptContinuation = 0,
      kBestEffort = 1,
      kRegular = 2,
      kLoading = 3,
      kLoadingControl = 4,
      kFindInPage = 5,
      kExperimentalDatabase = 6,
      kJavaScriptTimer = 7,
      kHighPriorityLocalFrame = 8,
      kCompositor = 9,  // Main-thread only.

      kCount = 10
    };

    // kPrioritisationTypeWidthBits is the number of bits required
    // for PrioritisationType::kCount - 1, which is the number of bits needed
    // to represent |prioritisation_type| in QueueTraitKeyType.
    // We need to update it whenever there is a change in
    // PrioritisationType::kCount.
    // TODO(sreejakshetty) make the number of bits calculation automated.
    static constexpr int kPrioritisationTypeWidthBits = 4;
    static_assert(static_cast<int>(PrioritisationType::kCount) <=
                      (1 << kPrioritisationTypeWidthBits),
                  "Wrong Instanstiation for kPrioritisationTypeWidthBits");

    QueueTraits(const QueueTraits&) = default;

    QueueTraits SetCanBeDeferred(bool value) {
      can_be_deferred = value;
      return *this;
    }

    QueueTraits SetCanBeThrottled(bool value) {
      can_be_throttled = value;
      return *this;
    }

    QueueTraits SetCanBePaused(bool value) {
      can_be_paused = value;
      return *this;
    }

    QueueTraits SetCanBeFrozen(bool value) {
      can_be_frozen = value;
      return *this;
    }

    QueueTraits SetCanRunInBackground(bool value) {
      can_run_in_background = value;
      return *this;
    }

    QueueTraits SetCanRunWhenVirtualTimePaused(bool value) {
      can_run_when_virtual_time_paused = value;
      return *this;
    }

    QueueTraits SetPrioritisationType(PrioritisationType type) {
      prioritisation_type = type;
      return *this;
    }

    QueueTraits SetCanBePausedForAndroidWebview(bool value) {
      can_be_paused_for_android_webview = value;
      return *this;
    }

    bool operator==(const QueueTraits& other) const {
      return can_be_deferred == other.can_be_deferred &&
             can_be_throttled == other.can_be_throttled &&
             can_be_paused == other.can_be_paused &&
             can_be_frozen == other.can_be_frozen &&
             can_run_in_background == other.can_run_in_background &&
             can_run_when_virtual_time_paused ==
                 other.can_run_when_virtual_time_paused &&
             prioritisation_type == other.prioritisation_type &&
             can_be_paused_for_android_webview ==
                 other.can_be_paused_for_android_webview;
    }

    // Return a key suitable for WTF::HashMap.
    QueueTraitsKeyType Key() const {
      // offset for shifting bits to compute |key|.
      // |key| starts at 1 since 0 and -1 are used for empty/deleted values.
      int offset = 0;
      int key = 1 << (offset++);
      key |= can_be_deferred << (offset++);
      key |= can_be_throttled << (offset++);
      key |= can_be_paused << (offset++);
      key |= can_be_frozen << (offset++);
      key |= can_run_in_background << (offset++);
      key |= can_run_when_virtual_time_paused << (offset++);
      key |= can_be_paused_for_android_webview << (offset++);
      key |= static_cast<int>(prioritisation_type) << offset;
      offset += kPrioritisationTypeWidthBits;
      return key;
    }

    bool can_be_deferred : 1;
    bool can_be_throttled : 1;
    bool can_be_paused : 1;
    bool can_be_frozen : 1;
    bool can_run_in_background : 1;
    bool can_run_when_virtual_time_paused : 1;
    bool can_be_paused_for_android_webview : 1;
    PrioritisationType prioritisation_type = PrioritisationType::kRegular;
  };

  struct QueueCreationParams {
    explicit QueueCreationParams(QueueType queue_type)
        : queue_type(queue_type),
          spec(NameForQueueType(queue_type)),
          frame_scheduler(nullptr),
          freeze_when_keep_active(false) {}

    QueueCreationParams SetFixedPriority(
        base::Optional<base::sequence_manager::TaskQueue::QueuePriority>
            priority) {
      fixed_priority = priority;
      return *this;
    }

    QueueCreationParams SetFreezeWhenKeepActive(bool value) {
      freeze_when_keep_active = value;
      return *this;
    }

    QueueCreationParams SetWebSchedulingPriority(
        base::Optional<WebSchedulingPriority> priority) {
      web_scheduling_priority = priority;
      return *this;
    }

    // Forwarded calls to |queue_traits|

    QueueCreationParams SetCanBeDeferred(bool value) {
      queue_traits = queue_traits.SetCanBeDeferred(value);
      ApplyQueueTraitsToSpec();
      return *this;
    }

    QueueCreationParams SetCanBeThrottled(bool value) {
      queue_traits = queue_traits.SetCanBeThrottled(value);
      ApplyQueueTraitsToSpec();
      return *this;
    }

    QueueCreationParams SetCanBePaused(bool value) {
      queue_traits = queue_traits.SetCanBePaused(value);
      ApplyQueueTraitsToSpec();
      return *this;
    }

    QueueCreationParams SetCanBeFrozen(bool value) {
      queue_traits = queue_traits.SetCanBeFrozen(value);
      ApplyQueueTraitsToSpec();
      return *this;
    }

    QueueCreationParams SetCanRunInBackground(bool value) {
      queue_traits = queue_traits.SetCanRunInBackground(value);
      ApplyQueueTraitsToSpec();
      return *this;
    }

    QueueCreationParams SetCanRunWhenVirtualTimePaused(bool value) {
      queue_traits = queue_traits.SetCanRunWhenVirtualTimePaused(value);
      ApplyQueueTraitsToSpec();
      return *this;
    }

    QueueCreationParams SetPrioritisationType(
        QueueTraits::PrioritisationType type) {
      queue_traits = queue_traits.SetPrioritisationType(type);
      ApplyQueueTraitsToSpec();
      return *this;
    }

    QueueCreationParams SetQueueTraits(QueueTraits value) {
      queue_traits = value;
      ApplyQueueTraitsToSpec();
      return *this;
    }

    // Forwarded calls to |spec|.

    QueueCreationParams SetFrameScheduler(FrameSchedulerImpl* scheduler) {
      frame_scheduler = scheduler;
      return *this;
    }
    QueueCreationParams SetShouldMonitorQuiescence(bool should_monitor) {
      spec = spec.SetShouldMonitorQuiescence(should_monitor);
      return *this;
    }

    QueueCreationParams SetShouldNotifyObservers(bool run_observers) {
      spec = spec.SetShouldNotifyObservers(run_observers);
      return *this;
    }

    QueueCreationParams SetTimeDomain(
        base::sequence_manager::TimeDomain* domain) {
      spec = spec.SetTimeDomain(domain);
      return *this;
    }

    QueueType queue_type;
    base::sequence_manager::TaskQueue::Spec spec;
    base::Optional<base::sequence_manager::TaskQueue::QueuePriority>
        fixed_priority;
    FrameSchedulerImpl* frame_scheduler;
    QueueTraits queue_traits;
    bool freeze_when_keep_active;
    base::Optional<WebSchedulingPriority> web_scheduling_priority;

   private:
    void ApplyQueueTraitsToSpec() {
      spec = spec.SetDelayedFencesAllowed(queue_traits.can_be_throttled);
    }
  };

  ~MainThreadTaskQueue() override;

  QueueType queue_type() const { return queue_type_; }

  base::Optional<base::sequence_manager::TaskQueue::QueuePriority>
  FixedPriority() const {
    return fixed_priority_;
  }

  bool CanBeDeferred() const { return queue_traits_.can_be_deferred; }

  bool CanBeThrottled() const { return queue_traits_.can_be_throttled; }

  bool CanBePaused() const { return queue_traits_.can_be_paused; }

  // Used for WebView's pauseTimers API. This API expects layout, parsing, and
  // Javascript timers to be paused. Though this suggests we should pause
  // loading (where parsing happens) as well, there are some expectations of JS
  // still being able to run during pause. Because of this we only pause timers
  // as well as any other pausable frame task queue.
  // https://developer.android.com/reference/android/webkit/WebView#pauseTimers()
  bool CanBePausedForAndroidWebview() const {
    return queue_traits_.can_be_paused_for_android_webview;
  }

  bool CanBeFrozen() const { return queue_traits_.can_be_frozen; }

  bool CanRunInBackground() const {
    return queue_traits_.can_run_in_background;
  }

  bool CanRunWhenVirtualTimePaused() const {
    return queue_traits_.can_run_when_virtual_time_paused;
  }

  bool FreezeWhenKeepActive() const { return freeze_when_keep_active_; }

  QueueTraits GetQueueTraits() const { return queue_traits_; }

  QueueTraits::PrioritisationType GetPrioritisationType() const {
    return queue_traits_.prioritisation_type;
  }

  void OnTaskReady(const void* frame_scheduler,
                   const base::sequence_manager::Task& task,
                   base::sequence_manager::LazyNow* lazy_now);

  void OnTaskStarted(
      const base::sequence_manager::Task& task,
      const base::sequence_manager::TaskQueue::TaskTiming& task_timing);

  void OnTaskCompleted(
      const base::sequence_manager::Task& task,
      base::sequence_manager::TaskQueue::TaskTiming* task_timing,
      base::sequence_manager::LazyNow* lazy_now);

  void SetOnIPCTaskPosted(
      base::RepeatingCallback<void(const base::sequence_manager::Task&)>
          on_ipc_task_posted_callback);
  void DetachOnIPCTaskPostedWhileInBackForwardCache();

  void DetachFromMainThreadScheduler();

  // Override base method to notify MainThreadScheduler about shutdown queue.
  void ShutdownTaskQueue() override;

  FrameSchedulerImpl* GetFrameScheduler() const;

  scoped_refptr<base::SingleThreadTaskRunner> CreateTaskRunner(
      TaskType task_type) {
    return TaskQueue::CreateTaskRunner(static_cast<int>(task_type));
  }

  void SetNetRequestPriority(net::RequestPriority net_request_priority);
  base::Optional<net::RequestPriority> net_request_priority() const;

  void SetWebSchedulingPriority(WebSchedulingPriority priority);
  base::Optional<WebSchedulingPriority> web_scheduling_priority() const;

  base::WeakPtr<MainThreadTaskQueue> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 protected:
  void SetFrameSchedulerForTest(FrameSchedulerImpl* frame_scheduler);

  // TODO(kraynov): Consider options to remove TaskQueueImpl reference here.
  MainThreadTaskQueue(
      std::unique_ptr<base::sequence_manager::internal::TaskQueueImpl> impl,
      const Spec& spec,
      const QueueCreationParams& params,
      MainThreadSchedulerImpl* main_thread_scheduler);

 private:
  friend class base::sequence_manager::SequenceManager;
  friend class blink::scheduler::main_thread_scheduler_impl_unittest::
      MainThreadSchedulerImplTest;
  friend class agent_interference_recorder_test::AgentInterferenceRecorderTest;

  // Clear references to main thread scheduler and frame scheduler and dispatch
  // appropriate notifications. This is the common part of ShutdownTaskQueue and
  // DetachFromMainThreadScheduler.
  void ClearReferencesToSchedulers();

  const QueueType queue_type_;
  const base::Optional<base::sequence_manager::TaskQueue::QueuePriority>
      fixed_priority_;
  const QueueTraits queue_traits_;
  const bool freeze_when_keep_active_;

  // Warning: net_request_priority is not the same as the priority of the queue.
  // It is the priority (at the loading stack level) of the resource associated
  // to the queue, if one exists.
  //
  // Used to track UMA metrics for resource loading tasks split by net priority.
  base::Optional<net::RequestPriority> net_request_priority_;

  // |web_scheduling_priority_| is the priority of the task queue within the web
  // scheduling API. This priority is used in conjunction with the frame
  // scheduling policy to determine the task queue priority.
  base::Optional<WebSchedulingPriority> web_scheduling_priority_;

  // Needed to notify renderer scheduler about completed tasks.
  MainThreadSchedulerImpl* main_thread_scheduler_;  // NOT OWNED

  // Set in the constructor. Cleared in ClearReferencesToSchedulers(). Can never
  // be set to a different value afterwards (except in tests).
  FrameSchedulerImpl* frame_scheduler_;  // NOT OWNED

  base::WeakPtrFactory<MainThreadTaskQueue> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MainThreadTaskQueue);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_MAIN_THREAD_TASK_QUEUE_H_
