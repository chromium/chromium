// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/midi/midi_input_port_android.h"

#include "base/android/jni_array.h"
#include "base/time/time.h"
#include "media/midi/midi_jni_headers/MidiInputPortAndroid_jni.h"

using base::android::JavaParamRef;

namespace midi {

MidiInputPortAndroid::MidiInputPortAndroid(JNIEnv* env,
                                           jobject raw,
                                           Delegate* delegate)
    : raw_port_(env, raw), delegate_(delegate) {}

MidiInputPortAndroid::~MidiInputPortAndroid() {
  Close();
}

bool MidiInputPortAndroid::Open() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_MidiInputPortAndroid_open(env, raw_port_,
                                        reinterpret_cast<jlong>(this));
}

void MidiInputPortAndroid::Close() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_MidiInputPortAndroid_close(env, raw_port_);
}

void MidiInputPortAndroid::OnData(JNIEnv* env,
                                  const JavaParamRef<jbyteArray>& data,
                                  jint offset,
                                  jint size,
                                  jlong timestamp) {
  std::vector<uint8_t> bytes;
  base::android::JavaByteArrayToByteVector(env, data, &bytes);

  if (size == 0) {
    return;
  }

  // TimeTick's internal value is in microseconds, |timestamp| is in
  // nanoseconds. Both are monotonic.
  base::TimeTicks timestamp_to_pass = base::TimeTicks::FromInternalValue(
      timestamp / base::TimeTicks::kNanosecondsPerMicrosecond);
  delegate_->OnReceivedData(this, &bytes[offset], size, timestamp_to_pass);
}

}  // namespace midi
