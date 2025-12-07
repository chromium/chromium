// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_SIMULATED_GAMEPAD_DATA_FETCHER_H_
#define DEVICE_GAMEPAD_SIMULATED_GAMEPAD_DATA_FETCHER_H_

#include <map>
#include <memory>
#include <optional>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/unguessable_token.h"
#include "device/gamepad/abstract_haptic_gamepad.h"
#include "device/gamepad/gamepad_data_fetcher.h"
#include "device/gamepad/public/cpp/gamepad.h"
#include "device/gamepad/public/mojom/gamepad.mojom-forward.h"
#include "device/gamepad/simulated_gamepad_inputs.h"
#include "device/gamepad/simulated_gamepad_params.h"

namespace device {

// Manages and provides access to simulated gamepads.
class DEVICE_GAMEPAD_EXPORT SimulatedGamepadDataFetcher
    : public GamepadDataFetcher {
 public:
  using Factory = GamepadDataFetcherFactoryImpl<SimulatedGamepadDataFetcher,
                                                GamepadSource::kSimulated>;

  SimulatedGamepadDataFetcher();

  SimulatedGamepadDataFetcher(const SimulatedGamepadDataFetcher&) = delete;
  SimulatedGamepadDataFetcher& operator=(const SimulatedGamepadDataFetcher&) =
      delete;

  ~SimulatedGamepadDataFetcher() override;

  // GamepadDataFetcher implementation.
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

  // Create a simulated gamepad described by `params` and identified by `token`.
  void AddSimulatedGamepad(base::UnguessableToken token,
                           SimulatedGamepadParams params);

  // Remove the simulated gamepad identified by `token`.
  void RemoveSimulatedGamepad(base::UnguessableToken token);

  void SimulateInputFrame(base::UnguessableToken token,
                          SimulatedGamepadInputs inputs);

  void SetOnPollForTesting(base::RepeatingClosure on_poll);

 private:
  class SimulatedGamepad : public AbstractHapticGamepad {
   public:
    SimulatedGamepad(SimulatedGamepadParams params, uint32_t source_id);
    SimulatedGamepad(const SimulatedGamepad&) = delete;
    SimulatedGamepad& operator=(const SimulatedGamepad&) = delete;
    ~SimulatedGamepad() override;

    // AbstractHapticGamepad implementation.
    void SetVibration(mojom::GamepadEffectParametersPtr params) override;
    base::WeakPtr<AbstractHapticGamepad> GetWeakPtr() override;

    // Update `pad` with the most recent input state.
    void UpdateGamepadState(Gamepad& pad);

    // Immutable gamepad properties needed when initializing the gamepad.
    const SimulatedGamepadParams params_;

    // The source ID assigned to this gamepad.
    const uint32_t source_id_;

    // The length of the axes and buttons arrays.
    const size_t axis_count_;
    const size_t button_count_;

    // A mapping of axis index to axis values for axes with updated values.
    std::map<uint32_t, double> pending_axis_inputs_;

    // A mapping of button index to button values for buttons with updated
    // values.
    std::map<uint32_t, SimulatedGamepadButton> pending_button_inputs_;

    // The current list of active touch points.
    std::vector<SimulatedGamepadTouch> active_touches_;

    base::WeakPtrFactory<SimulatedGamepad> weak_factory_{this};
  };

  std::map<base::UnguessableToken, SimulatedGamepad> gamepads_;
  uint32_t next_source_id_ = 0;

  // For testing. Called after updating gamepad state in GetGamepadData.
  base::RepeatingClosure on_poll_;
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_SIMULATED_GAMEPAD_DATA_FETCHER_H_
