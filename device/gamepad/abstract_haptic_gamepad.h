// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_ABSTRACT_HAPTIC_GAMEPAD_
#define DEVICE_GAMEPAD_ABSTRACT_HAPTIC_GAMEPAD_

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "device/gamepad/gamepad_export.h"
#include "device/gamepad/public/mojom/gamepad.mojom.h"

namespace device {

// AbstractHapticGamepad is a base class for gamepads that support dual-rumble
// vibration effects. To use it, override the SetVibration method so that it
// sets the vibration intensity on the device. Then, calling PlayEffect or
// ResetVibration will call your SetVibration method at the appropriate times
// to produce the desired vibration effect. When the effect is complete, or when
// it has been preempted by another effect, the callback is invoked with a
// result code.
//
// By default, SetZeroVibration simply calls SetVibration with both parameters
// set to zero. You may optionally override SetZeroVibration if the device has a
// more efficient means of stopping an ongoing effect.
class DEVICE_GAMEPAD_EXPORT AbstractHapticGamepad {
 public:
  AbstractHapticGamepad();
  virtual ~AbstractHapticGamepad();

  // Start playing a haptic effect of type |type|, described by |params|. When
  // the effect is complete, or if it encounters an error, the result code is
  // passed back to the caller on its own sequence by calling |callback| using
  // |callback_runner|.
  void PlayEffect(
      mojom::GamepadHapticEffectType type,
      mojom::GamepadEffectParametersPtr params,
      mojom::GamepadHapticsManager::PlayVibrationEffectOnceCallback callback,
      scoped_refptr<base::SequencedTaskRunner> callback_runner);

  // Reset vibration on the gamepad, perhaps interrupting an ongoing effect. A
  // result code is passed back to the caller on its own sequence by calling
  // |callback| using |callback_runner|.
  void ResetVibration(
      mojom::GamepadHapticsManager::ResetVibrationActuatorCallback callback,
      scoped_refptr<base::SequencedTaskRunner> callback_runner);

  // Stop vibration effects, run callbacks, and release held resources. Must be
  // called exactly once before the device is destroyed.
  void Shutdown();

  // Returns true if Shutdown() has been called.
  bool IsShuttingDown() { return is_shutting_down_; }

  // Set the vibration magnitude for the strong and weak vibration actuators.
  virtual void SetVibration(double strong_magnitude, double weak_magnitude) = 0;

  // Set the vibration magnitude for both actuators to zero.
  virtual void SetZeroVibration();

  // The maximum effect duration supported by this device. Long-running effects
  // must be divided into effects of this duration or less.
  virtual double GetMaxEffectDurationMillis();

  virtual base::WeakPtr<AbstractHapticGamepad> GetWeakPtr() = 0;

 private:
  // Override to perform additional shutdown actions after vibration effects
  // are halted and callbacks are issued.
  virtual void DoShutdown() {}

  void PlayDualRumbleEffect(int sequence_id,
                            double duration,
                            double start_delay,
                            double strong_magnitude,
                            double weak_magnitude);
  void StartVibration(int sequence_id,
                      double duration,
                      double strong_magnitude,
                      double weak_magnitude);
  void FinishEffect(int sequence_id);

  bool is_shutting_down_ = false;
  bool is_shut_down_ = false;
  int sequence_id_ = 0;
  mojom::GamepadHapticsManager::PlayVibrationEffectOnceCallback
      playing_effect_callback_;
  scoped_refptr<base::SequencedTaskRunner> callback_runner_;
  THREAD_CHECKER(thread_checker_);
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_ABSTRACT_HAPTIC_GAMEPAD_
