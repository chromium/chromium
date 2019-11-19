// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Define the default data fetcher that GamepadProvider will use if none is
// supplied. (GamepadPlatformDataFetcher).

#ifndef DEVICE_GAMEPAD_GAMEPAD_PLATFORM_DATA_FETCHER_H_
#define DEVICE_GAMEPAD_GAMEPAD_PLATFORM_DATA_FETCHER_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "device/gamepad/gamepad_data_fetcher.h"
#include "device/gamepad/gamepad_data_fetcher_manager.h"

#if defined(OS_ANDROID)
#include "device/gamepad/gamepad_platform_data_fetcher_android.h"
#elif defined(OS_WIN)
#include "device/gamepad/gamepad_platform_data_fetcher_win.h"
#include "device/gamepad/nintendo_data_fetcher.h"
#include "device/gamepad/raw_input_data_fetcher_win.h"
#elif defined(OS_MACOSX)
#include "device/gamepad/game_controller_data_fetcher_mac.h"
#include "device/gamepad/gamepad_platform_data_fetcher_mac.h"
#include "device/gamepad/nintendo_data_fetcher.h"
#include "device/gamepad/xbox_data_fetcher_mac.h"
#elif defined(OS_LINUX) && defined(USE_UDEV)
#include "device/gamepad/gamepad_platform_data_fetcher_linux.h"
#include "device/gamepad/nintendo_data_fetcher.h"
#endif

namespace device {

void AddGamepadPlatformDataFetchers(GamepadDataFetcherManager* manager) {
#if defined(OS_ANDROID)

  manager->AddFactory(new GamepadPlatformDataFetcherAndroid::Factory());

#elif defined(OS_WIN)

  manager->AddFactory(new GamepadPlatformDataFetcherWin::Factory());
  manager->AddFactory(new NintendoDataFetcher::Factory());
  manager->AddFactory(new RawInputDataFetcher::Factory());

#elif defined(OS_MACOSX)

  manager->AddFactory(new GameControllerDataFetcherMac::Factory());
  manager->AddFactory(new GamepadPlatformDataFetcherMac::Factory());
  manager->AddFactory(new NintendoDataFetcher::Factory());
  manager->AddFactory(new XboxDataFetcher::Factory());

#elif defined(OS_LINUX) && defined(USE_UDEV)

  manager->AddFactory(new GamepadPlatformDataFetcherLinux::Factory(
      base::SequencedTaskRunnerHandle::Get()));
  manager->AddFactory(new NintendoDataFetcher::Factory());

#endif
}

}  // namespace device

#endif  // DEVICE_GAMEPAD_GAMEPAD_PLATFORM_DATA_FETCHER_H_
