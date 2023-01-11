// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/abstract_haptic_gamepad.h"

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "device/gamepad/gamepad_data_fetcher.h"

namespace device {
namespace {
constexpr double kMaxDurationMillis = 5000.0;  // 5 seconds

bool IsValidEffectType(mojom::GamepadHapticEffectType type) {
  return type == mojom::GamepadHapticEffectType::
                     GamepadHapticEffectTypeDualRumble ||
         type == mojom::GamepadHapticEffectType::
                     GamepadHapticEffectTypeTriggerRumble;
}

}  // namespace

AbstractHapticGamepad::AbstractHapticGamepad() = default;

AbstractHapticGamepad::~AbstractHapticGamepad() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Shutdown() must be called to allow the device a chance to stop vibration
  // and release held resources.
  DCHECK(is_shut_down_);
}

void AbstractHapticGamepad::Shutdown() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!is_shutting_down_);
  is_shutting_down_ = true;

  // If an effect is still playing, try to stop vibration. This may fail if the
  // gamepad is already disconnected.
  if (playing_effect_callback_) {
    sequence_id_++;
    SetZeroVibration();
    GamepadDataFetcher::RunVibrationCallback(
        std::move(playing_effect_callback_), std::move(callback_runner_),
        mojom::GamepadHapticsResult::GamepadHapticsResultPreempted);
  }
  DoShutdown();

  // No vibration effects may be played once shutdown is complete.
  is_shut_down_ = true;
}

void AbstractHapticGamepad::SetZeroVibration() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  SetVibration(mojom::GamepadEffectParameters::New());
}

double AbstractHapticGamepad::GetMaxEffectDurationMillis() {
  return kMaxDurationMillis;
}

void AbstractHapticGamepad::PlayEffect(
    mojom::GamepadHapticEffectType type,
    mojom::GamepadEffectParametersPtr params,
    mojom::GamepadHapticsManager::PlayVibrationEffectOnceCallback callback,
    scoped_refptr<base::SequencedTaskRunner> callback_runner) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!is_shut_down_);
  if (!IsValidEffectType(type)) {
    // Only dual-rumble and trigger-rumble effects are supported.
    GamepadDataFetcher::RunVibrationCallback(
        std::move(callback), std::move(callback_runner),
        mojom::GamepadHapticsResult::GamepadHapticsResultNotSupported);
    return;
  }

  int sequence_id = ++sequence_id_;

  if (playing_effect_callback_) {
    // An effect is already playing on this device and will be preempted in
    // order to start the new effect. Finish the playing effect by calling its
    // callback with a "preempted" result code. Use the |callback_runner_| that
    // was provided with the playing effect as it may post tasks to a different
    // sequence than the |callback_runner| for the current effect.
    GamepadDataFetcher::RunVibrationCallback(
        std::move(playing_effect_callback_), std::move(callback_runner_),
        mojom::GamepadHapticsResult::GamepadHapticsResultPreempted);
  }
  if (params->start_delay > 0.0)
    SetZeroVibration();

  playing_effect_callback_ = std::move(callback);
  callback_runner_ = std::move(callback_runner);

  PlayVibrationEffect(sequence_id, std::move(params));
}

void AbstractHapticGamepad::ResetVibration(
    mojom::GamepadHapticsManager::ResetVibrationActuatorCallback callback,
    scoped_refptr<base::SequencedTaskRunner> callback_runner) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!is_shut_down_);
  sequence_id_++;

  SetZeroVibration();
  if (playing_effect_callback_) {
    // An effect is already playing on this device and will be preempted in
    // order to reset vibration. Finish the playing effect by calling its
    // callback with a "preempted" result code. Use the |callback_runner_| that
    // was provided with the playing effect as it may post tasks to a different
    // sequence than the |callback_runner| for the reset.
    GamepadDataFetcher::RunVibrationCallback(
        std::move(playing_effect_callback_), std::move(callback_runner_),
        mojom::GamepadHapticsResult::GamepadHapticsResultPreempted);
  }

  GamepadDataFetcher::RunVibrationCallback(
      std::move(callback), std::move(callback_runner),
      mojom::GamepadHapticsResult::GamepadHapticsResultComplete);
}

void AbstractHapticGamepad::PlayVibrationEffect(
    int sequence_id,
    mojom::GamepadEffectParametersPtr params) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  double duration = params->duration;
  double start_delay = params->start_delay;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&AbstractHapticGamepad::StartVibration, GetWeakPtr(),
                     sequence_id, duration, std::move(params)),
      base::Milliseconds(start_delay));
}

void AbstractHapticGamepad::StartVibration(
    int sequence_id,
    double duration,
    mojom::GamepadEffectParametersPtr params) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (is_shut_down_ || sequence_id != sequence_id_)
    return;
  SetVibration(params.Clone());

  const double max_duration = GetMaxEffectDurationMillis();
  if (duration > max_duration) {
    // The device does not support effects this long. Issue periodic vibration
    // commands until the effect is complete.
    double remaining_duration = duration - max_duration;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&AbstractHapticGamepad::StartVibration, GetWeakPtr(),
                       sequence_id, remaining_duration, params.Clone()),
        base::Milliseconds(max_duration));
  } else {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&AbstractHapticGamepad::FinishEffect, GetWeakPtr(),
                       sequence_id),
        base::Milliseconds(duration));
  }
}

void AbstractHapticGamepad::FinishEffect(int sequence_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (is_shut_down_ || sequence_id != sequence_id_)
    return;

  GamepadDataFetcher::RunVibrationCallback(
      std::move(playing_effect_callback_), std::move(callback_runner_),
      mojom::GamepadHapticsResult::GamepadHapticsResultComplete);
}

}  // namespace device
