// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SCHEDULER_SCRIPT_WRAPPABLE_TASK_STATE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SCHEDULER_SCRIPT_WRAPPABLE_TASK_STATE_H_

#include "third_party/blink/public/common/scheduler/task_attribution_id.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {
class DOMTaskSignal;

// The scheduler uses `ScriptWrappableTaskState` objects to store continuation
// preserved embedder data, which is data stored on V8 promise reactions at
// creation time and restored at run time.
class MODULES_EXPORT ScriptWrappableTaskState final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // Get the `ScriptWrappableTaskState` currently stored as continuation
  // preserved embedder data.
  static ScriptWrappableTaskState* GetCurrent(ScriptState*);

  // Set the given `ScriptWrappableTaskState` as the current continuation
  // preserved embedder data.
  static void SetCurrent(ScriptState*, ScriptWrappableTaskState*);

  ScriptWrappableTaskState(scheduler::TaskAttributionId id,
                           DOMTaskSignal* signal);

  scheduler::TaskAttributionId GetTaskAttributionId() const {
    return task_attribution_id_;
  }

  DOMTaskSignal* GetSignal() { return signal_; }

  void Trace(Visitor*) const override;

 private:
  const scheduler::TaskAttributionId task_attribution_id_;
  const Member<DOMTaskSignal> signal_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SCHEDULER_SCRIPT_WRAPPABLE_TASK_STATE_H_
