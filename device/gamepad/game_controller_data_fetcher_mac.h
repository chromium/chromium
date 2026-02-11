// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_GAME_CONTROLLER_DATA_FETCHER_MAC_H_
#define DEVICE_GAMEPAD_GAME_CONTROLLER_DATA_FETCHER_MAC_H_

#include <stddef.h>

#include <memory>

#include "base/containers/flat_map.h"
#include "device/gamepad/gamepad_data_fetcher.h"
#include "device/gamepad/public/cpp/gamepad.h"

namespace device {

class GameControllerDataFetcherMac : public GamepadDataFetcher {
 public:
  using Factory = GamepadDataFetcherFactoryImpl<GameControllerDataFetcherMac,
                                                GamepadSource::kMacGc>;

  GameControllerDataFetcherMac();
  GameControllerDataFetcherMac(const GameControllerDataFetcherMac&) = delete;
  GameControllerDataFetcherMac& operator=(const GameControllerDataFetcherMac&) =
      delete;
  ~GameControllerDataFetcherMac() override;

  // GamepadDataFetcher implementation.
  void OnAddedToProvider() override;
  GamepadSource source() override;
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

  struct GameControllerDataFetcherMacImpl;

 private:
  int NextUnusedPlayerIndex();

  std::unique_ptr<GameControllerDataFetcherMacImpl> impl_;

  int next_source_id_ = 0;
  bool connected_[Gamepads::kItemsLengthCap] = {};

  scoped_refptr<base::SingleThreadTaskRunner> polling_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  base::WeakPtrFactory<GameControllerDataFetcherMac> weak_factory_{this};
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_GAME_CONTROLLER_DATA_FETCHER_MAC_H_
