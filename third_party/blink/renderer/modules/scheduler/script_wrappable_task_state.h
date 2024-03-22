// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SCHEDULER_SCRIPT_WRAPPABLE_TASK_STATE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SCHEDULER_SCRIPT_WRAPPABLE_TASK_STATE_H_

#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace v8 {
class Isolate;
}  // namespace v8

namespace blink::scheduler {
class TaskAttributionInfo;
}  // namespace blink::scheduler

namespace blink {
class AbortSignal;
class DOMTaskSignal;
class ScriptState;

// `ScriptWrappableTaskState` objects are stored in V8 as continuation preserved
// embedder data (CPED). They aren't exposed directly to JS, but are
// `ScriptWrappable` so they can be stored in CPED.
//
// V8 propagates these objects to continuations by binding the current CPED and
// restoring it in microtasks:
//   1. For promises, the current CPED is bound to the promise reaction at
//      creation time (e.g. when .then() is called or a promise is awaited)
//
//   2. For promises resolved with a custom thennable, there's an extra hop
//      through a microtask to run the custom .then() function. For the promise
//      being resolved, (1) above applies. For the custom .then() function, the
//      resolve-time CPED is bound to the microtask, i.e. the CPED inside the
//      custom .then() function is the same as when the resolve happens, keeping
//      it consistent across the async hop.
//
//   3. For non-promise microtasks, which are used throughout Blink, the current
//      CPED is bound when EnqueueMicrotask() is called.
//
// Similarly, in Blink these objects are propagated to descendant tasks by
// capturing the current CPED during various API calls and restoring it prior to
// running a callback. For example, the current CPED is captured when setTimeout
// is called and restored before running the associated callback.
//
// Instances of this class will either be WebSchedulingTaskState, if propagating
// abort and priority sources (web scheduling APIs), or TaskAttributionInfoImpl,
// which is exposed as TaskAttributionInfo via TaskAttributionTracker public
// APIs.
class MODULES_EXPORT ScriptWrappableTaskState : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // Get the `ScriptWrappableTaskState` currently stored as continuation
  // preserved embedder data.
  static ScriptWrappableTaskState* GetCurrent(v8::Isolate*);

  // Set the given `ScriptWrappableTaskState` as the current continuation
  // preserved embedder data.
  static void SetCurrent(ScriptState*, ScriptWrappableTaskState*);

  virtual scheduler::TaskAttributionInfo* GetTaskAttributionInfo() = 0;

  virtual AbortSignal* AbortSource() = 0;
  virtual DOMTaskSignal* PrioritySource() = 0;

  void Trace(Visitor*) const override;

 protected:
  ScriptWrappableTaskState();
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SCHEDULER_SCRIPT_WRAPPABLE_TASK_STATE_H_
