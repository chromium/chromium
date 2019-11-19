// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/gamepad_provider.h"

#include <stddef.h>
#include <string.h>
#include <cmath>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_pump_type.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/third_party/dynamic_annotations/dynamic_annotations.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "device/gamepad/gamepad_data_fetcher.h"
#include "device/gamepad/gamepad_data_fetcher_manager.h"
#include "device/gamepad/gamepad_user_gesture.h"
#include "device/gamepad/public/cpp/gamepad_features.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "services/service_manager/public/cpp/connector.h"

namespace device {

GamepadProvider::GamepadProvider(
    GamepadConnectionChangeClient* connection_change_client,
    std::unique_ptr<service_manager::Connector> service_manager_connector)
    : gamepad_shared_buffer_(std::make_unique<GamepadSharedBuffer>()),
      connection_change_client_(connection_change_client),
      service_manager_connector_(std::move(service_manager_connector)) {
  Initialize(std::unique_ptr<GamepadDataFetcher>());
}

GamepadProvider::GamepadProvider(
    GamepadConnectionChangeClient* connection_change_client,
    std::unique_ptr<service_manager::Connector> service_manager_connector,
    std::unique_ptr<GamepadDataFetcher> fetcher,
    std::unique_ptr<base::Thread> polling_thread)
    : gamepad_shared_buffer_(std::make_unique<GamepadSharedBuffer>()),
      polling_thread_(std::move(polling_thread)),
      connection_change_client_(connection_change_client),
      service_manager_connector_(std::move(service_manager_connector)) {
  Initialize(std::move(fetcher));
}

GamepadProvider::~GamepadProvider() {
  GamepadDataFetcherManager::GetInstance()->ClearProvider();

  base::SystemMonitor* monitor = base::SystemMonitor::Get();
  if (monitor)
    monitor->RemoveDevicesChangedObserver(this);

  // Delete GamepadDataFetchers on |polling_thread_|. This is important because
  // some of them require their destructor to be called on the same sequence as
  // their other methods.
  polling_thread_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&GamepadFetcherVector::clear,
                                base::Unretained(&data_fetchers_)));

  // The service manager connector is bound to the polling thread and must be
  // destroyed on that thread.
  polling_thread_->task_runner()->DeleteSoon(
      FROM_HERE, std::move(service_manager_connector_));

  // Use Stop() to join the polling thread, as there may be pending callbacks
  // which dereference |polling_thread_|.
  polling_thread_->Stop();

  DCHECK(data_fetchers_.empty());
}

base::ReadOnlySharedMemoryRegion
GamepadProvider::DuplicateSharedMemoryRegion() {
  return gamepad_shared_buffer_->DuplicateSharedMemoryRegion();
}

void GamepadProvider::GetCurrentGamepadData(Gamepads* data) {
  const Gamepads* pads = gamepad_shared_buffer_->buffer();
  base::AutoLock lock(shared_memory_lock_);
  *data = *pads;
}

void GamepadProvider::PlayVibrationEffectOnce(
    uint32_t pad_index,
    mojom::GamepadHapticEffectType type,
    mojom::GamepadEffectParametersPtr params,
    mojom::GamepadHapticsManager::PlayVibrationEffectOnceCallback callback) {
  polling_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&GamepadProvider::PlayEffectOnPollingThread,
                     Unretained(this), pad_index, type, std::move(params),
                     std::move(callback), base::ThreadTaskRunnerHandle::Get()));
}

void GamepadProvider::ResetVibrationActuator(
    uint32_t pad_index,
    mojom::GamepadHapticsManager::ResetVibrationActuatorCallback callback) {
  polling_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&GamepadProvider::ResetVibrationOnPollingThread,
                     Unretained(this), pad_index, std::move(callback),
                     base::ThreadTaskRunnerHandle::Get()));
}

