// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "device/gamepad/game_controller_data_fetcher_mac.h"

#import <GameController/GameController.h>
#include <string.h>

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/mac/mac_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "device/gamepad/game_controller_gamepad.h"
#include "device/gamepad/gamepad_pad_state_provider.h"
#include "device/gamepad/gamepad_standard_mappings.h"
#include "device/gamepad/public/cpp/gamepad.h"
#include "device/gamepad/public/cpp/gamepad_features.h"
#include "device/gamepad/public/mojom/gamepad.mojom.h"

@interface GameControllerNotificationHandler : NSObject
- (instancetype)
    initWithImpl:(base::WeakPtr<device::GameControllerDataFetcherMac::
                                    GameControllerDataFetcherMacImpl>)impl
      taskRunner:(scoped_refptr<base::SingleThreadTaskRunner>)runner;
- (void)onControllerDidConnect:(NSNotification*)notification;
- (void)onControllerDidDisconnect:(NSNotification*)notification;
@end

namespace device {

struct GameControllerDataFetcherMac::GameControllerDataFetcherMacImpl {
  explicit GameControllerDataFetcherMacImpl(GameControllerDataFetcherMac* owner)
      : owner_(owner), weak_factory_(this) {}

  void OnGameControllerConnect(GCController* controller);
  void OnGameControllerDisconnect(GCController* controller);

  static void RegisterOnMainThread(
      base::WeakPtr<GameControllerDataFetcherMacImpl> impl,
      scoped_refptr<base::SingleThreadTaskRunner> polling_task_runner);
  static void UnregisterOnMainThread(
      GameControllerNotificationHandler* handler);

  // The owning fetcher, used to access shared state like `GetPadState`.
  const raw_ptr<GameControllerDataFetcherMac> owner_;

  GameControllerNotificationHandler* __strong notification_handler_;
  base::flat_map<GCController*, int> controller_to_source_id_;
  base::flat_map<int, std::unique_ptr<GameControllerGamepad>> gamepads_;

  base::WeakPtrFactory<GameControllerDataFetcherMacImpl> weak_factory_;
};

}  // namespace device

@implementation GameControllerNotificationHandler {
 @private
  base::WeakPtr<
      device::GameControllerDataFetcherMac::GameControllerDataFetcherMacImpl>
      _implWeak;
  scoped_refptr<base::SingleThreadTaskRunner> _pollingTaskRunner;
}

- (instancetype)
    initWithImpl:(base::WeakPtr<device::GameControllerDataFetcherMac::
                                    GameControllerDataFetcherMacImpl>)impl
      taskRunner:(scoped_refptr<base::SingleThreadTaskRunner>)runner {
  self = [super init];
  if (self) {
    _implWeak = impl;
    _pollingTaskRunner = runner;
  }
  return self;
}

- (void)onControllerDidConnect:(NSNotification*)notification {
  _pollingTaskRunner->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<device::GameControllerDataFetcherMac::
                               GameControllerDataFetcherMacImpl> impl,
             id controller) {
            if (impl) {
              impl->OnGameControllerConnect(controller);
            }
          },
          _implWeak, notification.object));
}

- (void)onControllerDidDisconnect:(NSNotification*)notification {
  _pollingTaskRunner->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<device::GameControllerDataFetcherMac::
                               GameControllerDataFetcherMacImpl> impl,
             id controller) {
            if (impl) {
              impl->OnGameControllerDisconnect(controller);
            }
          },
          _implWeak, notification.object));
}
@end

