// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "device/gamepad/gamepad_platform_data_fetcher_android.h"

#include <stddef.h>

#include "base/android/build_info.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "device/gamepad/gamepad_id_list.h"
#include "device/gamepad/haptic_gamepad_android.h"
#include "device/gamepad/public/cpp/gamepad_features.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "device/gamepad/jni_headers/GamepadList_jni.h"

using base::android::AttachCurrentThread;
using base::android::CheckException;
using base::android::ClearException;
using base::android::ConvertJavaStringToUTF8;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace device {

namespace {

// Returns true if |gamepad_id| identifies a gamepad known to be mapped
// correctly on this version of Android. |x_input_type| identifies the flavor of
// XInput used by the gamepad, or kXInputTypeNone if the gamepad is not XInput.

bool HasStandardMappingOnAndroid(GamepadId gamepad_id,
                                 XInputType x_input_type) {
  // Gamepads that are mapped correctly on Android do not require a mapping
  // function to comply with the Standard Gamepad. Entries in this map
  // represent gamepads that have been manually tested and are known to
  // work correctly on recent versions of Android. The key is a GamepadId to
  // identify the gamepad and the value is the earliest
  // base::android::SdkVersion where the gamepad is known to be mapped
  // correctly.
  const base::flat_map<GamepadId, base::android::SdkVersion>
      kManualAssessmentResult = {
          // Stadia Controller USB
          {GamepadId::kGoogleProduct9400,
           base::android::SdkVersion::SDK_VERSION_OREO},
          // Xbox 360 wireless
          {GamepadId::kMicrosoftProduct02a1,
           base::android::SdkVersion::SDK_VERSION_R},
          // Xbox One USB (2015 firmware)
          {GamepadId::kMicrosoftProduct02dd,
           base::android::SdkVersion::SDK_VERSION_R},
          // Xbox One S USB
          {GamepadId::kMicrosoftProduct02ea,
           base::android::SdkVersion::SDK_VERSION_Q},
          // Xbox One S Bluetooth
          {GamepadId::kMicrosoftProduct02fd,
           base::android::SdkVersion::SDK_VERSION_R},
          // Xbox Series X USB
          {GamepadId::kMicrosoftProduct0b12,
           base::android::SdkVersion::SDK_VERSION_R},
          // Xbox Series X Bluetooth
          {GamepadId::kMicrosoftProduct0b13,
           base::android::SdkVersion::SDK_VERSION_Q},
          // Xbox One S Bluetooth (2021 firmware)
          {GamepadId::kMicrosoftProduct0b20,
           base::android::SdkVersion::SDK_VERSION_Q},
          // Xbox Adaptive (2021 firmware)
          {GamepadId::kMicrosoftProduct0b21,
           base::android::SdkVersion::SDK_VERSION_Q},
          // Xbox Elite Series 2 (2021 firmware)
          {GamepadId::kMicrosoftProduct0b22,
           base::android::SdkVersion::SDK_VERSION_Q},
          // Switch Pro Controller
          {GamepadId::kNintendoProduct2009,
           base::android::SdkVersion::SDK_VERSION_R},
      };

  auto find_it = kManualAssessmentResult.find(gamepad_id);

  if (find_it != kManualAssessmentResult.end()) {
    return find_it->second <=
           base::android::BuildInfo::GetInstance()->sdk_int();
  }
  // Assume XInput gamepads are always correctly mapped.
  return x_input_type == kXInputTypeXbox360 ||
         x_input_type == kXInputTypeXboxOne;
}

}  // namespace

GamepadPlatformDataFetcherAndroid::GamepadPlatformDataFetcherAndroid() =
    default;

GamepadPlatformDataFetcherAndroid::~GamepadPlatformDataFetcherAndroid() {
  PauseHint(true);
  for (const auto& pair : vibration_actuators_) {
    pair.second->Shutdown();
  }
}

GamepadSource GamepadPlatformDataFetcherAndroid::source() {
  return Factory::static_source();
}

void GamepadPlatformDataFetcherAndroid::OnAddedToProvider() {
  PauseHint(false);
}

void GamepadPlatformDataFetcherAndroid::SetDualRumbleVibrationActuator(
    int source_id) {
  DCHECK(!base::Contains(vibration_actuators_, source_id));
  vibration_actuators_.emplace(
      source_id, std::make_unique<HapticGamepadAndroid>(source_id));
}

void GamepadPlatformDataFetcherAndroid::TryShutdownDualRumbleVibrationActuator(
    int source_id) {
  auto find_it = vibration_actuators_.find(source_id);
  if (find_it != vibration_actuators_.end()) {
    find_it->second->Shutdown();
    vibration_actuators_.erase(find_it);
  }
}

void GamepadPlatformDataFetcherAndroid::SetVibration(int device_index,
                                                     double strong_magnitude,
                                                     double weak_magnitude) {
  Java_GamepadList_setVibration(base::android::AttachCurrentThread(),
                                device_index, strong_magnitude, weak_magnitude);
}

void GamepadPlatformDataFetcherAndroid::SetZeroVibration(int device_index) {
  Java_GamepadList_setZeroVibration(base::android::AttachCurrentThread(),
                                    device_index);
}

void GamepadPlatformDataFetcherAndroid::GetGamepadData(
    bool devices_changed_hint) {
  TRACE_EVENT0("GAMEPAD", "GetGamepadData");

  JNIEnv* env = AttachCurrentThread();
  if (!env)
    return;

  Java_GamepadList_updateGamepadData(env, reinterpret_cast<intptr_t>(this));
}

void GamepadPlatformDataFetcherAndroid::PauseHint(bool paused) {
  JNIEnv* env = AttachCurrentThread();
  if (!env)
    return;

  Java_GamepadList_setGamepadAPIActive(env, !paused);
}

void GamepadPlatformDataFetcherAndroid::PlayEffect(
    int source_id,
    mojom::GamepadHapticEffectType type,
    mojom::GamepadEffectParametersPtr params,
    mojom::GamepadHapticsManager::PlayVibrationEffectOnceCallback callback,
    scoped_refptr<base::SequencedTaskRunner> callback_runner) {
  auto find_it = vibration_actuators_.find(source_id);
  if (find_it == vibration_actuators_.end()) {
    LOG(ERROR) << "Failed to play vibration effect on a gamepad with no "
                  "vibration actuator";
    RunVibrationCallback(
        std::move(callback), std::move(callback_runner),
        mojom::GamepadHapticsResult::GamepadHapticsResultError);
    return;
  }
  find_it->second->PlayEffect(type, std::move(params), std::move(callback),
                              std::move(callback_runner));
}

void GamepadPlatformDataFetcherAndroid::ResetVibration(
    int source_id,
    mojom::GamepadHapticsManager::ResetVibrationActuatorCallback callback,
    scoped_refptr<base::SequencedTaskRunner> callback_runner) {
  auto find_it = vibration_actuators_.find(source_id);
  if (find_it == vibration_actuators_.end()) {
    LOG(ERROR) << "Failed to reset vibration effect on a gamepad with no "
                  "vibration actuator";
    RunVibrationCallback(
        std::move(callback), std::move(callback_runner),
        mojom::GamepadHapticsResult::GamepadHapticsResultError);
    return;
  }
  find_it->second->ResetVibration(std::move(callback),
                                  std::move(callback_runner));
}

static void JNI_GamepadList_SetGamepadData(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jlong data_fetcher,
    jint index,
    jboolean mapping,
    jboolean connected,
    const JavaParamRef<jstring>& devicename,
    jint vendor_id,
    jint product_id,
    jlong timestamp,
    const JavaParamRef<jfloatArray>& jaxes,
    const JavaParamRef<jfloatArray>& jbuttons,
    jint buttons_length,
    jboolean supports_dual_rumble) {
  DCHECK(data_fetcher);
  GamepadPlatformDataFetcherAndroid* fetcher =
      reinterpret_cast<GamepadPlatformDataFetcherAndroid*>(data_fetcher);
  DCHECK_LT(index, static_cast<int>(Gamepads::kItemsLengthCap));

  // Do not set gamepad parameters for all the gamepad devices that are not
  // attached.
  if (!connected) {
    fetcher->TryShutdownDualRumbleVibrationActuator(index);
    return;
  }

  PadState* state = fetcher->GetPadState(index);

  if (!state)
    return;

  Gamepad& pad = state->data;
  // Is this the first time we've seen this device?
  if (!state->is_initialized) {
    state->is_initialized = true;

    std::string product_name =
        base::android::ConvertJavaStringToUTF8(env, devicename);

    if (!mapping) {
      // The gamepad was assigned the default mapping function. Check if it is
      // known to be mapped correctly with the default mapping function on this
      // version of Android.
      auto gamepad_id = GamepadIdList::Get().GetGamepadId(
          product_name, vendor_id, product_id);
      auto x_input_type =
          GamepadIdList::Get().GetXInputType(vendor_id, product_id);
      mapping = HasStandardMappingOnAndroid(gamepad_id, x_input_type);
    }
    GamepadDataFetcher::UpdateGamepadStrings(product_name, vendor_id,
                                             product_id, mapping, pad);
    if (base::FeatureList::IsEnabled(
            features::kEnableAndroidGamepadVibration)) {
      pad.vibration_actuator.type = GamepadHapticActuatorType::kDualRumble;
      pad.vibration_actuator.not_null = supports_dual_rumble;
      if (supports_dual_rumble) {
        fetcher->SetDualRumbleVibrationActuator(state->source_id);
      }
    }
  }

  pad.connected = true;
  pad.timestamp = GamepadDataFetcher::CurrentTimeInMicroseconds();

  std::vector<float> axes;
  base::android::JavaFloatArrayToFloatVector(env, jaxes, &axes);

  // Set Gamepad::axes_length to the total number of axes on the gamepad device.
  // Only return the first kAxesLengthCap if the axes size captured by
  // GamepadList is larger than kAxesLengthCap.
  pad.axes_length = std::min(static_cast<int>(axes.size()),
                             static_cast<int>(Gamepad::kAxesLengthCap));

  // Copy axes state to the Gamepad axes[].
  for (unsigned int i = 0; i < pad.axes_length; i++) {
    pad.axes[i] = static_cast<double>(axes[i]);
  }

  std::vector<float> buttons;
  base::android::JavaFloatArrayToFloatVector(env, jbuttons, &buttons);

  // Set Gamepad::buttons_length to the total number of buttons on the gamepad
  // device. Only return the first kButtonsLengthCap if buttons_length captured
  // by GamepadList is larger than kButtonsLengthCap.
  pad.buttons_length =
      std::min({static_cast<int>(buttons.size()), buttons_length,
                static_cast<int>(Gamepad::kButtonsLengthCap)});

  // Copy buttons state to the Gamepad buttons[].
  for (unsigned int j = 0; j < pad.buttons_length; j++) {
    pad.buttons[j].pressed =
        buttons[j] > GamepadButton::kDefaultButtonPressedThreshold;
    pad.buttons[j].touched = buttons[j] > 0.0f;
    pad.buttons[j].value = buttons[j];
  }
}

}  // namespace device