void GamepadProvider::Pause() {
  {
    base::AutoLock lock(is_paused_lock_);
    is_paused_ = true;
  }
  polling_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&GamepadProvider::SendPauseHint, Unretained(this), true));
}

void GamepadProvider::Resume() {
  {
    base::AutoLock lock(is_paused_lock_);
    if (!is_paused_)
      return;
    is_paused_ = false;
  }

  polling_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&GamepadProvider::SendPauseHint, Unretained(this), false));
  polling_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&GamepadProvider::ScheduleDoPoll, Unretained(this)));
}

void GamepadProvider::RegisterForUserGesture(base::OnceClosure closure) {
  base::AutoLock lock(user_gesture_lock_);
  user_gesture_observers_.emplace_back(std::move(closure),
                                       base::ThreadTaskRunnerHandle::Get());
}

void GamepadProvider::OnDevicesChanged(base::SystemMonitor::DeviceType type) {
  base::AutoLock lock(devices_changed_lock_);
  devices_changed_ = true;
}

void GamepadProvider::Initialize(std::unique_ptr<GamepadDataFetcher> fetcher) {
  sampling_interval_delta_ =
      base::TimeDelta::FromMilliseconds(features::GetGamepadPollingInterval());

  base::SystemMonitor* monitor = base::SystemMonitor::Get();
  if (monitor)
    monitor->AddDevicesChangedObserver(this);

  if (!polling_thread_)
    polling_thread_.reset(new base::Thread("Gamepad polling thread"));
#if defined(OS_LINUX)
  // On Linux, the data fetcher needs to watch file descriptors, so the message
  // loop needs to be a libevent loop.
  const base::MessagePumpType kMessageLoopType = base::MessagePumpType::IO;
#elif defined(OS_ANDROID)
  // On Android, keeping a message loop of default type.
  const base::MessagePumpType kMessageLoopType = base::MessagePumpType::DEFAULT;
#else
  // On Mac, the data fetcher uses IOKit which depends on CFRunLoop, so the
  // message loop needs to be a UI-type loop. On Windows it must be a UI loop
  // to properly pump the MessageWindow that captures device state.
  const base::MessagePumpType kMessageLoopType = base::MessagePumpType::UI;
#endif
  polling_thread_->StartWithOptions(base::Thread::Options(kMessageLoopType, 0));

  if (fetcher) {
    AddGamepadDataFetcher(std::move(fetcher));
  } else {
    GamepadDataFetcherManager::GetInstance()->InitializeProvider(this);
  }
}

void GamepadProvider::AddGamepadDataFetcher(
    std::unique_ptr<GamepadDataFetcher> fetcher) {
  polling_thread_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&GamepadProvider::DoAddGamepadDataFetcher,
                                base::Unretained(this), std::move(fetcher)));
}

void GamepadProvider::RemoveSourceGamepadDataFetcher(GamepadSource source) {
  polling_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&GamepadProvider::DoRemoveSourceGamepadDataFetcher,
                     base::Unretained(this), source));
}

void GamepadProvider::PlayEffectOnPollingThread(
    uint32_t pad_index,
    mojom::GamepadHapticEffectType type,
    mojom::GamepadEffectParametersPtr params,
    mojom::GamepadHapticsManager::PlayVibrationEffectOnceCallback callback,
    scoped_refptr<base::SequencedTaskRunner> callback_runner) {
  DCHECK(polling_thread_->task_runner()->BelongsToCurrentThread());
  PadState* pad_state = GetConnectedPadState(pad_index);
  if (!pad_state) {
    GamepadDataFetcher::RunVibrationCallback(
        std::move(callback), std::move(callback_runner),
        mojom::GamepadHapticsResult::GamepadHapticsResultError);
    return;
  }

  GamepadDataFetcher* fetcher = GetSourceGamepadDataFetcher(pad_state->source);
  if (!fetcher) {
    GamepadDataFetcher::RunVibrationCallback(
        std::move(callback), std::move(callback_runner),
        mojom::GamepadHapticsResult::GamepadHapticsResultNotSupported);
    return;
  }

  fetcher->PlayEffect(pad_state->source_id, type, std::move(params),
                      std::move(callback), std::move(callback_runner));
}

