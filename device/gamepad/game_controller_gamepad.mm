// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/game_controller_gamepad.h"

#import <CoreHaptics/CoreHaptics.h>
#import <GameController/GameController.h>

#include <algorithm>
#include <memory>

#include "base/apple/foundation_util.h"
#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "device/gamepad/gamepad_standard_mappings.h"

namespace device {

namespace {

// Default value for sharpness to use for haptics, must be between [0.0, 1.0]
const float kDefaultHapticSharpness = 1.0f;

// Sony Gamepad touchpad dimensions
// TODO: Verify this also applies to DualSense
constexpr uint16_t kSonyTouchDimensionX = 1920;
constexpr uint16_t kSonyTouchDimensionY = 942;

// Helper to create a new haptic player for a continuous vibration event.
id<CHHapticPatternPlayer> CreateContinuousPlayer(double intensity,
                                                 CHHapticEngine* engine) {
  if (!engine) {
    return nil;
  }

  NSError* error = nil;
  float float_intensity = static_cast<float>(std::clamp(intensity, 0.0, 1.0));
  float float_sharpness = kDefaultHapticSharpness;

  CHHapticEventParameter* intensity_param = [[CHHapticEventParameter alloc]
      initWithParameterID:CHHapticEventParameterIDHapticIntensity
                    value:float_intensity];
  CHHapticEventParameter* sharpness_param = [[CHHapticEventParameter alloc]
      initWithParameterID:CHHapticEventParameterIDHapticSharpness
                    value:float_sharpness];

  CHHapticEvent* event = [[CHHapticEvent alloc]
      initWithEventType:CHHapticEventTypeHapticContinuous
             parameters:@[ intensity_param, sharpness_param ]
           relativeTime:0
               duration:GCHapticDurationInfinite];

  CHHapticPattern* pattern = [[CHHapticPattern alloc] initWithEvents:@[ event ]
                                                          parameters:@[]
                                                               error:&error];
  if (!pattern) {
    return nil;
  }

  id<CHHapticPatternPlayer> player = [engine createPlayerWithPattern:pattern
                                                               error:&error];
  if (!player || ![player startAtTime:0 error:&error]) {
    return nil;
  }

  return player;
}

void UpdateHapticPlayer(double intensity,
                        CHHapticEngine* engine,
                        __strong id<CHHapticPatternPlayer>& player) {
  if (!engine) {
    return;
  }

  double float_intensity = std::clamp(intensity, 0.0, 1.0);

  if (float_intensity == 0.0) {
    if (player) {
      [player stopAtTime:0 error:nil];
      player = nil;
    }
    return;
  }

  if (player) {
    NSError* error = nil;
    NSArray* params = @[
      [[CHHapticDynamicParameter alloc]
          initWithParameterID:CHHapticDynamicParameterIDHapticIntensityControl
                        value:float_intensity
                 relativeTime:0],
      [[CHHapticDynamicParameter alloc]
          initWithParameterID:CHHapticDynamicParameterIDHapticSharpnessControl
                        value:kDefaultHapticSharpness
                 relativeTime:0]
    ];

    if ([player sendParameters:params atTime:0 error:&error]) {
      return;
    }
    [player stopAtTime:0 error:nil];
    player = nil;
  }

  player = CreateContinuousPlayer(intensity, engine);
}

void SetOptionalButton(Gamepad& pad,
                       int button_index,
                       GCControllerButtonInput* button) {
  if (button) {
    pad.buttons[button_index].pressed = button.isPressed;
    pad.buttons[button_index].value = button.value;
  } else {
    pad.buttons[button_index].pressed = false;
    pad.buttons[button_index].value = 0.0f;
  }
}

}  // namespace

GameControllerGamepad::GameControllerGamepad(GCController* controller)
    : controller_(controller) {
  GCDeviceHaptics* haptics = controller_.haptics;
  if (haptics == nil) {
    return;
  }

  auto setup_locality =
      [haptics](GCHapticsLocality locality) -> CHHapticEngine* {
    CHHapticEngine* engine = [haptics createEngineWithLocality:locality];
    if (engine == nil) {
      return nil;
    }

    engine.playsHapticsOnly = YES;
    engine.autoShutdownEnabled = NO;

    __weak CHHapticEngine* weak_engine = engine;
    engine.stoppedHandler = ^(CHHapticEngineStoppedReason reason) {
      if (reason != CHHapticEngineStoppedReasonGameControllerDisconnect) {
        [weak_engine startAndReturnError:nil];
      }
    };
    engine.resetHandler = ^{
      [weak_engine startAndReturnError:nil];
    };

    return engine;
  };

  bool has_trigger_rumble = [controller.haptics.supportedLocalities
                                containsObject:GCHapticsLocalityLeftTrigger] &&
                            [controller.haptics.supportedLocalities
                                containsObject:GCHapticsLocalityRightTrigger];

  bool has_dual_rumble =
      [haptics.supportedLocalities
          containsObject:GCHapticsLocalityLeftHandle] &&
      [haptics.supportedLocalities containsObject:GCHapticsLocalityRightHandle];

  if (!has_dual_rumble && !has_trigger_rumble) {
    default_haptic_engine_ = setup_locality(GCHapticsLocalityDefault);
  } else {
    if (has_dual_rumble) {
      left_haptic_engine_ = setup_locality(GCHapticsLocalityLeftHandle);
      right_haptic_engine_ = setup_locality(GCHapticsLocalityRightHandle);
    }

    if (has_trigger_rumble) {
      left_trigger_haptic_engine_ =
          setup_locality(GCHapticsLocalityLeftTrigger);
      right_trigger_haptic_engine_ =
          setup_locality(GCHapticsLocalityRightTrigger);
    }
  }
}

GameControllerGamepad::~GameControllerGamepad() = default;

void GameControllerGamepad::UpdateState(Gamepad& pad) {
  auto* extended_gamepad = [controller_ extendedGamepad];
  if (!extended_gamepad) {
    return;
  }

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

  BUTTON(BUTTON_INDEX_START, extended_gamepad.buttonMenu);
  SetOptionalButton(pad, BUTTON_INDEX_META, extended_gamepad.buttonHome);
  SetOptionalButton(pad, BUTTON_INDEX_BACK_SELECT,
                    extended_gamepad.buttonOptions);
  SetOptionalButton(pad, BUTTON_INDEX_LEFT_THUMBSTICK,
                    extended_gamepad.leftThumbstickButton);
  SetOptionalButton(pad, BUTTON_INDEX_RIGHT_THUMBSTICK,
                    extended_gamepad.rightThumbstickButton);

  if (GCXboxGamepad* xbox_gamepad =
          base::apple::ObjCCast<GCXboxGamepad>(extended_gamepad)) {
    // Game controller framework doesn't detect share button presses over
    // USB. Bug filed: FB21568043
    SetOptionalButton(pad, XBOX_SERIES_X_BUTTON_SHARE,
                      xbox_gamepad.buttonShare);
    if (xbox_gamepad.buttonShare) {
      pad.buttons_length = XBOX_SERIES_X_BUTTON_COUNT;
    }
  } else if (GCDualSenseGamepad* dualsense_gamepad =
                 base::apple::ObjCCast<GCDualSenseGamepad>(extended_gamepad)) {
    SetOptionalButton(pad, DUAL_SENSE_BUTTON_TOUCHPAD,
                      dualsense_gamepad.touchpadButton);
    if (dualsense_gamepad.touchpadButton) {
      pad.buttons_length = DUAL_SENSE_BUTTON_COUNT;
    }

    uint32_t touch_count = 0;
    if (ProcessTouchPoint(dualsense_gamepad.touchpadPrimary,
                          primary_touch_state_,
                          pad.touch_events[touch_count])) {
      touch_count++;
    }
    if (ProcessTouchPoint(dualsense_gamepad.touchpadSecondary,
                          secondary_touch_state_,
                          pad.touch_events[touch_count])) {
      touch_count++;
    }
    pad.touch_events_length = touch_count;
  } else if (GCDualShockGamepad* dualshock_gamepad =
                 base::apple::ObjCCast<GCDualShockGamepad>(extended_gamepad)) {
    SetOptionalButton(pad, DUALSHOCK_BUTTON_TOUCHPAD,
                      dualshock_gamepad.touchpadButton);
    if (dualshock_gamepad.touchpadButton) {
      pad.buttons_length = DUALSHOCK_BUTTON_COUNT;
    }

    uint32_t touch_count = 0;
    if (ProcessTouchPoint(dualshock_gamepad.touchpadPrimary,
                          primary_touch_state_,
                          pad.touch_events[touch_count])) {
      touch_count++;
    }
    if (ProcessTouchPoint(dualshock_gamepad.touchpadSecondary,
                          secondary_touch_state_,
                          pad.touch_events[touch_count])) {
      touch_count++;
    }
    pad.touch_events_length = touch_count;
  }

#undef BUTTON
}

void GameControllerGamepad::StartHaptics() {
  if (haptics_started_) {
    return;
  }

  haptics_started_ = true;
  NSError* error = nil;

  if (left_haptic_engine_ &&
      ![left_haptic_engine_ startAndReturnError:&error]) {
    LOG(ERROR) << "Failed to start left haptic engine: "
               << base::SysNSStringToUTF16(error.localizedDescription);
    haptics_started_ = false;
  }

  if (right_haptic_engine_ &&
      ![right_haptic_engine_ startAndReturnError:&error]) {
    LOG(ERROR) << "Failed to start right haptic engine: "
               << base::SysNSStringToUTF16(error.localizedDescription);
    haptics_started_ = false;
  }

  if (left_trigger_haptic_engine_ &&
      ![left_trigger_haptic_engine_ startAndReturnError:&error]) {
    LOG(ERROR) << "Failed to start left trigger haptic engine: "
               << base::SysNSStringToUTF16(error.localizedDescription);
    haptics_started_ = false;
  }

  if (right_trigger_haptic_engine_ &&
      ![right_trigger_haptic_engine_ startAndReturnError:&error]) {
    LOG(ERROR) << "Failed to start right trigger haptic engine: "
               << base::SysNSStringToUTF16(error.localizedDescription);
    haptics_started_ = false;
  }

  if (default_haptic_engine_ &&
      ![default_haptic_engine_ startAndReturnError:&error]) {
    LOG(ERROR) << "Failed to start default haptic engine: "
               << base::SysNSStringToUTF16(error.localizedDescription);
    haptics_started_ = false;
  }
}

void GameControllerGamepad::InitializeStaticData(Gamepad& pad) {
  NSString* vendor_name = controller_.vendorName;
  NSString* ident =
      [NSString stringWithFormat:@"%@ (STANDARD GAMEPAD)",
                                 vendor_name ? vendor_name : @"Unknown"];

  pad.mapping = GamepadMapping::kStandard;
  pad.SetID(base::SysNSStringToUTF16(ident));
  pad.axes_length = AXIS_INDEX_COUNT;
  pad.buttons_length = BUTTON_INDEX_COUNT;
  pad.connected = true;

  // Check for haptics support.
  if (controller_.haptics) {
    bool has_trigger_rumble =
        [controller_.haptics.supportedLocalities
            containsObject:GCHapticsLocalityLeftTrigger] &&
        [controller_.haptics.supportedLocalities
            containsObject:GCHapticsLocalityRightTrigger];

    if (has_trigger_rumble) {
      pad.vibration_actuator.type = GamepadHapticActuatorType::kTriggerRumble;
      pad.vibration_actuator.not_null = true;
    } else {
      // Always fallback to use dual-rumble, even if it's a single-channel
      // actuator.
      pad.vibration_actuator.type = GamepadHapticActuatorType::kDualRumble;
      pad.vibration_actuator.not_null = true;
    }
  }

  if ([controller_ isKindOfClass:[GCDualSenseGamepad class]] ||
      [controller_ isKindOfClass:[GCDualShockGamepad class]]) {
    pad.supports_touch_events_ = true;
  }
}

void GameControllerGamepad::SetVibration(
    mojom::GamepadEffectParametersPtr params) {
  StartHaptics();
  // Assuming strong -> left and weak -> right as in Xbox controllers.
  UpdateHapticPlayer(params->strong_magnitude, left_haptic_engine_,
                     left_haptic_player_);
  UpdateHapticPlayer(params->weak_magnitude, right_haptic_engine_,
                     right_haptic_player_);
  UpdateHapticPlayer(params->left_trigger, left_trigger_haptic_engine_,
                     left_trigger_haptic_player_);
  UpdateHapticPlayer(params->right_trigger, right_trigger_haptic_engine_,
                     right_trigger_haptic_player_);
  // In the case of a single-channel haptic actuator, add the strong and weak
  // magnitude
  UpdateHapticPlayer(
      std::clamp(params->strong_magnitude + params->weak_magnitude, 0.0, 1.0),
      default_haptic_engine_, default_haptic_player_);
}

void GameControllerGamepad::DoShutdown() {
  if (left_haptic_engine_) {
    [left_haptic_engine_ stopWithCompletionHandler:nil];
  }
  if (right_haptic_engine_) {
    [right_haptic_engine_ stopWithCompletionHandler:nil];
  }
  if (left_trigger_haptic_engine_) {
    [left_trigger_haptic_engine_ stopWithCompletionHandler:nil];
  }
  if (right_trigger_haptic_engine_) {
    [right_trigger_haptic_engine_ stopWithCompletionHandler:nil];
  }
  if (default_haptic_engine_) {
    [default_haptic_engine_ stopWithCompletionHandler:nil];
  }
}

base::WeakPtr<AbstractHapticGamepad> GameControllerGamepad::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

bool GameControllerGamepad::ProcessTouchPoint(
    GCControllerDirectionPad* dpad,
    GameControllerGamepad::TouchState& state,
    GamepadTouch& touch) {
  // Check if the touchpad is being touched
  if (dpad.xAxis.value == 0.0f && dpad.yAxis.value == 0.0f) {
    state.active = false;
    return false;
  }

  // If the touch just started, assign it a new unique id
  if (!state.active) {
    state.active = true;
    state.id = next_touch_id_++;

    if (!initial_touch_id_.has_value()) {
      initial_touch_id_ = state.id;
    }
  }

  touch.touch_id = state.id - initial_touch_id_.value();
  touch.surface_id = 0;
  touch.surface_width = kSonyTouchDimensionX;
  touch.surface_height = kSonyTouchDimensionY;
  touch.has_surface_dimensions = true;

  // Normalization: GC framework provides [-1, 1].
  // Coordinate System: -1.0 is Top/Left, 1.0 is Bottom/Right.
  // Apple's yAxis is "up-positive" (1.0 is top), so we negate it.
  touch.x = dpad.xAxis.value;
  touch.y = -dpad.yAxis.value;

  return true;
}

}  // namespace device
