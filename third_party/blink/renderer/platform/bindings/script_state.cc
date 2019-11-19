// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/script_state.h"

#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_context_data.h"
#include "third_party/blink/renderer/platform/instrumentation/instance_counters.h"

namespace blink {

ScriptState::ScriptState(v8::Local<v8::Context> context,
                         scoped_refptr<DOMWrapperWorld> world)
    : isolate_(context->GetIsolate()),
      context_(isolate_, context),
      world_(std::move(world)),
      per_context_data_(std::make_unique<V8PerContextData>(context)),
      reference_from_v8_context_(PERSISTENT_FROM_HERE, this) {
  DCHECK(world_);
  context_.SetWeak(this, &OnV8ContextCollectedCallback);
  context->SetAlignedPointerInEmbedderData(kV8ContextPerContextDataIndex, this);
}

ScriptState::~ScriptState() {
  DCHECK(!per_context_data_);
  DCHECK(context_.IsEmpty());
  InstanceCounters::DecrementCounter(
      InstanceCounters::kDetachedScriptStateCounter);
}

void ScriptState::DetachGlobalObject() {
  DCHECK(!context_.IsEmpty());
  GetContext()->DetachGlobal();
}

void ScriptState::DisposePerContextData() {
  per_context_data_ = nullptr;
  InstanceCounters::IncrementCounter(
      InstanceCounters::kDetachedScriptStateCounter);
}

void ScriptState::DissociateContext() {
  DCHECK(!per_context_data_);

  // On a worker thread we tear down V8's isolate without running a GC.
  // Alternately we manually clear all references between V8 and Blink, and run
  // operations that should have been invoked by weak callbacks if a GC were
  // run.

  v8::HandleScope scope(GetIsolate());
  // Cut the reference from V8 context to ScriptState.
  GetContext()->SetAlignedPointerInEmbedderData(kV8ContextPerContextDataIndex,
                                                nullptr);
  reference_from_v8_context_.Clear();

  // Cut the reference from ScriptState to V8 context.
  context_.Clear();
}

void ScriptState::OnV8ContextCollectedCallback(
    const v8::WeakCallbackInfo<ScriptState>& data) {
  data.GetParameter()->reference_from_v8_context_.Clear();
  data.GetParameter()->context_.Clear();
}

}  // namespace blink