void GamepadProvider::ResetVibrationOnPollingThread(
    uint32_t pad_index,
    mojom::GamepadHapticsManager::PlayVibrationEffectOnceCallback callback,
    scoped_refptr<base::SequencedTaskRunner> callback_runner) {
  DCHECK(polling_thread_->task_runner()->BelongsToCurrentThread());
  PadState* pad_state = GetConnectedPadState(pad_index);
  if (!pad_state) {
    GamepadDataFetcher::RunVibrationCallback(
        std::move(callback), std::move(callback_runner),
        mojom::GamepadHapticsResult::GamepadHapticsResultError);
    return;
  }

  GamepadDataFetcher* fetcher = GetSourceGamepadDataFetcher(pad_state->source);
  if (!fetcher) {
    GamepadDataFetcher::RunVibrationCallback(
        std::move(callback), std::move(callback_runner),
        mojom::GamepadHapticsResult::GamepadHapticsResultNotSupported);
    return;
  }

  fetcher->ResetVibration(pad_state->source_id, std::move(callback),
                          std::move(callback_runner));
}

GamepadDataFetcher* GamepadProvider::GetSourceGamepadDataFetcher(
    GamepadSource source) {
  for (auto it = data_fetchers_.begin(); it != data_fetchers_.end();) {
    if ((*it)->source() == source) {
      return it->get();
    } else {
      ++it;
    }
  }
  return nullptr;
}

void GamepadProvider::DoAddGamepadDataFetcher(
    std::unique_ptr<GamepadDataFetcher> fetcher) {
  DCHECK(polling_thread_->task_runner()->BelongsToCurrentThread());

  if (!fetcher)
    return;

  InitializeDataFetcher(fetcher.get(), service_manager_connector_.get());
  data_fetchers_.push_back(std::move(fetcher));
}

void GamepadProvider::DoRemoveSourceGamepadDataFetcher(GamepadSource source) {
  DCHECK(polling_thread_->task_runner()->BelongsToCurrentThread());

  for (auto it = data_fetchers_.begin(); it != data_fetchers_.end();) {
    if ((*it)->source() == source) {
      it = data_fetchers_.erase(it);
    } else {
      ++it;
    }
  }
}

void GamepadProvider::SendPauseHint(bool paused) {
  DCHECK(polling_thread_->task_runner()->BelongsToCurrentThread());
  for (const auto& it : data_fetchers_) {
    it->PauseHint(paused);
  }
}

