// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/script_state.h"

#include "base/check_deref.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_context_data.h"
#include "third_party/blink/renderer/platform/instrumentation/instance_counters.h"
#include "third_party/blink/renderer/platform/instrumentation/resource_coordinator/renderer_resource_coordinator.h"
#include "third_party/blink/renderer/platform/scheduler/public/event_loop.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

ScriptState::ScriptState(v8::Local<v8::Context> context,
                         DOMWrapperWorld* world,
                         scheduler::EventLoop* event_loop)
    : isolate_(world->GetIsolate()),
      context_(isolate_, context),
      world_(world),
      per_context_data_(
          MakeGarbageCollected<V8PerContextData>(context, event_loop)) {
  CHECK(isolate_);
  DCHECK(world_);
  context_.SetWeak(this, &OnV8ContextCollectedCallback);
  context->SetAlignedPointerInEmbedderData(kV8ContextPerContextDataIndex, this,
                                           kTypeTag);
}

void ScriptState::EnqueueMicrotask(
    base::OnceCallback<void(ScriptState*)> callback) {
  if (!ContextIsValid()) {
    return;
  }
  CHECK(per_context_data_);
  // Certain type of contexts would not have an event loop, but caller
  // should know better and not schedule a microtask on such contexts.
  scheduler::EventLoop& event_loop =
      CHECK_DEREF(per_context_data_->GetEventLoop());
  event_loop.EnqueueMicrotask(blink::BindOnce(
      [](ScriptState* script_state,
         base::OnceCallback<void(ScriptState*)> callback) {
        if (!script_state->ContextIsValid()) {
          return;
        }
        ScriptState::Scope scope(script_state);
        std::move(callback).Run(script_state);
      },
      WrapPersistent(this), std::move(callback)));
}

ScriptState::~ScriptState() {
  DCHECK(!per_context_data_);
  DCHECK(context_.IsEmpty());
  InstanceCounters::DecrementCounter(
      InstanceCounters::kDetachedScriptStateCounter);
  RendererResourceCoordinator::Get()->OnScriptStateDestroyed(this);
}

void ScriptState::Trace(Visitor* visitor) const {
  visitor->Trace(per_context_data_);
  visitor->Trace(world_);
}

void ScriptState::DetachGlobalObject() {
  DCHECK(!context_.IsEmpty());
  GetContext()->DetachGlobal();
}

void ScriptState::DisposePerContextData() {
  v8::HandleScope scope(GetIsolate());
  per_context_data_->Dispose();
  per_context_data_ = nullptr;
  InstanceCounters::IncrementCounter(
      InstanceCounters::kDetachedScriptStateCounter);
  RendererResourceCoordinator::Get()->OnScriptStateDetached(this);
}

void ScriptState::DissociateContext() {
  DCHECK(!per_context_data_);

  // On a worker thread we tear down V8's isolate without running a GC.
  // Alternately we manually clear all references between V8 and Blink, and run
  // operations that should have been invoked by weak callbacks if a GC were
  // run.

  v8::HandleScope scope(GetIsolate());
  // Cut the reference from V8 context to ScriptState.
  GetContext()->SetAlignedPointerInEmbedderData(
      kV8ContextPerContextDataIndex, static_cast<ScriptState*>(nullptr),
      kTypeTag);
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
