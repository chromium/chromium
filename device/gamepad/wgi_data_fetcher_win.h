// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_WGI_DATA_FETCHER_WIN_H_
#define DEVICE_GAMEPAD_WGI_DATA_FETCHER_WIN_H_

#include "device/gamepad/gamepad_data_fetcher.h"

#include <Windows.Gaming.Input.h>
#include <wrl/event.h>

#include "base/sequence_checker.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace device {

class DEVICE_GAMEPAD_EXPORT WgiDataFetcherWin final
    : public GamepadDataFetcher {
 public:
  struct WindowsGamingInputControllerMapping {
   public:
    WindowsGamingInputControllerMapping(
        int input_source_id,
        Microsoft::WRL::ComPtr<ABI::Windows::Gaming::Input::IGamepad>
            input_gamepad);

    WindowsGamingInputControllerMapping(
        const WindowsGamingInputControllerMapping& other);

    WindowsGamingInputControllerMapping(
        WindowsGamingInputControllerMapping&& other);

    WindowsGamingInputControllerMapping& operator=(
        const WindowsGamingInputControllerMapping& other);

    WindowsGamingInputControllerMapping& operator=(
        WindowsGamingInputControllerMapping&&);

    ~WindowsGamingInputControllerMapping();

    int source_id;
    Microsoft::WRL::ComPtr<ABI::Windows::Gaming::Input::IGamepad> gamepad;
  };

  enum class InitializationState {
    kUninitialized,
    kInitialized,
    kAddGamepadAddedFailed,
    kAddGamepadRemovedFailed,
    kRoGetActivationFactoryFailed,
    kCoreWinrtStringDelayLoadFailed,
  };

  using Factory =
      GamepadDataFetcherFactoryImpl<WgiDataFetcherWin, GAMEPAD_SOURCE_WIN_WGI>;

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

  // Set fake ActivationFunction for test to avoid dependencies on the OS API.
  void SetGetActivationFunctionForTesting(GetActivationFactoryFunction value);

  const std::vector<WindowsGamingInputControllerMapping>&
  GetGamepadsForTesting() const;

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

  void UnregisterEventHandlers();

  int next_source_id_ = 0;
  InitializationState initialization_state_ =
      InitializationState::kUninitialized;

  std::vector<WindowsGamingInputControllerMapping> gamepads_;

  Microsoft::WRL::ComPtr<ABI::Windows::Gaming::Input::IGamepadStatics>
      gamepad_statics_;

  GetActivationFactoryFunction get_activation_factory_function_;

  absl::optional<EventRegistrationToken> added_event_token_;
  absl::optional<EventRegistrationToken> removed_event_token_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<WgiDataFetcherWin> weak_factory_{this};
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_WGI_DATA_FETCHER_WIN_H_