void GamepadProvider::DoPoll() {
  DCHECK(polling_thread_->task_runner()->BelongsToCurrentThread());
  DCHECK(have_scheduled_do_poll_);
  have_scheduled_do_poll_ = false;

  bool changed;

  ANNOTATE_BENIGN_RACE_SIZED(gamepad_shared_buffer_->buffer(), sizeof(Gamepads),
                             "Racey reads are discarded");

  {
    base::AutoLock lock(devices_changed_lock_);
    changed = devices_changed_;
    devices_changed_ = false;
  }

  for (size_t i = 0; i < Gamepads::kItemsLengthCap; ++i)
    pad_states_.get()[i].is_active = false;

  // Loop through each registered data fetcher and poll its gamepad data.
  // It's expected that GetGamepadData will mark each gamepad as active (via
  // GetPadState). If a gamepad is not marked as active during the calls to
  // GetGamepadData then it's assumed to be disconnected.
  for (const auto& it : data_fetchers_) {
    it->GetGamepadData(changed);
  }

  Gamepads* buffer = gamepad_shared_buffer_->buffer();

  // Send out disconnect events using the last polled data before we wipe it out
  // in the mapping step.
  if (ever_had_user_gesture_) {
    for (size_t i = 0; i < Gamepads::kItemsLengthCap; ++i) {
      PadState& state = pad_states_.get()[i];

      if (!state.is_newly_active && !state.is_active &&
          state.source != GAMEPAD_SOURCE_NONE) {
        auto pad = buffer->items[i];
        pad.connected = false;
        OnGamepadConnectionChange(false, i, pad);
        ClearPadState(state);
      }
    }
  }

  {
    base::AutoLock lock(shared_memory_lock_);

    // Acquire the SeqLock. There is only ever one writer to this data.
    // See gamepad_shared_buffer.h.
    gamepad_shared_buffer_->WriteBegin();
    for (size_t i = 0; i < Gamepads::kItemsLengthCap; ++i) {
      PadState& state = pad_states_.get()[i];
      // Must run through the map+sanitize here or CheckForUserGesture may fail.
      MapAndSanitizeGamepadData(&state, &buffer->items[i], sanitize_);
    }
    gamepad_shared_buffer_->WriteEnd();
  }

  if (ever_had_user_gesture_) {
    for (size_t i = 0; i < Gamepads::kItemsLengthCap; ++i) {
      PadState& state = pad_states_.get()[i];

      if (state.is_newly_active && buffer->items[i].connected) {
        state.is_newly_active = false;
        OnGamepadConnectionChange(true, i, buffer->items[i]);
      }
    }
  }

  bool did_notify = CheckForUserGesture();

  // Avoid double-notifying gamepad connection observers when a gamepad is
  // connected in the same polling cycle as the initial user gesture.
  //
  // If a gamepad is connected in the same polling cycle as the initial user
  // gesture, the user gesture will trigger a gamepadconnected event during the
  // CheckForUserGesture call above. If we don't clear |is_newly_active| here,
  // we will notify again for the same gamepad on the next polling cycle.
  if (did_notify) {
    for (size_t i = 0; i < Gamepads::kItemsLengthCap; ++i)
      pad_states_.get()[i].is_newly_active = false;
  }

  // Schedule our next interval of polling.
  ScheduleDoPoll();
}

void GamepadProvider::DisconnectUnrecognizedGamepad(GamepadSource source,
                                                    int source_id) {
  for (auto& fetcher : data_fetchers_) {
    if (fetcher->source() == source) {
      bool disconnected = fetcher->DisconnectUnrecognizedGamepad(source_id);
      DCHECK(disconnected);
      return;
    }
  }
}

void GamepadProvider::ScheduleDoPoll() {
  DCHECK(polling_thread_->task_runner()->BelongsToCurrentThread());
  if (have_scheduled_do_poll_)
    return;

  {
    base::AutoLock lock(is_paused_lock_);
    if (is_paused_)
      return;
  }

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::BindOnce(&GamepadProvider::DoPoll, Unretained(this)),
      sampling_interval_delta_);
  have_scheduled_do_poll_ = true;
}

void GamepadProvider::OnGamepadConnectionChange(bool connected,
                                                uint32_t index,
                                                const Gamepad& pad) {
  if (connection_change_client_)
    connection_change_client_->OnGamepadConnectionChange(connected, index, pad);
}

bool GamepadProvider::CheckForUserGesture() {
  base::AutoLock lock(user_gesture_lock_);
  if (user_gesture_observers_.empty() && ever_had_user_gesture_)
    return false;

  const Gamepads* pads = gamepad_shared_buffer_->buffer();
  if (GamepadsHaveUserGesture(*pads)) {
    ever_had_user_gesture_ = true;
    for (auto& closure_and_thread : user_gesture_observers_) {
      closure_and_thread.second->PostTask(FROM_HERE,
                                          std::move(closure_and_thread.first));
    }
    user_gesture_observers_.clear();
    return true;
  }
  return false;
}

}  // namespace device
