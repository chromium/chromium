// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "device/gamepad/gamepad_provider.h"

#include <stddef.h>
#include <string.h>

#include <cmath>
#include <iterator>
#include <memory>
#include <ranges>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
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
#include "device/gamepad/simulated_gamepad_data_fetcher.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "third_party/abseil-cpp/absl/base/dynamic_annotations.h"

namespace device {

namespace {

bool TouchEventEqual(const GamepadTouch& a, const GamepadTouch& b) {
  return a.touch_id == b.touch_id && a.surface_id == b.surface_id &&
         a.has_surface_dimensions == b.has_surface_dimensions && a.x == b.x &&
         a.y == b.y && a.surface_width == b.surface_width &&
         a.surface_height == b.surface_height;
}

bool AreTouchEventsEqual(
    const std::array<GamepadTouch, Gamepad::kTouchEventsLengthCap>& old_touches,
    const std::array<GamepadTouch, Gamepad::kTouchEventsLengthCap>&
        new_touches) {
  return std::ranges::equal(old_touches, new_touches, TouchEventEqual);
}

bool HasInputChanged(const Gamepad& old_pad, const Gamepad& new_pad) {
  // If the timestamp hasn't changed, nothing could have changed.
  if (old_pad.timestamp == new_pad.timestamp) {
    return false;
  }

  // Note: We intentionally check touch_events_length for changes, but not
  // buttons_length or axes_length. For buttons/axes, the length is expected
  // to remain constant for a connected gamepad; a change likely means a
  // disconnect/reconnect. For touch events, the length is expected to
  // change as the user interacts with the touch surface. If we don't check
  // the length, we could miss changes when the number of active touches
  // changes, even if the values in unused slots match.
  return (!std::ranges::equal(old_pad.axes, new_pad.axes) ||
          !std::ranges::equal(old_pad.buttons, new_pad.buttons) ||
          old_pad.touch_events_length != new_pad.touch_events_length ||
          !AreTouchEventsEqual(old_pad.touch_events, new_pad.touch_events));
}

}  // namespace

constexpr int64_t kPollingIntervalMilliseconds = 4;  // ~250 Hz

GamepadProvider::SimulatedGamepadState::SimulatedGamepadState() = default;
GamepadProvider::SimulatedGamepadState::~SimulatedGamepadState() = default;

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
  simulated_gamepad_data_fetcher_ = nullptr;
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

void GamepadProvider::AddSimulatedGamepad(base::UnguessableToken token,
                                          SimulatedGamepadParams params) {
  const auto& [state_it, did_insert] = simulated_gamepad_state_.emplace(
      std::piecewise_construct, std::forward_as_tuple(token),
      std::forward_as_tuple());
  CHECK(did_insert);
  state_it->second.touch_surface_count = params.touch_surface_bounds.size();
  polling_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&GamepadProvider::DoAddSimulatedGamepad,
                     base::Unretained(this), token, std::move(params)));
}

void GamepadProvider::RemoveSimulatedGamepad(base::UnguessableToken token) {
  simulated_gamepad_state_.erase(token);
  polling_thread_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&GamepadProvider::DoRemoveSimulatedGamepad,
                                base::Unretained(this), token));
}

void GamepadProvider::SimulateAxisInput(base::UnguessableToken token,
                                        uint32_t index,
                                        double logical_value) {
  auto state_it = simulated_gamepad_state_.find(token);
  if (state_it == simulated_gamepad_state_.end()) {
    return;
  }
  state_it->second.inputs.pending_axis_inputs[index] = logical_value;
}

void GamepadProvider::SimulateButtonInput(base::UnguessableToken token,
                                          uint32_t index,
                                          double logical_value,
                                          std::optional<bool> pressed,
                                          std::optional<bool> touched) {
  auto state_it = simulated_gamepad_state_.find(token);
  if (state_it == simulated_gamepad_state_.end()) {
    return;
  }
  SimulatedGamepadButton& button =
      state_it->second.inputs.pending_button_inputs[index];
  button.logical_value = logical_value;
  button.pressed = pressed;
  button.touched = touched;
}

std::optional<uint32_t> GamepadProvider::SimulateTouchInput(
    base::UnguessableToken token,
    uint32_t surface_id,
    double logical_x,
    double logical_y) {
  auto state_it = simulated_gamepad_state_.find(token);
  if (state_it == simulated_gamepad_state_.end()) {
    return std::nullopt;
  }
  SimulatedGamepadState& state = state_it->second;
  if (surface_id >= state.touch_surface_count) {
    return std::nullopt;
  }
  uint32_t touch_id = state.next_touch_id++;
  state.inputs.active_touches.emplace_back(touch_id, surface_id, logical_x,
                                           logical_y);
  return touch_id;
}

void GamepadProvider::SimulateTouchMove(base::UnguessableToken token,
                                        uint32_t touch_id,
                                        double logical_x,
                                        double logical_y) {
  auto state_it = simulated_gamepad_state_.find(token);
  if (state_it == simulated_gamepad_state_.end()) {
    return;
  }
  auto& active_touches = state_it->second.inputs.active_touches;
  auto touch_it = std::ranges::find_if(active_touches, [&](const auto& touch) {
    return touch.touch_id == touch_id;
  });
  if (touch_it == active_touches.end()) {
    return;
  }
  touch_it->logical_x = logical_x;
  touch_it->logical_y = logical_y;
}

