// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_GAMEPAD_PROVIDER_H_
#define DEVICE_GAMEPAD_GAMEPAD_PROVIDER_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/system/system_monitor.h"
#include "base/time/time.h"
#include "device/gamepad/gamepad_export.h"
#include "device/gamepad/gamepad_pad_state_provider.h"
#include "device/gamepad/gamepad_shared_buffer.h"
#include "device/gamepad/public/cpp/gamepads.h"
#include "device/gamepad/public/mojom/gamepad.mojom.h"
#include "mojo/public/cpp/system/buffer.h"

namespace base {
class SingleThreadTaskRunner;
class Thread;
}  // namespace base

namespace service_manager {
class Connector;
}  // namespace service_manager

namespace device {

class GamepadDataFetcher;

class DEVICE_GAMEPAD_EXPORT GamepadConnectionChangeClient {
 public:
  virtual void OnGamepadConnectionChange(bool connected,
                                         uint32_t index,
                                         const Gamepad& pad) = 0;
};

class DEVICE_GAMEPAD_EXPORT GamepadProvider
    : public GamepadPadStateProvider,
      public base::SystemMonitor::DevicesChangedObserver {
 public:
  GamepadProvider(
      GamepadConnectionChangeClient* connection_change_client,
      std::unique_ptr<service_manager::Connector> service_manager_connector);

  // Manually specifies the data fetcher and polling thread. The polling thread
  // will be created normally if |polling_thread| is nullptr. Used for testing.
  GamepadProvider(
      GamepadConnectionChangeClient* connection_change_client,
      std::unique_ptr<service_manager::Connector> service_manager_connector,
      std::unique_ptr<GamepadDataFetcher> fetcher,
      std::unique_ptr<base::Thread> polling_thread);

  ~GamepadProvider() override;

  // Returns a duplicate of the shared memory region of the gamepad data.
  base::ReadOnlySharedMemoryRegion DuplicateSharedMemoryRegion();

  void GetCurrentGamepadData(Gamepads* data);

  void PlayVibrationEffectOnce(
      uint32_t pad_index,
      mojom::GamepadHapticEffectType,
      mojom::GamepadEffectParametersPtr,
      mojom::GamepadHapticsManager::PlayVibrationEffectOnceCallback);

  void ResetVibrationActuator(
      uint32_t pad_index,
      mojom::GamepadHapticsManager::ResetVibrationActuatorCallback);

  // Pause and resume the background polling thread. Can be called from any
  // thread.
  void Pause();
  void Resume();

  // Registers the given closure for calling when the user has interacted with
  // the device. This callback will only be issued once.
  void RegisterForUserGesture(base::OnceClosure closure);

  // base::SystemMonitor::DevicesChangedObserver implementation.
  void OnDevicesChanged(base::SystemMonitor::DeviceType type) override;

  // Add a gamepad data fetcher. Takes ownership of |fetcher|.
  void AddGamepadDataFetcher(std::unique_ptr<GamepadDataFetcher> fetcher);

  // Remove gamepad data fetchers with the given source.
  void RemoveSourceGamepadDataFetcher(GamepadSource source);

  void SetSanitizationEnabled(bool sanitize) { sanitize_ = sanitize; }

 private:
  void Initialize(std::unique_ptr<GamepadDataFetcher> fetcher);

  // Method for setting up the platform-specific data fetcher. Takes ownership
  // of |fetcher|.
  void DoAddGamepadDataFetcher(std::unique_ptr<GamepadDataFetcher> fetcher);
  void DoRemoveSourceGamepadDataFetcher(GamepadSource source);

  GamepadDataFetcher* GetSourceGamepadDataFetcher(GamepadSource source);

  // Method for sending pause hints to the low-level data fetcher. Runs on
  // polling_thread_.
  void SendPauseHint(bool paused);

  // Method for polling a GamepadDataFetcher. Runs on the polling_thread_.
  void DoPoll();
  void ScheduleDoPoll();

  void OnGamepadConnectionChange(bool connected,
                                 uint32_t index,
                                 const Gamepad& pad);

  // Checks the gamepad state to see if the user has interacted with it. Returns
  // true if any user gesture observers were notified.
  bool CheckForUserGesture();

  // GamepadPadStateProvider implementation.
  void DisconnectUnrecognizedGamepad(GamepadSource source,
                                     int source_id) override;

  void PlayEffectOnPollingThread(
      uint32_t pad_index,
      mojom::GamepadHapticEffectType,
      mojom::GamepadEffectParametersPtr,
      mojom::GamepadHapticsManager::PlayVibrationEffectOnceCallback,
      scoped_refptr<base::SequencedTaskRunner>);

  void ResetVibrationOnPollingThread(
      uint32_t pad_index,
      mojom::GamepadHapticsManager::PlayVibrationEffectOnceCallback,
      scoped_refptr<base::SequencedTaskRunner>);

  // The duration of the delay between iterations of DoPoll.
  base::TimeDelta sampling_interval_delta_;

  // Keeps track of when the background thread is paused. Access to is_paused_
  // must be guarded by is_paused_lock_.
  base::Lock is_paused_lock_;
  bool is_paused_ = true;

  // Keep track of when a polling task is schedlued, so as to prevent us from
  // accidentally scheduling more than one at any time, when rapidly toggling
  // |is_paused_|.
  bool have_scheduled_do_poll_ = false;

  // Lists all observers registered for user gestures, and the thread which
  // to issue the callbacks on. Since we always issue the callback on the
  // thread which the registration happened, and this class lives on the I/O
  // thread, the message loop proxies will normally just be the I/O thread.
  // However, this will be the main thread for unit testing.
  base::Lock user_gesture_lock_;
  using ClosureAndThread =
      std::pair<base::OnceClosure, scoped_refptr<base::SingleThreadTaskRunner>>;
  using UserGestureObserverVector = std::vector<ClosureAndThread>;
  UserGestureObserverVector user_gesture_observers_;

  // Updated based on notification from SystemMonitor when the system devices
  // have been updated, and this notification is passed on to the data fetcher
  // to enable it to avoid redundant (and possibly expensive) is-connected
  // tests. Access to devices_changed_ must be guarded by
  // devices_changed_lock_.
  base::Lock devices_changed_lock_;
  bool devices_changed_ = true;

  bool ever_had_user_gesture_ = false;
  bool sanitize_ = true;

  // Only used on the polling thread.
  using GamepadFetcherVector = std::vector<std::unique_ptr<GamepadDataFetcher>>;
  GamepadFetcherVector data_fetchers_;

  base::Lock shared_memory_lock_;
  std::unique_ptr<GamepadSharedBuffer> gamepad_shared_buffer_;

  // Polling is done on this background thread.
  std::unique_ptr<base::Thread> polling_thread_;

  GamepadConnectionChangeClient* connection_change_client_;

  // Service manager connector, to allow data fetchers to access the device
  // service from the polling thread.
  std::unique_ptr<service_manager::Connector> service_manager_connector_;

  DISALLOW_COPY_AND_ASSIGN(GamepadProvider);
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_GAMEPAD_PROVIDER_H_
