// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SCHEDULER_DOM_SCHEDULER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SCHEDULER_DOM_SCHEDULER_H_

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_priority.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class DOMTask;
class DOMTaskQueue;
class ExecutionContext;
class SchedulerPostTaskOptions;
class ScriptValue;
class V8Function;

class MODULES_EXPORT DOMScheduler : public ScriptWrappable,
                                    public ContextLifecycleObserver,
                                    public Supplement<Document> {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(DOMScheduler);

 public:
  static const char kSupplementName[];

  static DOMScheduler* From(Document&);

  explicit DOMScheduler(Document*);

  // Returns the DOMTaskQueue of the currently executing DOMTask. If the current
  // task was not scheduled through the scheduler, this returns the default
  // priority global task queue.
  DOMTaskQueue* currentTaskQueue();

  // Returns the DOMTaskQueue for the given |priority| or nullptr if the
  // underlying context is destroyed, e.g. for detached documents.
  DOMTaskQueue* getTaskQueue(AtomicString priority);

  // postTask creates and queues a DOMTask in the |global_task_queues_| entry
  // corresponding to the priority in the SchedulerPostTaskOptions, and returns
  // the DOMTask. If the underlying context is destroyed, e.g. for detached
  // documents, this returns nullptr.
  DOMTask* postTask(V8Function*,
                    SchedulerPostTaskOptions*,
                    const HeapVector<ScriptValue>& args);

  // Callbacks invoked by DOMTasks when they run.
  void OnTaskStarted(DOMTaskQueue*, DOMTask*);
  void OnTaskCompleted(DOMTaskQueue*, DOMTask*);

  void ContextDestroyed(ExecutionContext*) override;

  void Trace(Visitor*) override;

 private:
  static constexpr size_t kWebSchedulingPriorityCount =
      static_cast<size_t>(WebSchedulingPriority::kLastPriority) + 1;

  void CreateGlobalTaskQueues(Document*);
  DOMTaskQueue* GetTaskQueue(WebSchedulingPriority);

  // |global_task_queues_| is initialized with one entry per priority, indexed
  // by priority. This will be empty when the document is detached.
  HeapVector<Member<DOMTaskQueue>, kWebSchedulingPriorityCount>
      global_task_queues_;
  // The DOMTaskQueue associated with the currently running DOMTask, or nullptr
  // if the current task was not scheduled through this DOMScheduler.
  Member<DOMTaskQueue> current_task_queue_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SCHEDULER_DOM_SCHEDULER_H_
