// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script/modulator.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/script/document_modulator_impl.h"
#include "third_party/blink/renderer/core/script/import_map.h"
#include "third_party/blink/renderer/core/script/worker_modulator_impl.h"
#include "third_party/blink/renderer/core/script/worklet_modulator_impl.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worklet_global_scope.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_context_data.h"
#include "third_party/blink/renderer/platform/loader/fetch/script_fetch_options.h"

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
  if (IsA<LocalDOMWindow>(execution_context)) {
    modulator = MakeGarbageCollected<DocumentModulatorImpl>(script_state);
    Modulator::SetModulator(script_state, modulator);
  } else if (IsA<WorkletGlobalScope>(execution_context)) {
    modulator = MakeGarbageCollected<WorkletModulatorImpl>(script_state);
    Modulator::SetModulator(script_state, modulator);
  } else if (IsA<WorkerGlobalScope>(execution_context)) {
    modulator = MakeGarbageCollected<WorkerModulatorImpl>(script_state);
    Modulator::SetModulator(script_state, modulator);
  } else {
    NOTREACHED_IN_MIGRATION();
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

void Modulator::Trace(Visitor* visitor) const {
  visitor->Trace(import_map_);
}

}  // namespace blink
