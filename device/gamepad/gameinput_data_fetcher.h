// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_GAMEINPUT_DATA_FETCHER_H_
#define DEVICE_GAMEPAD_GAMEINPUT_DATA_FETCHER_H_

#include <GameInput.h>

#include <memory>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "device/gamepad/gameinput_gamepad_device.h"
#include "device/gamepad/gamepad_data_fetcher.h"
#include "device/gamepad/public/mojom/gamepad.mojom.h"

namespace device {

// Implements GamepadDataFetcher for the Windows platform using the GameInput
// API. Currently, Windows 19H1+ is shipped with GameInput API v0. If the
// Windows Gaming Services package is installed, the GameInput API will most
// likely be updated to the latest version available. This current data fetcher
// implementation targets the v0 API surface, which is supported by the latest
// GameInput API versions and has more advanced capabilities than the
// Windows.Gaming.Input (WGI) API - e.g. access to guide button data.
//
// More info:
// https://learn.microsoft.com/en-us/gaming/gdk/docs/reference/input/gameinput/gameinput_members?view=gdk-2510
class DEVICE_GAMEPAD_EXPORT GameInputDataFetcher final
    : public GamepadDataFetcher {
 public:
  enum class InitializationState {
    kUninitialized,
    kInitialized,
    kGetProcAddressFailed,
    kCreateGameInputFailed,
    kFailedDeviceEnumeration,
    kFailedGuideButtonCallbackRegistration,
  };

  using Factory = GamepadDataFetcherFactoryImpl<GameInputDataFetcher,
                                                GamepadSource::kWinGameInput>;

  GameInputDataFetcher();
  GameInputDataFetcher(const GameInputDataFetcher&) = delete;
  GameInputDataFetcher& operator=(const GameInputDataFetcher&) = delete;
  ~GameInputDataFetcher() override;

  // GamepadDataFetcher implementation.
  GamepadSource source() override;
  void OnAddedToProvider() override;
  void GetGamepadData(bool devices_changed_hint) override;
  void PlayEffect(int source_id,
                  mojom::GamepadHapticEffectType,
                  mojom::GamepadEffectParametersPtr,
                  mojom::GamepadHapticsManager::PlayVibrationEffectOnceCallback,
                  scoped_refptr<base::SequencedTaskRunner>) override;
  void ResetVibration(
      int source_id,
      mojom::GamepadHapticsManager::ResetVibrationActuatorCallback,
      scoped_refptr<base::SequencedTaskRunner>) override;

  InitializationState GetInitializationState() const;

  // Used for unittests overrides
  using CreateGameInputFunction =
      base::RepeatingCallback<HRESULT(IGameInput**)>;
  void OverrideGameInputCreationMethodForTesting(
      CreateGameInputFunction create_override);

  using DeviceMap = base::flat_map</*source_id=*/int,
                                   std::unique_ptr<GameInputGamepadDevice>>;
  const DeviceMap& GetGamepadsForTesting() const { return devices_; }

 private:
  void OnGamepadAdded(IGameInputDevice* device);
  void OnGamepadRemoved(IGameInputDevice* device);

  static void CALLBACK
  OnDeviceEnumerated(GameInputCallbackToken callbackToken,
                     void* context,
                     IGameInputDevice* device,
                     uint64_t timestamp,
                     GameInputDeviceStatus current_status,
                     GameInputDeviceStatus previous_status);

  void OnDeviceEnumeratedSequenced(
      Microsoft::WRL::ComPtr<IGameInputDevice> device,
      GameInputDeviceStatus current_status);

  static void CALLBACK
  OnGuideButtonChanged(GameInputCallbackToken callbackToken,
                       void* context,
                       IGameInputDevice* device,
                       uint64_t timestamp,
                       GameInputSystemButtons currentButtons,
                       GameInputSystemButtons previousButtons);

  void OnGuideButtonChangedSequenced(
      Microsoft::WRL::ComPtr<IGameInputDevice> device,
      GameInputSystemButtons current_buttons);

  int next_source_id_ = 0;
  InitializationState initialization_state_ =
      InitializationState::kUninitialized;

  DeviceMap devices_;

  using DeviceEnumeratedCallback = base::RepeatingCallback<void(
      Microsoft::WRL::ComPtr<IGameInputDevice> device,
      GameInputDeviceStatus device_status)>;

  using GuideButtonChangedCallback = base::RepeatingCallback<void(
      Microsoft::WRL::ComPtr<IGameInputDevice> device,
      GameInputSystemButtons current_buttons)>;

  GameInputCallbackToken device_callback_token_ =
      GAMEINPUT_INVALID_CALLBACK_TOKEN_VALUE;
  GameInputCallbackToken guide_button_callback_token_ =
      GAMEINPUT_INVALID_CALLBACK_TOKEN_VALUE;
  Microsoft::WRL::ComPtr<IGameInput> gameinput_;
  std::unique_ptr<DeviceEnumeratedCallback> device_enumerated_callback_;
  std::unique_ptr<GuideButtonChangedCallback> guide_button_changed_callback_;
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<GameInputDataFetcher> weak_factory_{this};
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_GAMEINPUT_DATA_FETCHER_H_
