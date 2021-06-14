// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SCHEDULER_DOM_SCHEDULER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SCHEDULER_DOM_SCHEDULER_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_priority.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class DOMTask;
class DOMTaskSignal;
class ExceptionState;
class SchedulerPostTaskOptions;
class V8SchedulerPostTaskCallback;
class WebSchedulingTaskQueue;

/*
 * DOMScheduler maintains a set of DOMTaskQueues (wrappers around
 * WebSchedulingTaskQueues) which are used to schedule tasks.
 *
 * There are two types of task queues that the scheduler maintains:
 *  1. Fixed-priority, shared task queues. The priority of these task queues
 *  never changes, which allows them to be shared by any tasks that don't
 *  require variable priority. These are postTask tasks created where one of the
 *  following are passed to postTask:
 *    a) A fixed priority
 *    b) Null priority and a signal that is null or not a TaskSignal instance
 *    c) A TaskSignal that was created in a previous postTask call where (a) or
 *       (b) held, and returned by |currentTaskSignal|
 *    d) The "default task signal", returned by currentTaskSignal when there
 *       is no current task signal
 *  These task queues are created when the DOMScheduler is created and destroyed
 *  when the underlying context is destroyed.
 *
 *  2. Variable-priority, per-signal task queues. The priority of these task
 *  queues can change, and is controlled by the associated TaskController. These
 *  task queues are created the first time a non-scheduler-created TaskSignal is
 *  passed to postTask, and their lifetime matches that of the associated
 *  TaskSignal.
 */
class MODULES_EXPORT DOMScheduler : public ScriptWrappable,
                                    public ExecutionContextLifecycleObserver,
                                    public Supplement<ExecutionContext> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static const char kSupplementName[];

  static DOMScheduler* scheduler(ExecutionContext&);

  explicit DOMScheduler(ExecutionContext*);

  // postTask creates and queues a DOMTask and returns a Promise that will
  // resolve when it completes. The task will be scheduled in the queue
  // corresponding to the priority in the SchedulerPostTaskOptions, or in a
  // queue associated with the given DOMTaskSignal if one is provided. If the
  // underlying context is destroyed, e.g. for detached windows, this will
  // return a rejected promise.
  ScriptPromise postTask(ScriptState*,
                         V8SchedulerPostTaskCallback*,
                         SchedulerPostTaskOptions*,
                         ExceptionState&);

  // Returns a TaskSignal representing the state when the current task was
  // scheduled. If postTask is given a signal but no priority, it will return
  // that signal. If postTask is given both a signal and a priority, it will
  // return a signal with the given priority that follows the given signal.
  // If a priority only was given, it will return a signal with the given
  // priority that neither follows another signal nor is known to a controller,
  // and is therefore unmodifiable. If called outside of a postTask task, it
  // will return a task signal at the default priority (user-visible).
  // NOTE: This uses V8's ContinuationPreservedEmbedderData to propagate the
  // currentTaskSignal across microtask boundaries, so it will remain usable
  // even in then() blocks or after an await in an async function.
  DOMTaskSignal* currentTaskSignal(ScriptState*);

  void ContextDestroyed() override;

  void Trace(Visitor*) const override;

 private:
  static constexpr size_t kWebSchedulingPriorityCount =
      static_cast<size_t>(WebSchedulingPriority::kLastPriority) + 1;

  static constexpr WebSchedulingPriority kDefaultPriority =
      WebSchedulingPriority::kUserVisiblePriority;

  // DOMTaskQueue is a thin wrapper around WebSchedulingTaskQueue to make it
  // pseudo garbage collected. We use this indirection because multiple
  // TaskSignals with independent lifetimes may map to the same
  // WebSchedulingTaskQueue, so the WebSchedulingTaskQueue doesn't have a
  // single, clear owner.
  class DOMTaskQueue final : public GarbageCollected<DOMTaskQueue> {
   public:
    explicit DOMTaskQueue(std::unique_ptr<WebSchedulingTaskQueue> task_queue);
    ~DOMTaskQueue();

    void Trace(Visitor* visitor) const {}

    WebSchedulingTaskQueue* GetWebSchedulingTaskQueue() {
      return web_scheduling_task_queue_.get();
    }

   private:
    std::unique_ptr<WebSchedulingTaskQueue> web_scheduling_task_queue_;
  };

  void CreateFixedPriorityTaskQueues(ExecutionContext*);

  // Create a new fixed-priority DOMTaskSignal for the given priority and set up
  // the mapping in |signal_to_task_queue_map_|.
  DOMTaskSignal* CreateTaskSignalFor(WebSchedulingPriority);

  // Create and initialize a new WebSchedulingTaskQueue for the given
  // DOMTaskSignal. This creates the signal and |signal_to_task_queue_map_|
  // mapping, and registers the callback for handling priority changes.
  void CreateTaskQueueFor(DOMTaskSignal*);

  // Callback for when the DOMTaskSignal signals priority change.
  void OnPriorityChange(DOMTaskSignal*);

  // |fixed_priority_task_queues_| is initialized with one entry per priority,
  // indexed by priority. This will be empty when the window is detached.
  HeapVector<Member<DOMTaskQueue>, kWebSchedulingPriorityCount>
      fixed_priority_task_queues_;

  // |signal_to_task_queue_map_| tracks the associated task queue for task
  // signals the scheduler knows about that are still alive. If the task signal
  // is "implicit", meaning created by the scheduler without a controller, then
  // the signal will point back to one of the |fixed_priority_task_queues_|.
  // Otherwise, it will point to a DOMTaskQueue that is unique for that signal.
  // Mappings are removed automatically when the corresponding signal is garbage
  // collected. This will be empty when the window is detached.
  HeapHashMap<WeakMember<DOMTaskSignal>, Member<DOMTaskQueue>>
      signal_to_task_queue_map_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SCHEDULER_DOM_SCHEDULER_H_
