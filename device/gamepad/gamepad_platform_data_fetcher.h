// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Define the default data fetcher that GamepadProvider will use if none is
// supplied. (GamepadPlatformDataFetcher).

#ifndef DEVICE_GAMEPAD_GAMEPAD_PLATFORM_DATA_FETCHER_H_
#define DEVICE_GAMEPAD_GAMEPAD_PLATFORM_DATA_FETCHER_H_

#include "base/compiler_specific.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "device/gamepad/gamepad_data_fetcher.h"
#include "device/gamepad/gamepad_data_fetcher_manager.h"
#include "device/gamepad/public/cpp/gamepad_features.h"

#if BUILDFLAG(IS_ANDROID)
#include "device/gamepad/gamepad_platform_data_fetcher_android.h"
#elif BUILDFLAG(IS_WIN)
#include "device/gamepad/nintendo_data_fetcher.h"
#include "device/gamepad/raw_input_data_fetcher_win.h"
#include "device/gamepad/wgi_data_fetcher_win.h"
#include "device/gamepad/xinput_data_fetcher_win.h"
#elif BUILDFLAG(IS_APPLE)
#include "device/gamepad/game_controller_data_fetcher_mac.h"
#if BUILDFLAG(IS_MAC)
#include "device/gamepad/gamepad_platform_data_fetcher_mac.h"
#include "device/gamepad/nintendo_data_fetcher.h"
#include "device/gamepad/xbox_data_fetcher_mac.h"
#endif
#elif (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && defined(USE_UDEV)
#include "device/gamepad/gamepad_platform_data_fetcher_linux.h"
#include "device/gamepad/nintendo_data_fetcher.h"
#endif

namespace device {

void AddGamepadPlatformDataFetchers(GamepadDataFetcherManager* manager) {
#if BUILDFLAG(IS_ANDROID)

  manager->AddFactory(new GamepadPlatformDataFetcherAndroid::Factory());

#elif BUILDFLAG(IS_WIN)

  // Windows.Gaming.Input is available in Windows 10.0.10240.0 and later.
  if (base::FeatureList::IsEnabled(
          features::kEnableWindowsGamingInputDataFetcher)) {
    manager->AddFactory(new WgiDataFetcherWin::Factory());
  } else {
    manager->AddFactory(new XInputDataFetcherWin::Factory());
  }
  manager->AddFactory(new NintendoDataFetcher::Factory());
  manager->AddFactory(new RawInputDataFetcher::Factory());

#elif BUILDFLAG(IS_APPLE)

  manager->AddFactory(new GameControllerDataFetcherMac::Factory());
#if BUILDFLAG(IS_MAC)
  manager->AddFactory(new GamepadPlatformDataFetcherMac::Factory());
  manager->AddFactory(new NintendoDataFetcher::Factory());
  manager->AddFactory(new XboxDataFetcher::Factory());
#endif

#elif (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && defined(USE_UDEV)

  manager->AddFactory(new GamepadPlatformDataFetcherLinux::Factory(
      base::SequencedTaskRunner::GetCurrentDefault()));
  manager->AddFactory(new NintendoDataFetcher::Factory());

#endif
}

}  // namespace device

#endif  // DEVICE_GAMEPAD_GAMEPAD_PLATFORM_DATA_FETCHER_H_
