// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "device/gamepad/gamepad_provider.h"

#include <stddef.h>
#include <string.h>
#include <cmath>
#include <memory>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/message_loop/message_pump_type.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "device/gamepad/gamepad_data_fetcher.h"
#include "device/gamepad/gamepad_data_fetcher_manager.h"
#include "device/gamepad/gamepad_user_gesture.h"
#include "device/gamepad/public/cpp/gamepad_features.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "third_party/abseil-cpp/absl/base/dynamic_annotations.h"

namespace device {

namespace {
std::vector<mojom::ButtonChangePtr> CompareButtons(const Gamepad* old_gamepad,
                                                   const Gamepad* new_gamepad) {
  if (!new_gamepad)
    return {};

  std::vector<mojom::ButtonChangePtr> button_changes;
  const auto* new_buttons = new_gamepad->buttons;
  const auto* old_buttons = old_gamepad ? old_gamepad->buttons : nullptr;
  for (size_t i = 0; i < new_gamepad->buttons_length; ++i) {
    double new_value = new_buttons[i].value;
    bool new_pressed = new_buttons[i].pressed;
    if (old_buttons && i < old_gamepad->buttons_length) {
      double old_value = old_buttons[i].value;
      bool old_pressed = old_buttons[i].pressed;
      auto this_change = mojom::ButtonChange::New();
      this_change->button_index = i;
      this_change->button_snapshot = new_buttons[i];
      bool relevant_change = false;
      if (old_value != new_value) {
        relevant_change = true;
        this_change->value_changed = true;
      }
      if (old_pressed != new_pressed) {
        relevant_change = true;
        this_change->button_down = new_pressed;
        this_change->button_up = !new_pressed;
      }
      if (relevant_change)
        button_changes.push_back(std::move(this_change));
    }
  }
  return button_changes;
}

std::vector<mojom::AxisChangePtr> CompareAxes(const Gamepad* old_gamepad,
                                              const Gamepad* new_gamepad) {
  if (!new_gamepad)
    return {};

  std::vector<mojom::AxisChangePtr> axis_changes;
  const auto* new_axes = new_gamepad->axes;
  const auto* old_axes = old_gamepad ? old_gamepad->axes : nullptr;
  for (size_t i = 0; i < new_gamepad->axes_length; ++i) {
    const double new_value = new_axes[i];
    if (old_axes && i < old_gamepad->axes_length) {
      const double old_value = old_axes[i];
      if (old_value != new_value) {
        auto this_change = mojom::AxisChange::New();
        this_change->axis_index = i;
        this_change->axis_snapshot = new_value;
        axis_changes.push_back(std::move(this_change));
      }
    }
  }
  return axis_changes;
}

mojom::GamepadChangesPtr CompareGamepadState(const Gamepad* old_gamepad,
                                             const Gamepad* new_gamepad,
                                             size_t index) {
  return mojom::GamepadChanges::New(
      index, CompareButtons(old_gamepad, new_gamepad),
      CompareAxes(old_gamepad, new_gamepad), new_gamepad->timestamp);
}
}  // namespace

constexpr int64_t kPollingIntervalMilliseconds = 4;  // ~250 Hz

GamepadProvider::GamepadProvider(GamepadChangeClient* gamepad_change_client)
    : gamepad_shared_buffer_(std::make_unique<GamepadSharedBuffer>()),
      main_thread_task_runner_(
          base::SingleThreadTaskRunner::GetCurrentDefault()),
      gamepad_change_client_(gamepad_change_client) {
  Initialize(std::unique_ptr<GamepadDataFetcher>());
}

GamepadProvider::GamepadProvider(GamepadChangeClient* gamepad_change_client,
                                 std::unique_ptr<GamepadDataFetcher> fetcher,
                                 std::unique_ptr<base::Thread> polling_thread)
    : gamepad_shared_buffer_(std::make_unique<GamepadSharedBuffer>()),
      polling_thread_(std::move(polling_thread)),
      main_thread_task_runner_(
          base::SingleThreadTaskRunner::GetCurrentDefault()),
      gamepad_change_client_(gamepad_change_client) {
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
                     std::move(callback),
                     base::SingleThreadTaskRunner::GetCurrentDefault()));
}

