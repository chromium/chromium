// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SCHEDULER_DOM_SCHEDULER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SCHEDULER_DOM_SCHEDULER_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/public/common/scheduler/task_attribution_id.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_priority.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace blink {

class DOMTask;
class DOMTaskSignal;
class ExceptionState;
class SchedulerPostTaskOptions;
class DOMSchedulerTest;
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
 *    b) Undefined priority and an AbortSignal or undefined signal
 *  These task queues are created when the DOMScheduler is created and destroyed
 *  when the underlying context is destroyed.
 *
 *  2. Variable-priority, per-signal task queues. The priority of these task
 *  queues can change, and is controlled by the associated TaskController. These
 *  task queues are created the first time a TaskSignal is passed to postTask,
 *  and their lifetime matches that of the associated TaskSignal.
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

  scheduler::TaskAttributionIdType taskId(ScriptState*);
  AtomicString isAncestor(ScriptState*,
                          scheduler::TaskAttributionIdType parent_id);

  void ContextDestroyed() override;

  void Trace(Visitor*) const override;

 private:
  friend class DOMTask;  // For DOMTaskQueue
  friend class DOMSchedulerTest;

  static constexpr size_t kWebSchedulingPriorityCount =
      static_cast<size_t>(WebSchedulingPriority::kLastPriority) + 1;

  static constexpr WebSchedulingPriority kDefaultPriority =
      WebSchedulingPriority::kUserVisiblePriority;

  // DOMTaskQueue is a thin wrapper around WebSchedulingTaskQueue to make it
  // pseudo garbage collected. This allows us to store WebSchedulingTaskQueues
  // in on-heap collections.
  class DOMTaskQueue final : public GarbageCollected<DOMTaskQueue> {
   public:
    explicit DOMTaskQueue(std::unique_ptr<WebSchedulingTaskQueue> task_queue,
                          WebSchedulingPriority priority);
    ~DOMTaskQueue();

    void Trace(Visitor* visitor) const {}

    base::SingleThreadTaskRunner& GetTaskRunner() { return *task_runner_; }

    WebSchedulingPriority GetPriority() const { return priority_; }

    void SetPriority(WebSchedulingPriority);

   private:
    std::unique_ptr<WebSchedulingTaskQueue> web_scheduling_task_queue_;
    scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
    WebSchedulingPriority priority_;
  };

  void CreateFixedPriorityTaskQueues(ExecutionContext*);

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
  // signals the scheduler knows about that are still alive, with each signal
  // mapping to the corresponding dynamic priority DOMTaskQueue. Mappings are
  // removed automatically when the corresponding signal is garbage collected.
  // This will be empty when the window is detached.
  HeapHashMap<WeakMember<DOMTaskSignal>, Member<DOMTaskQueue>>
      signal_to_task_queue_map_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SCHEDULER_DOM_SCHEDULER_H_
