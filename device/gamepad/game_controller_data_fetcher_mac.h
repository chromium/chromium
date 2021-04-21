// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_GAME_CONTROLLER_DATA_FETCHER_MAC_H_
#define DEVICE_GAMEPAD_GAME_CONTROLLER_DATA_FETCHER_MAC_H_

#include "base/macros.h"
#include "device/gamepad/gamepad_data_fetcher.h"

#if defined(__OBJC__)
@class NSArray;
#else
class NSArray;
#endif

namespace device {

class GameControllerDataFetcherMac : public GamepadDataFetcher {
 public:
  typedef GamepadDataFetcherFactoryImpl<GameControllerDataFetcherMac,
                                        GAMEPAD_SOURCE_MAC_GC>
      Factory;

  GameControllerDataFetcherMac();
  ~GameControllerDataFetcherMac() override;

  GamepadSource source() override;

  void GetGamepadData(bool devices_changed_hint) override;

 private:
  int NextUnusedPlayerIndex();

  DISALLOW_COPY_AND_ASSIGN(GameControllerDataFetcherMac);

  bool connected_[Gamepads::kItemsLengthCap];
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_GAME_CONTROLLER_DATA_FETCHER_MAC_H_
