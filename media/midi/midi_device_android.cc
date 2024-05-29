// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/midi/midi_device_android.h"

#include <memory>
#include <string>

#include "base/android/jni_string.h"
#include "media/midi/midi_output_port_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "media/midi/midi_jni_headers/MidiDeviceAndroid_jni.h"

using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace midi {

namespace {

std::string ConvertMaybeJavaString(JNIEnv* env,
                                   const base::android::JavaRef<jstring>& str) {
  if (!str.obj())
    return std::string();
  return base::android::ConvertJavaStringToUTF8(str);
}
}

MidiDeviceAndroid::MidiDeviceAndroid(JNIEnv* env,
                                     const JavaRef<jobject>& raw_device,
                                     MidiInputPortAndroid::Delegate* delegate)
    : raw_device_(raw_device) {
  ScopedJavaLocalRef<jobjectArray> raw_input_ports =
      Java_MidiDeviceAndroid_getInputPorts(env, raw_device);
  for (auto j_port : raw_input_ports.ReadElements<jobject>()) {
    input_ports_.push_back(
        std::make_unique<MidiInputPortAndroid>(env, j_port.obj(), delegate));
  }

  ScopedJavaLocalRef<jobjectArray> raw_output_ports =
      Java_MidiDeviceAndroid_getOutputPorts(env, raw_device);
  for (auto j_port : raw_output_ports.ReadElements<jobject>()) {
    output_ports_.push_back(
        std::make_unique<MidiOutputPortAndroid>(env, j_port.obj()));
  }
}

MidiDeviceAndroid::~MidiDeviceAndroid() {}

std::string MidiDeviceAndroid::GetManufacturer() {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  return ConvertMaybeJavaString(
      env, Java_MidiDeviceAndroid_getManufacturer(env, raw_device_));
}

std::string MidiDeviceAndroid::GetProductName() {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  return ConvertMaybeJavaString(
      env, Java_MidiDeviceAndroid_getProduct(env, raw_device_));
}

std::string MidiDeviceAndroid::GetDeviceVersion() {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  return ConvertMaybeJavaString(
      env, Java_MidiDeviceAndroid_getVersion(env, raw_device_));
}

}  // namespace midi
