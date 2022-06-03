// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_GAME_CONTROLLER_DATA_FETCHER_MAC_H_
#define DEVICE_GAMEPAD_GAME_CONTROLLER_DATA_FETCHER_MAC_H_

#include "device/gamepad/gamepad_data_fetcher.h"

namespace device {

class GameControllerDataFetcherMac : public GamepadDataFetcher {
 public:
  typedef GamepadDataFetcherFactoryImpl<GameControllerDataFetcherMac,
                                        GAMEPAD_SOURCE_MAC_GC>
      Factory;

  GameControllerDataFetcherMac();
  GameControllerDataFetcherMac(const GameControllerDataFetcherMac&) = delete;
  GameControllerDataFetcherMac& operator=(const GameControllerDataFetcherMac&) =
      delete;
  ~GameControllerDataFetcherMac() override;

  // GamepadDataFetcher implementation.
  GamepadSource source() override;
  void GetGamepadData(bool devices_changed_hint) override;

 private:
  int NextUnusedPlayerIndex();

  bool connected_[Gamepads::kItemsLengthCap];
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_GAME_CONTROLLER_DATA_FETCHER_MAC_H_
