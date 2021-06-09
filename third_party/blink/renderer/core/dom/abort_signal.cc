// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/abort_signal.h"

#include <utility>

#include "base/callback.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/event_target_names.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

AbortSignal::AbortSignal(ExecutionContext* execution_context)
    : execution_context_(execution_context) {}
AbortSignal::~AbortSignal() = default;

// static
AbortSignal* AbortSignal::abort(ScriptState* script_state) {
  AbortSignal* signal =
      MakeGarbageCollected<AbortSignal>(ExecutionContext::From(script_state));
  signal->aborted_flag_ = true;
  return signal;
}

const AtomicString& AbortSignal::InterfaceName() const {
  return event_target_names::kAbortSignal;
}

ExecutionContext* AbortSignal::GetExecutionContext() const {
  return execution_context_.Get();
}

void AbortSignal::AddAlgorithm(base::OnceClosure algorithm) {
  if (aborted_flag_)
    return;
  abort_algorithms_.push_back(std::move(algorithm));
}

void AbortSignal::AddSignalAbortAlgorithm(AbortSignal* dependent_signal) {
  if (aborted_flag_)
    return;

  // The signal should be kept alive as long as parentSignal is allow chained
  // requests like the following:
  // controller -owns-> signal1 -owns-> signal2 -owns-> signal3 <-owns- request
  //
  // Due to lack to traced closures we pass a weak persistent but also add
  // |dependent_signal| as a dependency that is traced. We do not use
  // WrapPersistent here as this would create a root for Oilpan and unified heap
  // that leaks the |execution_context_| as there is no explicit event removing
  // the root anymore.
  abort_algorithms_.emplace_back(WTF::Bind(
      &AbortSignal::SignalAbort, WrapWeakPersistent(dependent_signal)));
  dependent_signals_.push_back(dependent_signal);
}

void AbortSignal::SignalAbort() {
  if (aborted_flag_)
    return;
  aborted_flag_ = true;
  for (base::OnceClosure& closure : abort_algorithms_) {
    std::move(closure).Run();
  }
  abort_algorithms_.clear();
  dependent_signals_.clear();
  DispatchEvent(*Event::Create(event_type_names::kAbort));
}

void AbortSignal::Follow(AbortSignal* parentSignal) {
  if (aborted_flag_)
    return;
  if (parentSignal->aborted_flag_)
    SignalAbort();

  parentSignal->AddSignalAbortAlgorithm(this);
}

void AbortSignal::Trace(Visitor* visitor) const {
  visitor->Trace(execution_context_);
  visitor->Trace(dependent_signals_);
  EventTargetWithInlineData::Trace(visitor);
}

}  // namespace blink
