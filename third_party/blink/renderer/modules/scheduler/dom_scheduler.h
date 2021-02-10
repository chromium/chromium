// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SCHEDULER_DOM_SCHEDULER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SCHEDULER_DOM_SCHEDULER_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/dom/document.h"
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

class MODULES_EXPORT DOMScheduler : public ScriptWrappable,
                                    public ExecutionContextLifecycleObserver,
                                    public Supplement<LocalDOMWindow> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static const char kSupplementName[];

  static DOMScheduler* scheduler(LocalDOMWindow&);

  explicit DOMScheduler(LocalDOMWindow*);

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
  DOMTaskSignal* currentTaskSignal(ScriptState*) const;

  base::SingleThreadTaskRunner* GetTaskRunnerFor(WebSchedulingPriority);

  void ContextDestroyed() override;

  void Trace(Visitor*) const override;

 private:
  static constexpr size_t kWebSchedulingPriorityCount =
      static_cast<size_t>(WebSchedulingPriority::kLastPriority) + 1;

  void CreateGlobalTaskQueues(LocalDOMWindow*);

  // |global_task_queues_| is initialized with one entry per priority, indexed
  // by priority. This will be empty when the window is detached.
  Vector<std::unique_ptr<WebSchedulingTaskQueue>, kWebSchedulingPriorityCount>
      global_task_queues_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SCHEDULER_DOM_SCHEDULER_H_