namespace device {

namespace {

const int kGCControllerPlayerIndexCount = 4;

// Returns true if |controller| should be enumerated by this data fetcher.
bool IsSupported(GCController* controller) {
  // We only support the extendedGamepad profile.
  if (!controller.extendedGamepad) {
    return false;
  }

  // In macOS 10.15, support for some console gamepads was added to the Game
  // Controller framework and a productCategory property was added to enable
  // applications to detect the new devices. These gamepads are already
  // supported in Chrome through other data fetchers and must be blocked here to
  // avoid double-enumeration.
  NSString* product_category = controller.productCategory;
  if ([product_category isEqualToString:@"HID"] ||
      [product_category isEqualToString:@"Switch Pro Controller"] ||
      [product_category isEqualToString:@"Nintendo Switch JoyCon (L/R)"]) {
    return false;
  }

  if (!base::FeatureList::IsEnabled(
          features::kXboxUseGameControllerDataFetcherMac) &&
      [product_category isEqualToString:@"Xbox One"]) {
    return false;
  }

  if (!base::FeatureList::IsEnabled(
          features::kPlayStationUseGameControllerDataFetcherMac) &&
      ([product_category isEqualToString:@"DualShock 4"] ||
       [product_category isEqualToString:@"DualSense"])) {
    return false;
  }

  return true;
}

}  // namespace

void GameControllerDataFetcherMac::GameControllerDataFetcherMacImpl::
    OnGameControllerConnect(GCController* controller) {
  DCHECK(owner_->polling_task_runner_->RunsTasksInCurrentSequence());
  if (!IsSupported(controller)) {
    return;
  }

  // Ignore controllers that have already been connected to.
  if (controller_to_source_id_.find(controller) !=
      controller_to_source_id_.end()) {
    return;
  }

  // Assign a new unique source ID.
  const int source_id = owner_->next_source_id_++;
  controller_to_source_id_[controller] = source_id;

  auto gamepad = std::make_unique<GameControllerGamepad>(controller);

  // Initialize the pad state if a slot is available. If not, GetGamepadData
  // will try again during the next polling cycle.
  PadState* state = owner_->GetPadState(owner_->next_source_id_);
  if (state) {
    state->is_initialized = true;
    gamepad->InitializeStaticData(state->data);
  }

  // Store the gamepad object.
  owner_->impl_->gamepads_.emplace(source_id, std::move(gamepad));
}

void GameControllerDataFetcherMac::GameControllerDataFetcherMacImpl::
    OnGameControllerDisconnect(GCController* controller) {
  DCHECK(owner_->polling_task_runner_->RunsTasksInCurrentSequence());
  auto it = controller_to_source_id_.find(controller);
  if (it == controller_to_source_id_.end()) {
    return;
  }

  const int source_id = it->second;

  // Mark the pad as disconnected
  PadState* state = owner_->GetPadState(source_id);
  if (state) {
    state->data.connected = false;
  }

  // Shut down haptics and remove the gamepad object
  auto gamepad_it = owner_->impl_->gamepads_.find(source_id);
  if (gamepad_it != owner_->impl_->gamepads_.end()) {
    gamepad_it->second->Shutdown();
    owner_->impl_->gamepads_.erase(gamepad_it);
  }

  controller_to_source_id_.erase(it);
}

void GameControllerDataFetcherMac::GameControllerDataFetcherMacImpl::
    RegisterOnMainThread(
        base::WeakPtr<GameControllerDataFetcherMacImpl> impl,
        scoped_refptr<base::SingleThreadTaskRunner> polling_task_runner) {
  GameControllerNotificationHandler* handler =
      [[GameControllerNotificationHandler alloc]
          initWithImpl:impl
            taskRunner:polling_task_runner];

  // Register for gamepad connection and disconnection notifications.
  // The notifications will fire for already-connected controllers once we
  // register.
  [[NSNotificationCenter defaultCenter]
      addObserver:handler
         selector:@selector(onControllerDidConnect:)
             name:GCControllerDidConnectNotification
           object:nil];
  [[NSNotificationCenter defaultCenter]
      addObserver:handler
         selector:@selector(onControllerDidDisconnect:)
             name:GCControllerDidDisconnectNotification
           object:nil];

  polling_task_runner->PostTask(
      FROM_HERE, base::BindOnce(
                     [](base::WeakPtr<GameControllerDataFetcherMacImpl> impl,
                        GameControllerNotificationHandler* handler) {
                       if (impl) {
                         impl->notification_handler_ = handler;
                       }
                     },
                     impl, handler));
}

void GameControllerDataFetcherMac::GameControllerDataFetcherMacImpl::
    UnregisterOnMainThread(GameControllerNotificationHandler* handler) {
  [[NSNotificationCenter defaultCenter] removeObserver:handler];
}

GameControllerDataFetcherMac::GameControllerDataFetcherMac()
    : impl_(std::make_unique<GameControllerDataFetcherMacImpl>(this)),
      main_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()) {}

