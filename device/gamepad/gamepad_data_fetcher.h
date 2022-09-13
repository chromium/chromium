// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_GAMEPAD_DATA_FETCHER_H_
#define DEVICE_GAMEPAD_GAMEPAD_DATA_FETCHER_H_

#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "device/gamepad/gamepad_data_fetcher_manager.h"
#include "device/gamepad/gamepad_export.h"
#include "device/gamepad/gamepad_pad_state_provider.h"
#include "device/gamepad/public/cpp/gamepad.h"
#include "device/gamepad/public/mojom/gamepad.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/device/public/mojom/hid.mojom.h"

namespace device {

// Abstract interface for implementing platform- (and test-) specific behavior
// for getting the gamepad data.
class DEVICE_GAMEPAD_EXPORT GamepadDataFetcher {
 public:
  GamepadDataFetcher();
  virtual ~GamepadDataFetcher();
  virtual void GetGamepadData(bool devices_changed_hint) = 0;
  virtual void PauseHint(bool paused) {}
  virtual void PlayEffect(
      int source_id,
      mojom::GamepadHapticEffectType,
      mojom::GamepadEffectParametersPtr,
      mojom::GamepadHapticsManager::PlayVibrationEffectOnceCallback,
      scoped_refptr<base::SequencedTaskRunner>);
  virtual void ResetVibration(
      int source_id,
      mojom::GamepadHapticsManager::ResetVibrationActuatorCallback,
      scoped_refptr<base::SequencedTaskRunner>);

  virtual GamepadSource source() = 0;

  // Shuts down the gamepad with given |source_id| and removes it from the data
  // fetchers list of devices. Default implementation used on data fetchers for
  // recognized gamepads because it should never be called on those gamepads.
  // Returns a boolean that is true if the gamepad was successfully
  // disconnected.
  virtual bool DisconnectUnrecognizedGamepad(int source_id);

  GamepadPadStateProvider* provider() { return provider_; }

  PadState* GetPadState(int source_id, bool new_pad_recognized = true) {
    if (!provider_)
      return nullptr;

    return provider_->GetPadState(source(), source_id, new_pad_recognized);
  }

  // Returns the current time value in microseconds. Data fetchers should use
  // the value returned by this method to update the |timestamp| gamepad member.
  static int64_t CurrentTimeInMicroseconds();

  // Converts a TimeTicks value to a timestamp in microseconds, as used for
  // the |timestamp| gamepad member.
  static int64_t TimeInMicroseconds(base::TimeTicks update_time);

  // Perform one-time string initialization on the gamepad state in |pad|.
  static void UpdateGamepadStrings(const std::string& product_name,
                                   uint16_t vendor_id,
                                   uint16_t product_id,
                                   bool has_standard_mapping,
                                   Gamepad& pad);

  // Call a vibration callback on the same sequence that the vibration command
  // was issued on.
  static void RunVibrationCallback(
      base::OnceCallback<void(mojom::GamepadHapticsResult)> callback,
      scoped_refptr<base::SequencedTaskRunner> callback_runner,
      mojom::GamepadHapticsResult result);

  // Sets a global callback for GamepadProviders to use when binding a
  // HidManager interface.
  using HidManagerBinder =
      base::RepeatingCallback<void(mojo::PendingReceiver<mojom::HidManager>)>;
  static void SetHidManagerBinder(HidManagerBinder binder);

 protected:
  friend GamepadPadStateProvider;

  // To be called by the GamepadPadStateProvider on the polling thread.
  void InitializeProvider(GamepadPadStateProvider* provider);

  // Binds a HidManager interface.
  void BindHidManager(mojo::PendingReceiver<mojom::HidManager> receiver);

  // This call will happen on the gamepad polling thread. Any initialization
  // that needs to happen on that thread should be done here, not in the
  // constructor.
  virtual void OnAddedToProvider() {}

 private:
  // GamepadPadStateProvider is the base class of GamepadProvider, which owns
  // this data fetcher.
  raw_ptr<GamepadPadStateProvider> provider_ = nullptr;
};

// Factory class for creating a GamepadDataFetcher. Used by the
// GamepadDataFetcherManager.
class DEVICE_GAMEPAD_EXPORT GamepadDataFetcherFactory {
 public:
  GamepadDataFetcherFactory();
  virtual ~GamepadDataFetcherFactory() {}
  virtual std::unique_ptr<GamepadDataFetcher> CreateDataFetcher() = 0;
  virtual GamepadSource source() = 0;
};

// Basic factory implementation for GamepadDataFetchers without a complex
// constructor.
template <typename DataFetcherType, GamepadSource DataFetcherSource>
class GamepadDataFetcherFactoryImpl : public GamepadDataFetcherFactory {
 public:
  ~GamepadDataFetcherFactoryImpl() override {}
  std::unique_ptr<GamepadDataFetcher> CreateDataFetcher() override {
    return std::unique_ptr<GamepadDataFetcher>(new DataFetcherType());
  }
  GamepadSource source() override { return DataFetcherSource; }
  static GamepadSource static_source() { return DataFetcherSource; }
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_GAMEPAD_DATA_FETCHER_H_
