// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/v8_microtasks_scope.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/scheduler/public/event_loop.h"

namespace blink {

// static
template <>
v8::MicrotasksScope::Type
V8MicrotasksScope<MicrotasksScopeMode::kDoNotRunMicrotasks>::EffectiveMode(
    ExecutionContext*) {
  return MicrotasksScopeMode::kDoNotRunMicrotasks;
}

// static
template <>
v8::MicrotasksScope::Type
V8MicrotasksScope<MicrotasksScopeMode::kRunMicrotasks>::EffectiveMode(
    ExecutionContext* execution_context) {
  return ToEventLoop(execution_context).AreMicrotasksPaused()
             ? MicrotasksScopeMode::kDoNotRunMicrotasks
             : MicrotasksScopeMode::kRunMicrotasks;
}

}  // namespace blink
