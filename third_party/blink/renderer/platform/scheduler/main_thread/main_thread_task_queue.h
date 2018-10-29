// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_MAIN_THREAD_TASK_QUEUE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_MAIN_THREAD_TASK_QUEUE_H_

#include "base/task/sequence_manager/task_queue.h"
#include "base/task/sequence_manager/task_queue_impl.h"
#include "net/base/request_priority.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"

namespace base {
namespace sequence_manager {
class SequenceManager;
}
}  // namespace base

namespace blink {
namespace scheduler {

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

    kUnthrottled = 4,
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
    kIPC = 17,
    kInput = 18,

    // Detached is used in histograms for tasks which are run after frame
    // is detached and task queue is gracefully shutdown.
    // TODO(altimin): Move to the top when histogram is renumbered.
    kDetached = 19,

    kCleanup = 20,

    kWebSchedulingUserInteraction = 21,
    kWebSchedulingBestEffort = 22,

    // Used to group multiple types when calculating Expected Queueing Time.
    kOther = 23,
    kCount = 24
  };

  // Returns name of the given queue type. Returned string has application
  // lifetime.
  static const char* NameForQueueType(QueueType queue_type);

  // High-level category used by MainThreadScheduler to make scheduling
  // decisions.
  enum class QueueClass {
    kNone = 0,
    kLoading = 1,
    kTimer = 2,
    kCompositor = 4,

    kCount = 5,
  };

  static QueueClass QueueClassForQueueType(QueueType type);

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
          can_run_in_background(true) {}

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

    bool operator==(const QueueTraits& other) const {
      return can_be_deferred == other.can_be_deferred &&
             can_be_throttled == other.can_be_throttled &&
             can_be_paused == other.can_be_paused &&
             can_be_frozen == other.can_be_frozen &&
             can_run_in_background == other.can_run_in_background;
    }

    // Return a key suitable for WTF::HashMap.
    QueueTraitsKeyType Key() const {
      // Start at 1; 0 and -1 are used for empty/deleted values.
      int key = 1 << 0;
      key |= can_be_deferred << 1;
      key |= can_be_throttled << 2;
      key |= can_be_paused << 3;
      key |= can_be_frozen << 4;
      key |= can_run_in_background << 5;
      return key;
    }

    bool can_be_deferred : 1;
    bool can_be_throttled : 1;
    bool can_be_paused : 1;
    bool can_be_frozen : 1;
    bool can_run_in_background : 1;
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

    // Forwarded calls to |queue_traits|

    QueueCreationParams SetCanBeDeferred(bool value) {
      queue_traits = queue_traits.SetCanBeDeferred(value);
      return *this;
    }

    QueueCreationParams SetCanBeThrottled(bool value) {
      queue_traits = queue_traits.SetCanBeThrottled(value);
      return *this;
    }

    QueueCreationParams SetCanBePaused(bool value) {
      queue_traits = queue_traits.SetCanBePaused(value);
      return *this;
    }

    QueueCreationParams SetCanBeFrozen(bool value) {
      queue_traits = queue_traits.SetCanBeFrozen(value);
      return *this;
    }

    QueueCreationParams SetCanRunInBackground(bool value) {
      queue_traits = queue_traits.SetCanRunInBackground(value);
      return *this;
    }

    QueueCreationParams SetQueueTraits(QueueTraits value) {
      queue_traits = value;
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
  };

  ~MainThreadTaskQueue() override;

  QueueType queue_type() const { return queue_type_; }

  QueueClass queue_class() const { return queue_class_; }

  base::Optional<base::sequence_manager::TaskQueue::QueuePriority>
  FixedPriority() const {
    return fixed_priority_;
  }

  bool CanBeDeferred() const { return queue_traits_.can_be_deferred; }

  bool CanBeThrottled() const { return queue_traits_.can_be_throttled; }

  bool CanBePaused() const { return queue_traits_.can_be_paused; }

  bool CanBeFrozen() const { return queue_traits_.can_be_frozen; }

  bool CanRunInBackground() const {
    return queue_traits_.can_run_in_background;
  }

  bool FreezeWhenKeepActive() const { return freeze_when_keep_active_; }

  QueueTraits GetQueueTraits() const { return queue_traits_; }

  void OnTaskStarted(
      const base::sequence_manager::Task& task,
      const base::sequence_manager::TaskQueue::TaskTiming& task_timing);

  void OnTaskCompleted(
      const base::sequence_manager::Task& task,
      const base::sequence_manager::TaskQueue::TaskTiming& task_timing);

  void DetachFromMainThreadScheduler();

  // Override base method to notify MainThreadScheduler about shutdown queue.
  void ShutdownTaskQueue() override;

  FrameSchedulerImpl* GetFrameScheduler() const;
  void DetachFromFrameScheduler();

  scoped_refptr<base::SingleThreadTaskRunner> CreateTaskRunner(
      TaskType task_type) {
    return TaskQueue::CreateTaskRunner(static_cast<int>(task_type));
  }

  void SetNetRequestPriority(net::RequestPriority net_request_priority);
  base::Optional<net::RequestPriority> net_request_priority() const;

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

  // Clear references to main thread scheduler and frame scheduler and dispatch
  // appropriate notifications. This is the common part of ShutdownTaskQueue and
  // DetachFromMainThreadScheduler.
  void ClearReferencesToSchedulers();

  const QueueType queue_type_;
  const QueueClass queue_class_;
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

  // Needed to notify renderer scheduler about completed tasks.
  MainThreadSchedulerImpl* main_thread_scheduler_;  // NOT OWNED

  FrameSchedulerImpl* frame_scheduler_;  // NOT OWNED

  DISALLOW_COPY_AND_ASSIGN(MainThreadTaskQueue);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_MAIN_THREAD_TASK_QUEUE_H_