void GamepadProvider::SimulateTouchEnd(base::UnguessableToken token,
                                       uint32_t touch_id) {
  auto state_it = simulated_gamepad_state_.find(token);
  if (state_it == simulated_gamepad_state_.end()) {
    return;
  }
  auto& active_touches = state_it->second.inputs.active_touches;
  active_touches.erase(
      std::remove_if(
          active_touches.begin(), active_touches.end(),
          [&](const auto& touch) { return touch.touch_id == touch_id; }),
      active_touches.end());
}

void GamepadProvider::SimulateInputFrame(base::UnguessableToken token) {
  SimulatedGamepadInputs inputs;
  auto state_it = simulated_gamepad_state_.find(token);
  if (state_it != simulated_gamepad_state_.end()) {
    SimulatedGamepadState& state = state_it->second;
    std::swap(inputs.pending_axis_inputs, state.inputs.pending_axis_inputs);
    std::swap(inputs.pending_button_inputs, state.inputs.pending_button_inputs);
    inputs.active_touches = state.inputs.active_touches;
  }
  polling_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&GamepadProvider::DoSimulateInputFrame,
                     base::Unretained(this), token, std::move(inputs)));
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

  if (fetcher->source() == GamepadSource::kSimulated) {
    CHECK(!simulated_gamepad_data_fetcher_);
    simulated_gamepad_data_fetcher_ =
        static_cast<SimulatedGamepadDataFetcher*>(fetcher.get());
  }
  InitializeDataFetcher(fetcher.get());
  data_fetchers_.push_back(std::move(fetcher));
}

void GamepadProvider::DoRemoveSourceGamepadDataFetcher(GamepadSource source) {
  DCHECK(polling_thread_->task_runner()->BelongsToCurrentThread());

  if (source == GamepadSource::kSimulated) {
    simulated_gamepad_data_fetcher_ = nullptr;
  }
  for (auto it = data_fetchers_.begin(); it != data_fetchers_.end();) {
    if ((*it)->source() == source) {
      it = data_fetchers_.erase(it);
    } else {
      ++it;
    }
  }
}

void GamepadProvider::DoAddSimulatedGamepad(base::UnguessableToken token,
                                            SimulatedGamepadParams params) {
  CHECK(polling_thread_->task_runner()->BelongsToCurrentThread());
  if (!simulated_gamepad_data_fetcher_) {
    return;
  }
  simulated_gamepad_data_fetcher_->AddSimulatedGamepad(token,
                                                       std::move(params));
}

void GamepadProvider::DoRemoveSimulatedGamepad(base::UnguessableToken token) {
  CHECK(polling_thread_->task_runner()->BelongsToCurrentThread());
  if (!simulated_gamepad_data_fetcher_) {
    return;
  }
  simulated_gamepad_data_fetcher_->RemoveSimulatedGamepad(token);
}

void GamepadProvider::DoSimulateInputFrame(base::UnguessableToken token,
                                           SimulatedGamepadInputs inputs) {
  CHECK(polling_thread_->task_runner()->BelongsToCurrentThread());
  if (!simulated_gamepad_data_fetcher_) {
    return;
  }
  simulated_gamepad_data_fetcher_->SimulateInputFrame(token, std::move(inputs));
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
    pad_states_[i].is_active = false;

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

  for (size_t i = 0; i < Gamepads::kItemsLengthCap; ++i) {
    PadState& state = pad_states_[i];

    // Send out disconnect events using the last polled data.
    if (ever_had_user_gesture_ && !state.is_newly_active && !state.is_active &&
        state.source != GamepadSource::kNone) {
      auto pad = old_buffer.items[i];
      pad.connected = false;
      OnGamepadConnectionChange(false, i, pad);
      ClearPadState(state);
    }

    MapAndSanitizeGamepadData(&state, &new_buffer.items[i], sanitize_);
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
      PadState& state = pad_states_[i];
      if (state.is_newly_active && new_buffer.items[i].connected) {
        state.is_newly_active = false;
        OnGamepadConnectionChange(true, i, new_buffer.items[i]);
      }

      // Raw input change detection.
      if (base::FeatureList::IsEnabled(features::kGamepadRawInputChangeEvent)) {
        if (new_buffer.items[i].connected && !state.is_newly_active &&
            HasInputChanged(old_buffer.items[i], new_buffer.items[i])) {
          has_input_changed_.store(true);
          OnGamepadRawInputChanged(i, new_buffer.items[i]);
        }
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
      pad_states_[i].is_newly_active = false;
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

void GamepadProvider::OnGamepadRawInputChanged(uint32_t index,
                                               const Gamepad& pad) {
  if (gamepad_change_client_) {
    main_thread_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&GamepadChangeClient::OnGamepadRawInputChanged,
                       base::Unretained(gamepad_change_client_), index, pad));
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
