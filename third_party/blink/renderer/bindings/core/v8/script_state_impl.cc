// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/script_state_impl.h"

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"

namespace blink {

// static
void ScriptStateImpl::Init() {
  ScriptState::SetCreateCallback(ScriptStateImpl::Create);
}

// static
ScriptState* ScriptStateImpl::Create(v8::Local<v8::Context> context,
                                     DOMWrapperWorld* world,
                                     ExecutionContext* execution_context) {
  return MakeGarbageCollected<ScriptStateImpl>(context, std::move(world),
                                               execution_context);
}

ScriptStateImpl::ScriptStateImpl(v8::Local<v8::Context> context,
                                 DOMWrapperWorld* world,
                                 ExecutionContext* execution_context)
    : ScriptState(context, world, execution_context),
      execution_context_(execution_context) {}

void ScriptStateImpl::Trace(Visitor* visitor) const {
  ScriptState::Trace(visitor);
  visitor->Trace(execution_context_);
}

}  // namespace blink