GameControllerDataFetcherMac::~GameControllerDataFetcherMac() {
  if (base::FeatureList::IsEnabled(
          features::kXboxUseGameControllerDataFetcherMac) ||
      base::FeatureList::IsEnabled(
          features::kPlayStationUseGameControllerDataFetcherMac)) {
    GameControllerNotificationHandler* handler = impl_->notification_handler_;
    impl_->notification_handler_ = nil;
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &GameControllerDataFetcherMacImpl::UnregisterOnMainThread,
            handler));

    for (auto& entry : impl_->gamepads_) {
      if (entry.second) {
        entry.second->Shutdown();
      }
    }
  }
}

void GameControllerDataFetcherMac::OnAddedToProvider() {
  if (base::FeatureList::IsEnabled(
          features::kXboxUseGameControllerDataFetcherMac) ||
      base::FeatureList::IsEnabled(
          features::kPlayStationUseGameControllerDataFetcherMac)) {
    polling_task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&GameControllerDataFetcherMacImpl::RegisterOnMainThread,
                       impl_->weak_factory_.GetWeakPtr(),
                       polling_task_runner_));
  }
}

GamepadSource GameControllerDataFetcherMac::source() {
  return Factory::static_source();
}

void GameControllerDataFetcherMac::GetGamepadData(bool) {
  if (base::FeatureList::IsEnabled(
          features::kXboxUseGameControllerDataFetcherMac) ||
      base::FeatureList::IsEnabled(
          features::kPlayStationUseGameControllerDataFetcherMac)) {
    for (const auto& entry : impl_->gamepads_) {
      const int source_id = entry.first;

      PadState* state = GetPadState(source_id);

      if (!state) {
        continue;
      }

      if (!state->is_initialized) {
        state->is_initialized = true;
        entry.second->InitializeStaticData(state->data);
      }

      state->data.timestamp = CurrentTimeInMicroseconds();
      entry.second->UpdateState(state->data);
    }

    return;
  }

  NSArray* controllers = [GCController controllers];

  // In the first pass, record which player indices are still in use so unused
  // indices can be assigned to newly connected gamepads.
  bool player_indices[Gamepads::kItemsLengthCap];
  std::fill(player_indices, player_indices + Gamepads::kItemsLengthCap, false);
  for (GCController* controller in controllers) {
    if (!IsSupported(controller))
      continue;

    int player_index = controller.playerIndex;
    if (player_index != GCControllerPlayerIndexUnset)
      player_indices[player_index] = true;
  }

  for (size_t i = 0; i < Gamepads::kItemsLengthCap; ++i) {
    if (connected_[i] && !player_indices[i])
      connected_[i] = false;
  }

  // In the second pass, assign indices to newly connected gamepads and fetch
  // the gamepad state.
  for (GCController* controller in controllers) {
    if (!IsSupported(controller))
      continue;

    int player_index = controller.playerIndex;
    if (player_index == GCControllerPlayerIndexUnset) {
      player_index = NextUnusedPlayerIndex();
      if (player_index == GCControllerPlayerIndexUnset)
        continue;
    }

    PadState* state = GetPadState(player_index);
    if (!state)
      continue;

    Gamepad& pad = state->data;

    // This first time we encounter a gamepad, set its name, mapping, and
    // axes/button counts. This information is static, so it only needs to be
    // done once.
    if (!state->is_initialized) {
      state->is_initialized = true;
      NSString* vendorName = controller.vendorName;
      NSString* ident =
          [NSString stringWithFormat:@"%@ (STANDARD GAMEPAD)",
                                     vendorName ? vendorName : @"Unknown"];

      pad.mapping = GamepadMapping::kStandard;
      pad.SetID(base::SysNSStringToUTF16(ident));
      pad.axes_length = AXIS_INDEX_COUNT;
      pad.buttons_length = BUTTON_INDEX_COUNT - 1;
      pad.connected = true;
      connected_[player_index] = true;

      controller.playerIndex =
          static_cast<GCControllerPlayerIndex>(player_index);
    }

    pad.timestamp = CurrentTimeInMicroseconds();

    auto extended_gamepad = [controller extendedGamepad];
    pad.axes[AXIS_INDEX_LEFT_STICK_X] =
        extended_gamepad.leftThumbstick.xAxis.value;
    pad.axes[AXIS_INDEX_LEFT_STICK_Y] =
        -extended_gamepad.leftThumbstick.yAxis.value;
    pad.axes[AXIS_INDEX_RIGHT_STICK_X] =
        extended_gamepad.rightThumbstick.xAxis.value;
    pad.axes[AXIS_INDEX_RIGHT_STICK_Y] =
        -extended_gamepad.rightThumbstick.yAxis.value;

#define BUTTON(i, b)                      \
  pad.buttons[i].pressed = [b isPressed]; \
  pad.buttons[i].value = [b value];

    BUTTON(BUTTON_INDEX_PRIMARY, extended_gamepad.buttonA);
    BUTTON(BUTTON_INDEX_SECONDARY, extended_gamepad.buttonB);
    BUTTON(BUTTON_INDEX_TERTIARY, extended_gamepad.buttonX);
    BUTTON(BUTTON_INDEX_QUATERNARY, extended_gamepad.buttonY);
    BUTTON(BUTTON_INDEX_LEFT_SHOULDER, extended_gamepad.leftShoulder);
    BUTTON(BUTTON_INDEX_RIGHT_SHOULDER, extended_gamepad.rightShoulder);
    BUTTON(BUTTON_INDEX_LEFT_TRIGGER, extended_gamepad.leftTrigger);
    BUTTON(BUTTON_INDEX_RIGHT_TRIGGER, extended_gamepad.rightTrigger);

    BUTTON(BUTTON_INDEX_DPAD_UP, extended_gamepad.dpad.up);
    BUTTON(BUTTON_INDEX_DPAD_DOWN, extended_gamepad.dpad.down);
    BUTTON(BUTTON_INDEX_DPAD_LEFT, extended_gamepad.dpad.left);
    BUTTON(BUTTON_INDEX_DPAD_RIGHT, extended_gamepad.dpad.right);

#undef BUTTON
  }
}

