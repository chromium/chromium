// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_GAME_CONTROLLER_GAMEPAD_H_
#define DEVICE_GAMEPAD_GAME_CONTROLLER_GAMEPAD_H_

#import <CoreHaptics/CoreHaptics.h>
#import <GameController/GameController.h>

#include "base/memory/weak_ptr.h"
#include "device/gamepad/abstract_haptic_gamepad.h"

namespace device {

class GameControllerGamepad : public AbstractHapticGamepad {
 public:
  explicit GameControllerGamepad(GCController* controller);
  ~GameControllerGamepad() override;

  void UpdateState(Gamepad& pad);
  void StartHaptics();
  void InitializeStaticData(Gamepad& pad);

  // AbstractHapticGamepad implementation
  void SetVibration(mojom::GamepadEffectParametersPtr params) override;
  void DoShutdown() override;
  base::WeakPtr<AbstractHapticGamepad> GetWeakPtr() override;

 private:
  __strong GCController* controller_;
  bool haptics_started_ = false;
  __strong CHHapticEngine* left_haptic_engine_;
  __strong CHHapticEngine* right_haptic_engine_;
  __strong CHHapticEngine* left_trigger_haptic_engine_;
  __strong CHHapticEngine* right_trigger_haptic_engine_;
  __strong CHHapticEngine* default_haptic_engine_;
  __strong id<CHHapticPatternPlayer> left_haptic_player_;
  __strong id<CHHapticPatternPlayer> right_haptic_player_;
  __strong id<CHHapticPatternPlayer> left_trigger_haptic_player_;
  __strong id<CHHapticPatternPlayer> right_trigger_haptic_player_;
  __strong id<CHHapticPatternPlayer> default_haptic_player_;

  base::WeakPtrFactory<GameControllerGamepad> weak_factory_{this};
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_GAME_CONTROLLER_GAMEPAD_H_
