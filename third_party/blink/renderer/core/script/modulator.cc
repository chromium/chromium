// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script/modulator.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/script/document_modulator_impl.h"
#include "third_party/blink/renderer/core/script/worker_modulator_impl.h"
#include "third_party/blink/renderer/core/script/worklet_modulator_impl.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worklet_global_scope.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_context_data.h"

namespace blink {

namespace {
const char kPerContextDataKey[] = "Modulator";
}  // namespace

Modulator* Modulator::From(ScriptState* script_state) {
  if (!script_state)
    return nullptr;

  V8PerContextData* per_context_data = script_state->PerContextData();
  if (!per_context_data)
    return nullptr;

  Modulator* modulator =
      static_cast<Modulator*>(per_context_data->GetData(kPerContextDataKey));
  if (modulator)
    return modulator;
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  if (auto* document = DynamicTo<Document>(execution_context)) {
    modulator = MakeGarbageCollected<DocumentModulatorImpl>(script_state);
    Modulator::SetModulator(script_state, modulator);

    // See comment in LocalDOMWindow::modulator_ for this workaround.
    LocalDOMWindow* window = document->ExecutingWindow();
    window->SetModulator(modulator);
  } else if (auto* worklet_scope =
                 DynamicTo<WorkletGlobalScope>(execution_context)) {
    modulator = MakeGarbageCollected<WorkletModulatorImpl>(script_state);
    Modulator::SetModulator(script_state, modulator);

    // See comment in WorkerOrWorkletGlobalScope::modulator_ for this
    // workaround.
    worklet_scope->SetModulator(modulator);
  } else if (auto* worker_scope =
                 DynamicTo<WorkerGlobalScope>(execution_context)) {
    modulator = MakeGarbageCollected<WorkerModulatorImpl>(script_state);
    Modulator::SetModulator(script_state, modulator);

    // See comment in WorkerOrWorkletGlobalScope::modulator_ for this
    // workaround.
    worker_scope->SetModulator(modulator);
  } else {
    NOTREACHED();
  }
  return modulator;
}

Modulator::~Modulator() {}

void Modulator::SetModulator(ScriptState* script_state, Modulator* modulator) {
  DCHECK(script_state);
  V8PerContextData* per_context_data = script_state->PerContextData();
  DCHECK(per_context_data);
  per_context_data->AddData(kPerContextDataKey, modulator);
}

void Modulator::ClearModulator(ScriptState* script_state) {
  DCHECK(script_state);
  V8PerContextData* per_context_data = script_state->PerContextData();
  DCHECK(per_context_data);
  per_context_data->ClearData(kPerContextDataKey);
}

}  // namespace blink
