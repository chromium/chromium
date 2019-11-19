// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/gamepad/gamepad_haptic_actuator.h"

#include "base/bind_helpers.h"
#include "device/gamepad/public/cpp/gamepad.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/modules/gamepad/gamepad_dispatcher.h"
#include "third_party/blink/renderer/modules/gamepad/gamepad_effect_parameters.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace {

using device::mojom::GamepadHapticsResult;
using device::mojom::GamepadHapticEffectType;

const char kGamepadHapticActuatorTypeVibration[] = "vibration";
const char kGamepadHapticActuatorTypeDualRumble[] = "dual-rumble";

const char kGamepadHapticEffectTypeDualRumble[] = "dual-rumble";

const char kGamepadHapticsResultComplete[] = "complete";
const char kGamepadHapticsResultPreempted[] = "preempted";
const char kGamepadHapticsResultInvalidParameter[] = "invalid-parameter";
const char kGamepadHapticsResultNotSupported[] = "not-supported";

GamepadHapticEffectType EffectTypeFromString(const String& type) {
  if (type == kGamepadHapticEffectTypeDualRumble)
    return GamepadHapticEffectType::GamepadHapticEffectTypeDualRumble;

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

// static
GamepadHapticActuator* GamepadHapticActuator::Create(ExecutionContext* context,
                                                     int pad_index) {
  return MakeGarbageCollected<GamepadHapticActuator>(
      context, pad_index, device::GamepadHapticActuatorType::kDualRumble);
}

GamepadHapticActuator::GamepadHapticActuator(
    ExecutionContext* context,
    int pad_index,
    device::GamepadHapticActuatorType type)
    : ContextClient(context),
      pad_index_(pad_index),
      // See https://bit.ly/2S0zRAS for task types
      gamepad_dispatcher_(MakeGarbageCollected<GamepadDispatcher>(
          context->GetTaskRunner(TaskType::kMiscPlatformAPI))) {
  SetType(type);
}

GamepadHapticActuator::~GamepadHapticActuator() = default;

void GamepadHapticActuator::SetType(device::GamepadHapticActuatorType type) {
  switch (type) {
    case device::GamepadHapticActuatorType::kVibration:
      type_ = kGamepadHapticActuatorTypeVibration;
      break;
    case device::GamepadHapticActuatorType::kDualRumble:
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
      params->weakMagnitude() < 0.0 || params->weakMagnitude() > 1.0) {
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

  auto callback = WTF::Bind(&GamepadHapticActuator::OnPlayEffectCompleted,
                            WrapPersistent(this), WrapPersistent(resolver));

  gamepad_dispatcher_->PlayVibrationEffectOnce(
      pad_index_, EffectTypeFromString(type),
      device::mojom::blink::GamepadEffectParameters::New(
          params->duration(), params->startDelay(), params->strongMagnitude(),
          params->weakMagnitude()),
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
          ->PostTask(
              FROM_HERE,
              WTF::Bind(&GamepadHapticActuator::ResetVibrationIfNotPreempted,
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

  auto callback = WTF::Bind(&GamepadHapticActuator::OnResetCompleted,
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

void GamepadHapticActuator::Trace(blink::Visitor* visitor) {
  visitor->Trace(gamepad_dispatcher_);
  ScriptWrappable::Trace(visitor);
  ContextClient::Trace(visitor);
}

}  // namespace blink
