// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/gamepad_platform_data_fetcher_android.h"

#include <stddef.h>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"

#include "device/gamepad/jni_headers/GamepadList_jni.h"

using base::android::AttachCurrentThread;
using base::android::CheckException;
using base::android::ClearException;
using base::android::ConvertJavaStringToUTF8;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace device {

GamepadPlatformDataFetcherAndroid::GamepadPlatformDataFetcherAndroid() {
}

GamepadPlatformDataFetcherAndroid::~GamepadPlatformDataFetcherAndroid() {
  PauseHint(true);
}

GamepadSource GamepadPlatformDataFetcherAndroid::source() {
  return Factory::static_source();
}

void GamepadPlatformDataFetcherAndroid::OnAddedToProvider() {
  PauseHint(false);
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
    jint buttons_length) {
  DCHECK(data_fetcher);
  GamepadPlatformDataFetcherAndroid* fetcher =
      reinterpret_cast<GamepadPlatformDataFetcherAndroid*>(data_fetcher);
  DCHECK_LT(index, static_cast<int>(Gamepads::kItemsLengthCap));

  // Do not set gamepad parameters for all the gamepad devices that are not
  // attached.
  if (!connected)
    return;

  PadState* state = fetcher->GetPadState(index);

  if (!state)
    return;

  Gamepad& pad = state->data;

  // Is this the first time we've seen this device?
  if (!state->is_initialized) {
    state->is_initialized = true;
    // Map the Gamepad DeviceName String to the Gamepad Id. Ideally it should
    // be mapped to vendor and product information but it is only available at
    // kernel level and it can not be queried using class
    // android.hardware.input.InputManager.
    std::string gamepad_id =
        base::android::ConvertJavaStringToUTF8(env, devicename);
    GamepadDataFetcher::UpdateGamepadStrings(gamepad_id, vendor_id, product_id,
                                             mapping, pad);
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
