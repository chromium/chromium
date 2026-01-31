// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/midi/midi_input_port_android.h"

#include "base/android/jni_array.h"
#include "base/time/time.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "media/midi/midi_jni_headers/MidiInputPortAndroid_jni.h"

using base::android::JavaRef;

namespace midi {

MidiInputPortAndroid::MidiInputPortAndroid(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& raw,
    Delegate* delegate)
    : raw_port_(env, raw), delegate_(delegate) {}

MidiInputPortAndroid::~MidiInputPortAndroid() {
  Close();
}

bool MidiInputPortAndroid::Open() {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  return Java_MidiInputPortAndroid_open(env, raw_port_,
                                        reinterpret_cast<int64_t>(this));
}

void MidiInputPortAndroid::Close() {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  Java_MidiInputPortAndroid_close(env, raw_port_);
}

void MidiInputPortAndroid::OnData(JNIEnv* env,
                                  const JavaRef<jbyteArray>& data,
                                  int32_t offset,
                                  int32_t size,
                                  int64_t timestamp) {
  std::vector<uint8_t> bytes;
  base::android::JavaByteArrayToByteVector(env, data, &bytes);

  if (size == 0) {
    return;
  }

  // TimeTick's internal value is in microseconds, |timestamp| is in
  // nanoseconds. Both are monotonic.
  base::TimeTicks timestamp_to_pass = base::TimeTicks::FromInternalValue(
      timestamp / base::TimeTicks::kNanosecondsPerMicrosecond);
  delegate_->OnReceivedData(
      this,
      base::span(bytes).subspan(base::checked_cast<size_t>(offset),
                                base::checked_cast<size_t>(size)),
      timestamp_to_pass);
}

}  // namespace midi

DEFINE_JNI(MidiInputPortAndroid)
