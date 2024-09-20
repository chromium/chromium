// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/animationworklet/animation_worklet_global_scope.h"

#include <optional>

#include "base/time/time.h"
#include "third_party/blink/renderer/bindings/core/v8/generated_code_helper.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_function.h"
#include "third_party/blink/renderer/bindings/core/v8/worker_or_worklet_script_controller.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_animate_callback.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_animator_constructor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_state_callback.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/modules/animationworklet/animation_worklet_proxy_client.h"
#include "third_party/blink/renderer/modules/animationworklet/worklet_animation_effect_timings.h"
#include "third_party/blink/renderer/modules/animationworklet/worklet_animation_options.h"
#include "third_party/blink/renderer/platform/bindings/callback_method_retriever.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding_macros.h"
#include "third_party/blink/renderer/platform/bindings/v8_object_constructor.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

void UpdateAnimation(v8::Isolate* isolate,
                     Animator* animator,
                     WorkletAnimationId id,
                     double current_time,
                     AnimationWorkletDispatcherOutput* result) {
  AnimationWorkletDispatcherOutput::AnimationState animation_output(id);
  if (animator->Animate(isolate, current_time, &animation_output)) {
    result->animations.push_back(std::move(animation_output));
  }
}

}  // namespace

AnimationWorkletGlobalScope::AnimationWorkletGlobalScope(
    std::unique_ptr<GlobalScopeCreationParams> creation_params,
    WorkerThread* thread)
    : WorkletGlobalScope(std::move(creation_params),
                         thread->GetWorkerReportingProxy(),
                         thread) {}

AnimationWorkletGlobalScope::~AnimationWorkletGlobalScope() = default;

void AnimationWorkletGlobalScope::Trace(Visitor* visitor) const {
  visitor->Trace(animator_definitions_);
  visitor->Trace(animators_);
  WorkletGlobalScope::Trace(visitor);
}

void AnimationWorkletGlobalScope::Dispose() {
  DCHECK(IsContextThread());
  if (AnimationWorkletProxyClient* proxy_client =
          AnimationWorkletProxyClient::From(Clients()))
    proxy_client->Dispose();
  WorkletGlobalScope::Dispose();
}

Animator* AnimationWorkletGlobalScope::CreateAnimatorFor(
    int animation_id,
    const String& name,
    WorkletAnimationOptions options,
    scoped_refptr<SerializedScriptValue> serialized_state,
    const Vector<std::optional<base::TimeDelta>>& local_times,
    const Vector<Timing>& timings,
    const Vector<Timing::NormalizedTiming>& normalized_timings) {
  DCHECK(!animators_.Contains(animation_id));
  Animator* animator = CreateInstance(name, options, serialized_state,
                                      local_times, timings, normalized_timings);
  if (!animator)
    return nullptr;
  animators_.Set(animation_id, animator);

  return animator;
}

void AnimationWorkletGlobalScope::UpdateAnimatorsList(
    const AnimationWorkletInput& input) {
  DCHECK(IsContextThread());

  ScriptState* script_state = ScriptController()->GetScriptState();
  ScriptState::Scope scope(script_state);

  for (const auto& worklet_animation_id : input.removed_animations)
    animators_.erase(worklet_animation_id.animation_id);

  for (const auto& animation : input.added_and_updated_animations) {
    int id = animation.worklet_animation_id.animation_id;
    DCHECK(!animators_.Contains(id));
    const String name = String::FromUTF8(animation.name);

    WorkletAnimationOptions options(nullptr);
    // Down casting to blink type to access the serialized value.
    if (animation.options) {
      options =
          *(static_cast<WorkletAnimationOptions*>(animation.options.get()));
    }

    // Down casting to blink type
    WorkletAnimationEffectTimings* effect_timings =
        (static_cast<WorkletAnimationEffectTimings*>(
            animation.effect_timings.get()));
    Vector<Timing> timings = effect_timings->GetTimings()->data;
    DCHECK_GE(timings.size(), 1u);
    Vector<Timing::NormalizedTiming> normalized_timings =
        effect_timings->GetNormalizedTimings()->data;
    DCHECK_GE(normalized_timings.size(), 1u);

    Vector<std::optional<base::TimeDelta>> local_times(
        static_cast<int>(timings.size()), std::nullopt);

    CreateAnimatorFor(id, name, options, nullptr /* serialized_state */,
                      local_times, timings, normalized_timings);
  }
}

