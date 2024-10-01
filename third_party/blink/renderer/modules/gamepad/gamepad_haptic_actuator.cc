// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/gamepad/gamepad_haptic_actuator.h"

#include "base/functional/callback_helpers.h"
#include "device/gamepad/public/cpp/gamepad.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gamepad_effect_parameters.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gamepad_haptic_effect_type.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gamepad_haptics_result.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/gamepad/gamepad_dispatcher.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

using device::mojom::GamepadHapticsResult;
using device::mojom::GamepadHapticEffectType;

GamepadHapticEffectType EffectTypeFromEnum(
    V8GamepadHapticEffectType::Enum type) {
  switch (type) {
    case V8GamepadHapticEffectType::Enum::kDualRumble:
      return GamepadHapticEffectType::GamepadHapticEffectTypeDualRumble;
    case V8GamepadHapticEffectType::Enum::kTriggerRumble:
      return GamepadHapticEffectType::GamepadHapticEffectTypeTriggerRumble;
  }
  NOTREACHED();
}

V8GamepadHapticsResult ResultToV8(GamepadHapticsResult result) {
  switch (result) {
    case GamepadHapticsResult::GamepadHapticsResultComplete:
      return V8GamepadHapticsResult(V8GamepadHapticsResult::Enum::kComplete);
    case GamepadHapticsResult::GamepadHapticsResultPreempted:
      return V8GamepadHapticsResult(V8GamepadHapticsResult::Enum::kPreempted);
    case GamepadHapticsResult::GamepadHapticsResultInvalidParameter:
      return V8GamepadHapticsResult(
          V8GamepadHapticsResult::Enum::kInvalidParameter);
    case GamepadHapticsResult::GamepadHapticsResultNotSupported:
      return V8GamepadHapticsResult(
          V8GamepadHapticsResult::Enum::kNotSupported);
    default:
      NOTREACHED_IN_MIGRATION();
  }
  return V8GamepadHapticsResult(V8GamepadHapticsResult::Enum::kNotSupported);
}

}  // namespace

GamepadHapticActuator::GamepadHapticActuator(
    ExecutionContext& context,
    int pad_index,
    device::GamepadHapticActuatorType type)
    : ExecutionContextClient(&context),
      pad_index_(pad_index),
      gamepad_dispatcher_(MakeGarbageCollected<GamepadDispatcher>(context)) {
  SetType(type);
}

GamepadHapticActuator::~GamepadHapticActuator() = default;

void GamepadHapticActuator::SetType(device::GamepadHapticActuatorType type) {
  supported_effects_.clear();
  switch (type) {
    case device::GamepadHapticActuatorType::kVibration:
      type_ = V8GamepadHapticActuatorType::Enum::kVibration;
      break;
    // Currently devices that have trigger rumble support, also have dual-rumble
    // support.
    case device::GamepadHapticActuatorType::kTriggerRumble:
      supported_effects_.push_back(V8GamepadHapticEffectType(
          V8GamepadHapticEffectType::Enum::kTriggerRumble));
      [[fallthrough]];
    case device::GamepadHapticActuatorType::kDualRumble:
      supported_effects_.push_back(V8GamepadHapticEffectType(
          V8GamepadHapticEffectType::Enum::kDualRumble));
      type_ = V8GamepadHapticActuatorType::Enum::kDualRumble;
      break;
  }
}

ScriptPromise<V8GamepadHapticsResult> GamepadHapticActuator::playEffect(
    ScriptState* script_state,
    const V8GamepadHapticEffectType& type,
    const GamepadEffectParameters* params) {
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<V8GamepadHapticsResult>>(
          script_state);
  auto promise = resolver->Promise();

  if (params->duration() < 0.0 || params->startDelay() < 0.0 ||
      params->strongMagnitude() < 0.0 || params->strongMagnitude() > 1.0 ||
      params->weakMagnitude() < 0.0 || params->weakMagnitude() > 1.0 ||
      params->leftTrigger() < 0.0 || params->leftTrigger() > 1.0 ||
      params->rightTrigger() < 0.0 || params->rightTrigger() > 1.0) {
    resolver->Resolve(
        ResultToV8(GamepadHapticsResult::GamepadHapticsResultInvalidParameter));
    return promise;
  }

  // Limit the total effect duration.
  double effect_duration = params->duration() + params->startDelay();
  if (effect_duration >
      device::GamepadHapticActuator::kMaxEffectDurationMillis) {
    resolver->Resolve(
        ResultToV8(GamepadHapticsResult::GamepadHapticsResultInvalidParameter));
    return promise;
  }

  // Avoid resetting vibration for a preempted effect.
  should_reset_ = false;

  auto callback = WTF::BindOnce(&GamepadHapticActuator::OnPlayEffectCompleted,
                                WrapPersistent(this), WrapPersistent(resolver));

  gamepad_dispatcher_->PlayVibrationEffectOnce(
      pad_index_, EffectTypeFromEnum(type.AsEnum()),
      device::mojom::blink::GamepadEffectParameters::New(
          params->duration(), params->startDelay(), params->strongMagnitude(),
          params->weakMagnitude(), params->leftTrigger(),
          params->rightTrigger()),
      std::move(callback));

  return promise;
}

void GamepadHapticActuator::OnPlayEffectCompleted(
    ScriptPromiseResolver<V8GamepadHapticsResult>* resolver,
    device::mojom::GamepadHapticsResult result) {
  if (result == GamepadHapticsResult::GamepadHapticsResultError) {
    resolver->Reject();
    return;
  } else if (result == GamepadHapticsResult::GamepadHapticsResultComplete) {
    should_reset_ = true;
    ExecutionContext* context = GetExecutionContext();
    if (context) {
      // Post a delayed task to stop vibration. The task will be run after all
      // callbacks have run for the effect Promise, and may be ignored by
      // setting |should_reset_| to false. The intention is to only stop
      // vibration if the user did not chain another vibration effect in the
      // Promise callback.
      context->GetTaskRunner(TaskType::kMiscPlatformAPI)
          ->PostTask(FROM_HERE,
                     WTF::BindOnce(
                         &GamepadHapticActuator::ResetVibrationIfNotPreempted,
                         WrapPersistent(this)));
    } else {
      // The execution context is gone, meaning no new effects can be issued by
      // the page. Stop vibration without waiting for Promise callbacks.
      ResetVibrationIfNotPreempted();
    }
  }
  resolver->Resolve(ResultToV8(result));
}

void GamepadHapticActuator::ResetVibrationIfNotPreempted() {
  if (should_reset_) {
    should_reset_ = false;
    gamepad_dispatcher_->ResetVibrationActuator(pad_index_, base::DoNothing());
  }
}

ScriptPromise<V8GamepadHapticsResult> GamepadHapticActuator::reset(
    ScriptState* script_state) {
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<V8GamepadHapticsResult>>(
          script_state);

  auto callback = WTF::BindOnce(&GamepadHapticActuator::OnResetCompleted,
                                WrapPersistent(this), WrapPersistent(resolver));

  gamepad_dispatcher_->ResetVibrationActuator(pad_index_, std::move(callback));

  return resolver->Promise();
}

void GamepadHapticActuator::OnResetCompleted(
    ScriptPromiseResolver<V8GamepadHapticsResult>* resolver,
    device::mojom::GamepadHapticsResult result) {
  if (result == GamepadHapticsResult::GamepadHapticsResultError) {
    resolver->Reject();
    return;
  }
  resolver->Resolve(ResultToV8(result));
}

void GamepadHapticActuator::Trace(Visitor* visitor) const {
  visitor->Trace(gamepad_dispatcher_);
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

}  // namespace blink
