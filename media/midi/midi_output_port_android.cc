// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/midi/midi_output_port_android.h"

#include "base/android/jni_array.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "media/midi/midi_jni_headers/MidiOutputPortAndroid_jni.h"

using base::android::ScopedJavaLocalRef;

namespace midi {

MidiOutputPortAndroid::MidiOutputPortAndroid(JNIEnv* env, jobject raw)
    : raw_port_(env, raw) {}
MidiOutputPortAndroid::~MidiOutputPortAndroid() {
  Close();
}

bool MidiOutputPortAndroid::Open() {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  return Java_MidiOutputPortAndroid_open(env, raw_port_);
}

void MidiOutputPortAndroid::Close() {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  Java_MidiOutputPortAndroid_close(env, raw_port_);
}

void MidiOutputPortAndroid::Send(const std::vector<uint8_t>& data) {
  if (data.size() == 0) {
    return;
  }

  JNIEnv* env = jni_zero::AttachCurrentThread();
  ScopedJavaLocalRef<jbyteArray> data_to_pass =
      base::android::ToJavaByteArray(env, data);

  Java_MidiOutputPortAndroid_send(env, raw_port_, data_to_pass);
}

}  // namespace midi
