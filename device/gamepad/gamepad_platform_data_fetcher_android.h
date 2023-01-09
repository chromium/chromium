// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Define the data fetcher that GamepadProvider will use for android port.
// (GamepadPlatformDataFetcher).

#ifndef DEVICE_GAMEPAD_GAMEPAD_PLATFORM_DATA_FETCHER_ANDROID_H_
#define DEVICE_GAMEPAD_GAMEPAD_PLATFORM_DATA_FETCHER_ANDROID_H_

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/task/sequenced_task_runner.h"
#include "device/gamepad/gamepad_data_fetcher.h"
#include "device/gamepad/gamepad_provider.h"
#include "device/gamepad/gamepad_standard_mappings.h"
#include "device/gamepad/haptic_gamepad_android.h"
#include "device/gamepad/public/cpp/gamepads.h"

namespace device {

class GamepadPlatformDataFetcherAndroid : public GamepadDataFetcher {
 public:
  using Factory =
      GamepadDataFetcherFactoryImpl<GamepadPlatformDataFetcherAndroid,
                                    GamepadSource::kAndroid>;

  GamepadPlatformDataFetcherAndroid();
  GamepadPlatformDataFetcherAndroid(GamepadPlatformDataFetcherAndroid&&) =
      delete;
  GamepadPlatformDataFetcherAndroid& operator=(
      GamepadPlatformDataFetcherAndroid&&) = delete;
  ~GamepadPlatformDataFetcherAndroid() override;

  // wrap java setVibration method.
  static void SetVibration(int device_index,
                           double strong_magnitude,
                           double weak_magnitude);
  // wrap java setZeroVibration method.
  static void SetZeroVibration(int device_index);

  GamepadSource source() override;

  void PauseHint(bool paused) override;

  void GetGamepadData(bool devices_changed_hint) override;

  void PlayEffect(
      int source_id,
      mojom::GamepadHapticEffectType type,
      mojom::GamepadEffectParametersPtr params,
      mojom::GamepadHapticsManager::PlayVibrationEffectOnceCallback callback,
      scoped_refptr<base::SequencedTaskRunner> callback_runner) override;

  void ResetVibration(
      int source_id,
      mojom::GamepadHapticsManager::ResetVibrationActuatorCallback callback,
      scoped_refptr<base::SequencedTaskRunner> callback_runner) override;

  void SetDualRumbleVibrationActuator(int source_id);

  void TryShutdownDualRumbleVibrationActuator(int source_id);

 private:
  using VibrationActuatorMap =
      base::flat_map<int, std::unique_ptr<HapticGamepadAndroid>>;

  void OnAddedToProvider() override;

  VibrationActuatorMap vibration_actuators_;
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_GAMEPAD_PLATFORM_DATA_FETCHER_ANDROID_H_