void GamepadProvider::ResetVibrationActuator(
    uint32_t pad_index,
    mojom::GamepadHapticsManager::ResetVibrationActuatorCallback callback) {
  polling_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&GamepadProvider::ResetVibrationOnPollingThread,
                     Unretained(this), pad_index, std::move(callback),
                     base::SingleThreadTaskRunner::GetCurrentDefault()));
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
  user_gesture_observers_.emplace_back(
      std::move(closure), base::SingleThreadTaskRunner::GetCurrentDefault());
}

void GamepadProvider::OnDevicesChanged(base::SystemMonitor::DeviceType type) {
  base::AutoLock lock(devices_changed_lock_);
  devices_changed_ = true;
}

void GamepadProvider::Initialize(std::unique_ptr<GamepadDataFetcher> fetcher) {
  sampling_interval_delta_ = base::Milliseconds(kPollingIntervalMilliseconds);

  base::SystemMonitor* monitor = base::SystemMonitor::Get();
  if (monitor)
    monitor->AddDevicesChangedObserver(this);

  if (!polling_thread_)
    polling_thread_ = std::make_unique<base::Thread>("Gamepad polling thread");
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // On Linux, the data fetcher needs to watch file descriptors, so the message
  // loop needs to be a libevent loop.
  const base::MessagePumpType kMessageLoopType = base::MessagePumpType::IO;
#elif BUILDFLAG(IS_ANDROID)
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

  InitializeDataFetcher(fetcher.get());
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

  ABSL_ANNOTATE_BENIGN_RACE_SIZED(
      gamepad_shared_buffer_->buffer(), sizeof(Gamepads),
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

  Gamepads old_buffer;
  Gamepads new_buffer;
  GetCurrentGamepadData(&old_buffer);

  std::vector<mojom::GamepadChangesPtr> changes;
  changes.reserve(Gamepads::kItemsLengthCap);
  for (size_t i = 0; i < Gamepads::kItemsLengthCap; ++i) {
    PadState& state = pad_states_.get()[i];

    // Send out disconnect events using the last polled data.
    if (ever_had_user_gesture_ && !state.is_newly_active && !state.is_active &&
        state.source != GamepadSource::kNone) {
      auto pad = old_buffer.items[i];
      pad.connected = false;
      OnGamepadConnectionChange(false, i, pad);
      ClearPadState(state);
    }

    MapAndSanitizeGamepadData(&state, &new_buffer.items[i], sanitize_);
    if (gamepad_change_client_ &&
        features::AreGamepadButtonAxisEventsEnabled()) {
      changes.push_back(
          CompareGamepadState(&old_buffer.items[i], &new_buffer.items[i], i));
    }
  }

  {
    base::AutoLock lock(shared_memory_lock_);

    // Acquire the SeqLock. There is only ever one writer to this data.
    // See gamepad_shared_buffer.h.
    gamepad_shared_buffer_->WriteBegin();
    *gamepad_shared_buffer_->buffer() = new_buffer;
    gamepad_shared_buffer_->WriteEnd();
  }

  if (ever_had_user_gesture_) {
    for (size_t i = 0; i < Gamepads::kItemsLengthCap; ++i) {
      PadState& state = pad_states_.get()[i];
      if (state.is_newly_active && new_buffer.items[i].connected) {
        state.is_newly_active = false;
        OnGamepadConnectionChange(true, i, new_buffer.items[i]);
      }
    }
    for (auto& change : changes) {
      SendChangeEvents(std::move(change));
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

void GamepadProvider::SendChangeEvents(
    mojom::GamepadChangesPtr gamepad_changes) {
  DCHECK(gamepad_changes);
  if (gamepad_changes->button_changes.empty() &&
      gamepad_changes->axis_changes.empty()) {
    return;
  }
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&GamepadChangeClient::OnGamepadChange,
                                base::Unretained(gamepad_change_client_),
                                std::move(gamepad_changes)));
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

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::BindOnce(&GamepadProvider::DoPoll, Unretained(this)),
      sampling_interval_delta_);
  have_scheduled_do_poll_ = true;
}

void GamepadProvider::OnGamepadConnectionChange(bool connected,
                                                uint32_t index,
                                                const Gamepad& pad) {
  if (gamepad_change_client_) {
    main_thread_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&GamepadChangeClient::OnGamepadConnectionChange,
                       base::Unretained(gamepad_change_client_), connected,
                       index, pad));
  }
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
