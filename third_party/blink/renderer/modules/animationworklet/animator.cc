// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/animationworklet/animator.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_script_runner.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/animationworklet/animator_definition.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/to_v8.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"

namespace blink {

Animator::Animator(v8::Isolate* isolate,
                   AnimatorDefinition* definition,
                   v8::Local<v8::Value> instance,
                   int num_effects)
    : definition_(definition),
      instance_(isolate, instance),
      group_effect_(new WorkletGroupEffectProxy(num_effects)) {
  DCHECK_GE(num_effects, 1);
}

Animator::~Animator() = default;

void Animator::Trace(blink::Visitor* visitor) {
  visitor->Trace(definition_);
  visitor->Trace(group_effect_);
  visitor->Trace(instance_);
}

bool Animator::Animate(
    ScriptState* script_state,
    double current_time,
    AnimationWorkletDispatcherOutput::AnimationState* output) {
  v8::Isolate* isolate = script_state->GetIsolate();

  v8::Local<v8::Value> instance = instance_.NewLocal(isolate);
  v8::Local<v8::Function> animate = definition_->AnimateLocal(isolate);

  if (IsUndefinedOrNull(instance) || IsUndefinedOrNull(animate))
    return false;

  ScriptState::Scope scope(script_state);
  v8::TryCatch block(isolate);
  block.SetVerbose(true);

  // Prepare arguments (i.e., current time and effect) and pass them to animate
  // callback.
  v8::Local<v8::Value> v8_effect;
  if (group_effect_->getChildren().size() == 1) {
    v8_effect = ToV8(group_effect_->getChildren()[0],
                     script_state->GetContext()->Global(), isolate);
  } else {
    v8_effect =
        ToV8(group_effect_, script_state->GetContext()->Global(), isolate);
  }

  v8::Local<v8::Value> v8_current_time =
      ToV8(current_time, script_state->GetContext()->Global(), isolate);

  v8::Local<v8::Value> argv[] = {v8_current_time, v8_effect};

  V8ScriptRunner::CallFunction(animate, ExecutionContext::From(script_state),
                               instance, arraysize(argv), argv, isolate);

  // The animate function may have produced an error!
  // TODO(majidvp): We should probably just throw here.
  if (block.HasCaught())
    return false;

  output->local_times = GetLocalTimes();
  return true;
}

std::vector<base::Optional<TimeDelta>> Animator::GetLocalTimes() const {
  std::vector<base::Optional<TimeDelta>> local_times;
  local_times.reserve(group_effect_->getChildren().size());
  for (const auto& effect : group_effect_->getChildren()) {
    local_times.push_back(effect->local_time());
  }
  return local_times;
}

}  // namespace blink