void AnimationWorkletGlobalScope::UpdateAnimators(
    const AnimationWorkletInput& input,
    AnimationWorkletOutput* output,
    bool (*predicate)(Animator*)) {
  DCHECK(IsContextThread());

  ScriptState* script_state = ScriptController()->GetScriptState();
  v8::Isolate* isolate = script_state->GetIsolate();
  ScriptState::Scope scope(script_state);

  for (const auto& animation : input.added_and_updated_animations) {
    // We don't try to create an animator if there isn't any.
    // This can only happen if constructing an animator instance has failed
    // e.g., the constructor throws an exception.
    auto it = animators_.find(animation.worklet_animation_id.animation_id);
    if (it == animators_.end())
      continue;

    Animator* animator = it->value;
    if (!predicate(animator))
      continue;

    UpdateAnimation(isolate, animator, animation.worklet_animation_id,
                    animation.current_time, output);
  }

  for (const auto& animation : input.updated_animations) {
    // We don't try to create an animator if there isn't any.
    auto it = animators_.find(animation.worklet_animation_id.animation_id);
    if (it == animators_.end())
      continue;

    Animator* animator = it->value;
    if (!predicate(animator))
      continue;

    UpdateAnimation(isolate, animator, animation.worklet_animation_id,
                    animation.current_time, output);
  }
}

void AnimationWorkletGlobalScope::RegisterWithProxyClientIfNeeded() {
  if (registered_)
    return;

  if (AnimationWorkletProxyClient* proxy_client =
          AnimationWorkletProxyClient::From(Clients())) {
    proxy_client->AddGlobalScope(this);
    registered_ = true;
  }
}

void AnimationWorkletGlobalScope::registerAnimator(
    const String& name,
    V8AnimatorConstructor* animator_ctor,
    ExceptionState& exception_state) {
  RegisterWithProxyClientIfNeeded();

  DCHECK(IsContextThread());
  if (animator_definitions_.Contains(name)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "A class with name:'" + name + "' is already registered.");
    return;
  }

  if (name.empty()) {
    exception_state.ThrowTypeError("The empty string is not a valid name.");
    return;
  }

  if (!animator_ctor->IsConstructor()) {
    exception_state.ThrowTypeError(
        "The provided callback is not a constructor.");
    return;
  }

  CallbackMethodRetriever retriever(animator_ctor);
  retriever.GetPrototypeObject(exception_state);
  if (exception_state.HadException())
    return;
  v8::Local<v8::Function> v8_animate =
      retriever.GetMethodOrThrow("animate", exception_state);
  if (exception_state.HadException())
    return;
  V8AnimateCallback* animate = V8AnimateCallback::Create(v8_animate);

  v8::Local<v8::Value> v8_state =
      retriever.GetMethodOrUndefined("state", exception_state);
  if (exception_state.HadException())
    return;

  V8StateCallback* state =
      v8_state->IsFunction()
          ? V8StateCallback::Create(v8_state.As<v8::Function>())
          : nullptr;

  AnimatorDefinition* definition =
      MakeGarbageCollected<AnimatorDefinition>(animator_ctor, animate, state);

  // TODO(https://crbug.com/923063): Ensure worklet definitions are compatible
  // across global scopes.
  animator_definitions_.Set(name, definition);
  // TODO(crbug.com/920722): Currently one animator name is synced back per
  // registration. Eventually all registered names should be synced in batch
  // once a module completes its loading in the worklet scope.
  if (AnimationWorkletProxyClient* proxy_client =
          AnimationWorkletProxyClient::From(Clients())) {
    proxy_client->SynchronizeAnimatorName(name);
  }
}

