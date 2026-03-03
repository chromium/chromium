// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_V8_MICROTASKS_SCOPE_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_V8_MICROTASKS_SCOPE_H_

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/scheduler/public/event_loop.h"
#include "v8/include/v8-microtask-queue.h"

namespace blink {

class ExecutionContext;

using MicrotasksScopeMode = v8::MicrotasksScope::Type;

template <MicrotasksScopeMode kMode>
class V8MicrotasksScope {
 public:
  explicit V8MicrotasksScope(ScriptState* script_state)
      : V8MicrotasksScope(ExecutionContext::From(script_state)) {}

  explicit V8MicrotasksScope(ExecutionContext* execution_context)
      : scope_(execution_context->GetIsolate(),
               ToMicrotaskQueue(execution_context),
               EffectiveMode(execution_context)) {}

  V8MicrotasksScope(const V8MicrotasksScope&) = delete;
  V8MicrotasksScope& operator=(const V8MicrotasksScope&) = delete;

 private:
  static v8::MicrotasksScope::Type EffectiveMode(
      ExecutionContext* execution_context) {
    if constexpr (kMode == MicrotasksScopeMode::kDoNotRunMicrotasks) {
      return MicrotasksScopeMode::kDoNotRunMicrotasks;
    }
    return ToEventLoop(execution_context).AreMicrotasksPaused()
               ? MicrotasksScopeMode::kDoNotRunMicrotasks
               : MicrotasksScopeMode::kRunMicrotasks;
  }
  v8::MicrotasksScope scope_;
};

using V8DoNotRunMicrotasksScope =
    V8MicrotasksScope<MicrotasksScopeMode::kDoNotRunMicrotasks>;
using V8RunMicrotasksScope =
    V8MicrotasksScope<MicrotasksScopeMode::kRunMicrotasks>;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_V8_MICROTASKS_SCOPE_H_
