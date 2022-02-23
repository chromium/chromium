// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Define the data fetcher that GamepadProvider will use for android port.
// (GamepadPlatformDataFetcher).

#ifndef DEVICE_GAMEPAD_GAMEPAD_PLATFORM_DATA_FETCHER_ANDROID_H_
#define DEVICE_GAMEPAD_GAMEPAD_PLATFORM_DATA_FETCHER_ANDROID_H_

#include <jni.h>

#include "base/android/jni_android.h"
#include "device/gamepad/gamepad_data_fetcher.h"
#include "device/gamepad/gamepad_provider.h"
#include "device/gamepad/gamepad_standard_mappings.h"
#include "device/gamepad/public/cpp/gamepads.h"

namespace device {

class GamepadPlatformDataFetcherAndroid : public GamepadDataFetcher {
 public:
  typedef GamepadDataFetcherFactoryImpl<GamepadPlatformDataFetcherAndroid,
                                        GAMEPAD_SOURCE_ANDROID>
      Factory;

  GamepadPlatformDataFetcherAndroid();
  GamepadPlatformDataFetcherAndroid(GamepadPlatformDataFetcherAndroid&&) =
      delete;
  GamepadPlatformDataFetcherAndroid& operator=(
      GamepadPlatformDataFetcherAndroid&&) = delete;
  ~GamepadPlatformDataFetcherAndroid() override;

  GamepadSource source() override;

  void PauseHint(bool paused) override;

  void GetGamepadData(bool devices_changed_hint) override;

 private:
  void OnAddedToProvider() override;
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_GAMEPAD_PLATFORM_DATA_FETCHER_ANDROID_H_
