// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/script_state_impl.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_initializer.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/instrumentation/resource_coordinator/renderer_resource_coordinator.h"
#include "third_party/blink/renderer/platform/scheduler/public/event_loop.h"

namespace blink {

ScriptStateImpl::ScriptStateImpl(v8::Local<v8::Context> context,
                                 DOMWrapperWorld* world,
                                 ExecutionContext& execution_context)
    : ScriptState(context, world, execution_context.GetAgent()->event_loop()),
      execution_context_(&execution_context) {
  V8Initializer::InitializeContext(context, execution_context);
  RendererResourceCoordinator::Get()->OnScriptStateCreated(this);
}

void ScriptStateImpl::Trace(Visitor* visitor) const {
  ScriptState::Trace(visitor);
  visitor->Trace(execution_context_);
}

}  // namespace blink
