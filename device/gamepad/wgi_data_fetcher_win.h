// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_WGI_DATA_FETCHER_WIN_H_
#define DEVICE_GAMEPAD_WGI_DATA_FETCHER_WIN_H_

#include <Windows.Gaming.Input.h>
#include <wrl/event.h>

#include <memory>
#include <optional>
#include <string>

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "device/gamepad/gamepad_data_fetcher.h"
#include "device/gamepad/gamepad_id_list.h"
#include "device/gamepad/public/mojom/gamepad.mojom.h"
#include "device/gamepad/wgi_gamepad_device.h"
#include "device/gamepad/xinput_data_fetcher_win.h"

namespace device {

class DEVICE_GAMEPAD_EXPORT WgiDataFetcherWin final
    : public GamepadDataFetcher {
 public:
  enum class InitializationState {
    kUninitialized,
    kInitialized,
    kAddGamepadAddedFailed,
    kAddGamepadRemovedFailed,
    kRoGetActivationFactoryFailed,
  };

  using Factory =
      GamepadDataFetcherFactoryImpl<WgiDataFetcherWin, GamepadSource::kWinWgi>;

  // Define test hooks to use a fake WinRT RoGetActivationFactory
  // implementation to avoid dependencies on the OS for WGI testing.
  using GetActivationFactoryFunction = HRESULT (*)(HSTRING class_id,
                                                   const IID& iid,
                                                   void** out_factory);

  WgiDataFetcherWin();
  WgiDataFetcherWin(const WgiDataFetcherWin&) = delete;
  WgiDataFetcherWin& operator=(const WgiDataFetcherWin&) = delete;
  ~WgiDataFetcherWin() override;

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

  // Set fake ActivationFunction for test to avoid dependencies on the OS API.
  using ActivationFactoryFunctionCallback =
      base::RepeatingCallback<GetActivationFactoryFunction()>;
  static void OverrideActivationFactoryFunctionForTesting(
      ActivationFactoryFunctionCallback callback);

  // Used to store gamepad devices indexed by its source id.
  using DeviceMap = base::flat_map<int, std::unique_ptr<WgiGamepadDevice>>;
  const DeviceMap& GetGamepadsForTesting() const { return devices_; }

  InitializationState GetInitializationState() const;

 private:
  // Set the state of the new connected gamepad to initialized, update
  // gamepad state connection status, and add a new controller mapping for
  // `gamepad` to `gamepads_` on gamepad polling thread.
  void OnGamepadAdded(IInspectable* /* sender */,
                      ABI::Windows::Gaming::Input::IGamepad* gamepad);

  // Remove the corresponding controller mapping of `gamepad` in `gamepads_`
  // on gamepad polling thread.
  void OnGamepadRemoved(IInspectable* /* sender */,
                        ABI::Windows::Gaming::Input::IGamepad* gamepad);

  // WgiDataFetcherWin has its own instance of XInputDataFetcherWin to query for
  // the meta button state.
  std::unique_ptr<XInputDataFetcherWin> xinput_data_fetcher_;

  static ActivationFactoryFunctionCallback&
  GetActivationFactoryFunctionCallback();

  std::u16string GetGamepadDisplayName(
      ABI::Windows::Gaming::Input::IGamepad* gamepad);

  std::u16string BuildGamepadIdString(
      GamepadId gamepad_id,
      const std::u16string& display_name,
      ABI::Windows::Gaming::Input::IGamepad* gamepad);

  Microsoft::WRL::ComPtr<ABI::Windows::Gaming::Input::IRawGameController>
  GetRawGameController(ABI::Windows::Gaming::Input::IGamepad* gamepad);

  void UnregisterEventHandlers();

  int next_source_id_ = 0;
  InitializationState initialization_state_ =
      InitializationState::kUninitialized;

  DeviceMap devices_;

  Microsoft::WRL::ComPtr<ABI::Windows::Gaming::Input::IGamepadStatics>
      gamepad_statics_;

  GetActivationFactoryFunction get_activation_factory_function_;

  std::optional<EventRegistrationToken> added_event_token_;
  std::optional<EventRegistrationToken> removed_event_token_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<WgiDataFetcherWin> weak_factory_{this};
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_WGI_DATA_FETCHER_WIN_H_
