// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/animationworklet/animator.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_animate_callback.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_state_callback.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_workletanimationeffect_workletgroupeffect.h"
#include "third_party/blink/renderer/modules/animationworklet/animator_definition.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "v8/include/v8.h"

namespace blink {

Animator::Animator(v8::Isolate* isolate,
                   AnimatorDefinition* definition,
                   v8::Local<v8::Value> instance,
                   const String& name,
                   WorkletAnimationOptions options,
                   const Vector<std::optional<base::TimeDelta>>& local_times,
                   const Vector<Timing>& timings,
                   const Vector<Timing::NormalizedTiming>& normalized_timings)
    : definition_(definition),
      instance_(isolate, instance),
      name_(name),
      options_(options),
      group_effect_(
          MakeGarbageCollected<WorkletGroupEffect>(local_times,
                                                   timings,
                                                   normalized_timings)) {
  DCHECK_GE(local_times.size(), 1u);
}

Animator::~Animator() = default;

void Animator::Trace(Visitor* visitor) const {
  visitor->Trace(definition_);
  visitor->Trace(instance_);
  visitor->Trace(group_effect_);
}

bool Animator::Animate(
    v8::Isolate* isolate,
    double current_time,
    AnimationWorkletDispatcherOutput::AnimationState* output) {
  DCHECK(!std::isnan(current_time));

  v8::Local<v8::Value> instance = instance_.Get(isolate);
  if (IsUndefinedOrNull(instance))
    return false;

  V8UnionWorkletAnimationEffectOrWorkletGroupEffect* effect = nullptr;
  if (group_effect_->getChildren().size() == 1) {
    effect =
        MakeGarbageCollected<V8UnionWorkletAnimationEffectOrWorkletGroupEffect>(
            group_effect_->getChildren()[0]);
  } else {
    effect =
        MakeGarbageCollected<V8UnionWorkletAnimationEffectOrWorkletGroupEffect>(
            group_effect_);
  }

  v8::TryCatch try_catch(isolate);
  try_catch.SetVerbose(true);
  if (definition_->AnimateFunction()
          ->Invoke(instance, current_time, effect)
          .IsNothing()) {
    return false;
  }

  GetLocalTimes(output->local_times);
  return true;
}

Vector<Timing> Animator::GetTimings() const {
  Vector<Timing> timings;
  timings.ReserveInitialCapacity(group_effect_->getChildren().size());
  for (const auto& effect : group_effect_->getChildren()) {
    timings.push_back(effect->SpecifiedTiming());
  }
  return timings;
}

Vector<Timing::NormalizedTiming> Animator::GetNormalizedTimings() const {
  Vector<Timing::NormalizedTiming> normalized_timings;
  normalized_timings.ReserveInitialCapacity(
      group_effect_->getChildren().size());
  for (const auto& effect : group_effect_->getChildren()) {
    normalized_timings.push_back(effect->NormalizedTiming());
  }
  return normalized_timings;
}

bool Animator::IsStateful() const {
  return definition_->IsStateful();
}

v8::Local<v8::Value> Animator::State(v8::Isolate* isolate,
                                     ExceptionState& exception_state) {
  if (!IsStateful())
    return v8::Undefined(isolate);

  v8::Local<v8::Value> instance = instance_.Get(isolate);
  DCHECK(!IsUndefinedOrNull(instance));

  TryRethrowScope rethrow_scope(isolate, exception_state);
  v8::Maybe<ScriptValue> state = definition_->StateFunction()->Invoke(instance);
  if (rethrow_scope.HasCaught()) {
    return v8::Undefined(isolate);
  }
  return state.ToChecked().V8Value();
}

}  // namespace blink