Animator* AnimationWorkletGlobalScope::CreateInstance(
    const String& name,
    WorkletAnimationOptions options,
    scoped_refptr<SerializedScriptValue> serialized_state,
    const Vector<std::optional<base::TimeDelta>>& local_times,
    const Vector<Timing>& timings,
    const Vector<Timing::NormalizedTiming>& normalized_timings) {
  DCHECK(IsContextThread());
  AnimatorDefinition* definition = animator_definitions_.at(name);
  if (!definition)
    return nullptr;

  ScriptState* script_state = ScriptController()->GetScriptState();
  ScriptState::Scope scope(script_state);
  v8::Isolate* isolate = script_state->GetIsolate();

  v8::TryCatch try_catch(isolate);
  try_catch.SetVerbose(true);

  v8::Local<v8::Value> v8_options =
      options.GetData() ? options.GetData()->Deserialize(isolate)
                        : v8::Undefined(isolate).As<v8::Value>();
  v8::Local<v8::Value> v8_state = serialized_state
                                      ? serialized_state->Deserialize(isolate)
                                      : v8::Undefined(isolate).As<v8::Value>();
  ScriptValue options_value(isolate, v8_options);
  ScriptValue state_value(isolate, v8_state);

  ScriptValue instance;
  if (!definition->ConstructorFunction()
           ->Construct(options_value, state_value)
           .To(&instance)) {
    return nullptr;
  }

  return MakeGarbageCollected<Animator>(isolate, definition, instance.V8Value(),
                                        name, std::move(options), local_times,
                                        timings, normalized_timings);
}

bool AnimationWorkletGlobalScope::IsAnimatorStateful(int animation_id) {
  return animators_.at(animation_id)->IsStateful();
}

// Implementation of "Migrating an Animator Instance":
// https://drafts.css-houdini.org/css-animationworklet/#migrating-animator
// Note that per specification if the state function does not exist, the
// migration process should be aborted. However the following implementation
// is used for both the stateful and stateless animators. For the latter ones
// the migration (including name, options etc.) should be completed regardless
// the state function.
void AnimationWorkletGlobalScope::MigrateAnimatorsTo(
    AnimationWorkletGlobalScope* target_global_scope) {
  DCHECK_NE(this, target_global_scope);

  ScriptState* script_state = ScriptController()->GetScriptState();
  ScriptState::Scope scope(script_state);
  v8::Isolate* isolate = script_state->GetIsolate();

  for (const auto& animator_map : animators_) {
    int animation_id = animator_map.key;
    Animator* animator = animator_map.value;
    scoped_refptr<SerializedScriptValue> serialized_state;
    if (animator->IsStateful()) {
      v8::TryCatch try_catch(isolate);
      // If an animator state function throws or the state is not
      // serializable, the animator will be removed from the global scope.
      // TODO(crbug.com/1090522): We should post an error message to console in
      // case of exceptions.
      v8::Local<v8::Value> state =
          animator->State(isolate, PassThroughException(isolate));
      if (try_catch.HasCaught()) {
        continue;
      }

      // Do not skip migrating the stateful animator if its state is
      // undefined.
      if (!state->IsNullOrUndefined()) {
        serialized_state = SerializedScriptValue::Serialize(
            isolate, state, SerializedScriptValue::SerializeOptions(),
            PassThroughException(isolate));
        if (try_catch.HasCaught()) {
          continue;
        }
      }
    }

    Vector<std::optional<base::TimeDelta>> local_times;
    animator->GetLocalTimes(local_times);
    target_global_scope->CreateAnimatorFor(
        animation_id, animator->name(), animator->options(), serialized_state,
        std::move(local_times), animator->GetTimings(),
        animator->GetNormalizedTimings());
  }
  animators_.clear();
}

AnimatorDefinition* AnimationWorkletGlobalScope::FindDefinitionForTest(
    const String& name) {
  auto it = animator_definitions_.find(name);
  if (it != animator_definitions_.end())
    return it->value.Get();
  return nullptr;
}

}  // namespace blink
