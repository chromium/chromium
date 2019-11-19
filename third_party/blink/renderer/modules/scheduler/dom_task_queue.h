// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SCHEDULER_DOM_TASK_QUEUE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SCHEDULER_DOM_TASK_QUEUE_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_priority.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace blink {

class DOMScheduler;
class DOMTask;
class Document;
class ExecutionContext;
class ScriptValue;
class TaskQueuePostTaskOptions;
class V8Function;
class WebSchedulingTaskQueue;

namespace scheduler {
class WebSchedulingTaskQueue;
}  // namespace scheduler

class MODULES_EXPORT DOMTaskQueue : public ScriptWrappable,
                                    ContextLifecycleObserver {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(DOMTaskQueue);

 public:
  DOMTaskQueue(Document*, WebSchedulingPriority, DOMScheduler*);

  // Returns the priority of the DOMTaskQueue.
  AtomicString priority() const;

  // postTask creates and queues a DOMTask in this DOMTaskQueue and returns the
  // DOMTask. If the underlying context is destroyed, e.g. for detached
  // documents, this returns nullptr.
  DOMTask* postTask(V8Function*,
                    TaskQueuePostTaskOptions*,
                    const HeapVector<ScriptValue>& args);

  // Move the task from its current DOMTaskQueue to this one. For pending
  // non-delayed tasks, the task is enqueued at the end of this DOMTaskQueue.
  // For delayed tasks, the delay is adjusted before reposting it.
  void take(DOMTask*);

  const scoped_refptr<base::SingleThreadTaskRunner>& GetTaskRunner() {
    return task_runner_;
  }

  DOMScheduler* GetScheduler() { return scheduler_.Get(); }

  void ContextDestroyed(ExecutionContext*) override;

  void Trace(Visitor*) override;

 private:
  void RunTaskCallback(DOMTask*);
  void ScheduleTask(DOMTask*, base::TimeDelta delay);

  const WebSchedulingPriority priority_;
  // This is destroyed when the Context is destroyed, and we rely on this
  // happening before the underlying FrameScheduler is destroyed.
  std::unique_ptr<WebSchedulingTaskQueue> web_scheduling_task_queue_;
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  Member<DOMScheduler> scheduler_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SCHEDULER_DOM_TASK_QUEUE_H_
