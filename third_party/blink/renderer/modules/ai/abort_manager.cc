// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/abort_manager.h"

#include <utility>

#include "base/check.h"
#include "third_party/blink/renderer/core/dom/abort_controller.h"
#include "third_party/blink/renderer/modules/ai/exception_helpers.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

AbortManager::AbortManager(ScriptState* script_state,
                           AbortSignal* create_abort_signal,
                           base::OnceClosure on_create_abort)
    : destruction_abort_controller_(AbortController::Create(script_state)),
      create_abort_signal_(create_abort_signal) {
  if (create_abort_signal_) {
    CHECK(!create_abort_signal_->aborted());
    create_abort_handle_ =
        create_abort_signal_->AddAlgorithm(std::move(on_create_abort));
  }
}

AbortManager::~AbortManager() = default;

void AbortManager::Trace(Visitor* visitor) const {
  visitor->Trace(destruction_abort_controller_);
  visitor->Trace(create_abort_signal_);
  visitor->Trace(create_abort_handle_);
}

AbortSignal* AbortManager::CreateSignal(ScriptState* script_state,
                                        AbortSignal* operation_signal,
                                        ExceptionState& exception_state) {
  HeapVector<Member<AbortSignal>> signals;
  signals.push_back(destruction_abort_controller_->signal());
  if (operation_signal) {
    signals.push_back(operation_signal);
  }
  auto* signal = MakeGarbageCollected<AbortSignal>(script_state, signals);
  return HandleAbortSignal(signal, script_state, exception_state) ? nullptr
                                                                  : signal;
}

bool AbortManager::OnDestroy(ScriptState* script_state,
                             ExceptionState& exception_state) {
  if (!script_state || !script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return false;
  }
  ScriptState::Scope scope(script_state);
  if (create_abort_handle_ && create_abort_signal_->aborted()) {
    destruction_abort_controller_->abort(
        script_state, create_abort_signal_->reason(script_state));
  } else {
    destruction_abort_controller_->abort(script_state);
  }
  if (create_abort_handle_) {
    create_abort_signal_->RemoveAlgorithm(create_abort_handle_);
    create_abort_handle_ = nullptr;
  }
  return true;
}

}  // namespace blink
