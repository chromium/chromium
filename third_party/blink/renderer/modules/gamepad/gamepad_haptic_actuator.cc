// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/gamepad/gamepad_haptic_actuator.h"

#include "base/functional/callback_helpers.h"
#include "device/gamepad/public/cpp/gamepad.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gamepad_effect_parameters.h"
#include "third_party/blink/renderer/modules/gamepad/gamepad_dispatcher.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace {

using device::mojom::GamepadHapticsResult;
using device::mojom::GamepadHapticEffectType;

const char kGamepadHapticActuatorTypeVibration[] = "vibration";
const char kGamepadHapticActuatorTypeDualRumble[] = "dual-rumble";

const char kGamepadHapticEffectTypeDualRumble[] = "dual-rumble";
const char kGamepadHapticEffectTypeTriggerRumble[] = "trigger-rumble";

const char kGamepadHapticsResultComplete[] = "complete";
const char kGamepadHapticsResultPreempted[] = "preempted";
const char kGamepadHapticsResultInvalidParameter[] = "invalid-parameter";
const char kGamepadHapticsResultNotSupported[] = "not-supported";

GamepadHapticEffectType EffectTypeFromString(const String& type) {
  if (type == kGamepadHapticEffectTypeDualRumble)
    return GamepadHapticEffectType::GamepadHapticEffectTypeDualRumble;

  if (type == kGamepadHapticEffectTypeTriggerRumble)
    return GamepadHapticEffectType::GamepadHapticEffectTypeTriggerRumble;

  NOTREACHED();
  return GamepadHapticEffectType::GamepadHapticEffectTypeDualRumble;
}

String ResultToString(GamepadHapticsResult result) {
  switch (result) {
    case GamepadHapticsResult::GamepadHapticsResultComplete:
      return kGamepadHapticsResultComplete;
    case GamepadHapticsResult::GamepadHapticsResultPreempted:
      return kGamepadHapticsResultPreempted;
    case GamepadHapticsResult::GamepadHapticsResultInvalidParameter:
      return kGamepadHapticsResultInvalidParameter;
    case GamepadHapticsResult::GamepadHapticsResultNotSupported:
      return kGamepadHapticsResultNotSupported;
    default:
      NOTREACHED();
  }
  return kGamepadHapticsResultNotSupported;
}

}  // namespace

namespace blink {

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
  supported_effect_types_.clear();
  switch (type) {
    case device::GamepadHapticActuatorType::kVibration:
      type_ = kGamepadHapticActuatorTypeVibration;
      break;
    // Currently devices that have trigger rumble support, also have dual-rumble
    // support. Moreover, gamepads that support trigger-rumble should also be
    // listed as having GamepadHapticActuatorType::kDualRumble, since we want
    // to encourage the the use of 'canPlay' method instead.
    case device::GamepadHapticActuatorType::kTriggerRumble:
      supported_effect_types_.insert(kGamepadHapticEffectTypeTriggerRumble);
      [[fallthrough]];
    case device::GamepadHapticActuatorType::kDualRumble:
      supported_effect_types_.insert(kGamepadHapticEffectTypeDualRumble);
      type_ = kGamepadHapticActuatorTypeDualRumble;
      break;
    default:
      NOTREACHED();
  }
}

ScriptPromise GamepadHapticActuator::playEffect(
    ScriptState* script_state,
    const String& type,
    const GamepadEffectParameters* params) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);

  if (params->duration() < 0.0 || params->startDelay() < 0.0 ||
      params->strongMagnitude() < 0.0 || params->strongMagnitude() > 1.0 ||
      params->weakMagnitude() < 0.0 || params->weakMagnitude() > 1.0 ||
      params->leftTrigger() < 0.0 || params->leftTrigger() > 1.0 ||
      params->rightTrigger() < 0.0 || params->rightTrigger() > 1.0) {
    ScriptPromise promise = resolver->Promise();
    resolver->Resolve(kGamepadHapticsResultInvalidParameter);
    return promise;
  }

  // Limit the total effect duration.
  double effect_duration = params->duration() + params->startDelay();
  if (effect_duration >
      device::GamepadHapticActuator::kMaxEffectDurationMillis) {
    ScriptPromise promise = resolver->Promise();
    resolver->Resolve(kGamepadHapticsResultInvalidParameter);
    return promise;
  }

  // Avoid resetting vibration for a preempted effect.
  should_reset_ = false;

  auto callback = WTF::BindOnce(&GamepadHapticActuator::OnPlayEffectCompleted,
                                WrapPersistent(this), WrapPersistent(resolver));

  gamepad_dispatcher_->PlayVibrationEffectOnce(
      pad_index_, EffectTypeFromString(type),
      device::mojom::blink::GamepadEffectParameters::New(
          params->duration(), params->startDelay(), params->strongMagnitude(),
          params->weakMagnitude(), params->leftTrigger(),
          params->rightTrigger()),
      std::move(callback));

  return resolver->Promise();
}

void GamepadHapticActuator::OnPlayEffectCompleted(
    ScriptPromiseResolver* resolver,
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
  resolver->Resolve(ResultToString(result));
}

void GamepadHapticActuator::ResetVibrationIfNotPreempted() {
  if (should_reset_) {
    should_reset_ = false;
    gamepad_dispatcher_->ResetVibrationActuator(pad_index_, base::DoNothing());
  }
}

ScriptPromise GamepadHapticActuator::reset(ScriptState* script_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);

  auto callback = WTF::BindOnce(&GamepadHapticActuator::OnResetCompleted,
                                WrapPersistent(this), WrapPersistent(resolver));

  gamepad_dispatcher_->ResetVibrationActuator(pad_index_, std::move(callback));

  return resolver->Promise();
}

void GamepadHapticActuator::OnResetCompleted(
    ScriptPromiseResolver* resolver,
    device::mojom::GamepadHapticsResult result) {
  if (result == GamepadHapticsResult::GamepadHapticsResultError) {
    resolver->Reject();
    return;
  }
  resolver->Resolve(ResultToString(result));
}

bool GamepadHapticActuator::canPlay(const String& type) {
  return supported_effect_types_.Contains(type);
}

void GamepadHapticActuator::Trace(Visitor* visitor) const {
  visitor->Trace(gamepad_dispatcher_);
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

}  // namespace blink