int GameControllerDataFetcherMac::NextUnusedPlayerIndex() {
  for (int i = 0; i < kGCControllerPlayerIndexCount; ++i) {
    if (!connected_[i])
      return i;
  }
  return GCControllerPlayerIndexUnset;
}

void GameControllerDataFetcherMac::PlayEffect(
    int source_id,
    mojom::GamepadHapticEffectType type,
    mojom::GamepadEffectParametersPtr params,
    mojom::GamepadHapticsManager::PlayVibrationEffectOnceCallback callback,
    scoped_refptr<base::SequencedTaskRunner> callback_runner) {
  if (!base::FeatureList::IsEnabled(
          features::kXboxUseGameControllerDataFetcherMac) &&
      !base::FeatureList::IsEnabled(
          features::kPlayStationUseGameControllerDataFetcherMac)) {
    RunVibrationCallback(
        std::move(callback), std::move(callback_runner),
        mojom::GamepadHapticsResult::GamepadHapticsResultNotSupported);
    return;
  }
  auto it = impl_->gamepads_.find(source_id);
  if (it != impl_->gamepads_.end()) {
    it->second->PlayEffect(type, std::move(params), std::move(callback),
                           std::move(callback_runner));
  } else {
    RunVibrationCallback(
        std::move(callback), std::move(callback_runner),
        mojom::GamepadHapticsResult::GamepadHapticsResultNotSupported);
  }
}

void GameControllerDataFetcherMac::ResetVibration(
    int source_id,
    mojom::GamepadHapticsManager::ResetVibrationActuatorCallback callback,
    scoped_refptr<base::SequencedTaskRunner> callback_runner) {
  if (!base::FeatureList::IsEnabled(
          features::kXboxUseGameControllerDataFetcherMac) &&
      !base::FeatureList::IsEnabled(
          features::kPlayStationUseGameControllerDataFetcherMac)) {
    RunVibrationCallback(
        std::move(callback), std::move(callback_runner),
        mojom::GamepadHapticsResult::GamepadHapticsResultNotSupported);
    return;
  }
  auto it = impl_->gamepads_.find(source_id);
  if (it != impl_->gamepads_.end()) {
    it->second->ResetVibration(std::move(callback), std::move(callback_runner));
  } else {
    RunVibrationCallback(
        std::move(callback), std::move(callback_runner),
        mojom::GamepadHapticsResult::GamepadHapticsResultComplete);
  }
}

}  // namespace device
