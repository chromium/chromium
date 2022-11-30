// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MIDI_MIDI_OUTPUT_PORT_ANDROID_H_
#define MEDIA_MIDI_MIDI_OUTPUT_PORT_ANDROID_H_

#include <jni.h>
#include <stdint.h>
#include <vector>

#include "base/android/scoped_java_ref.h"

namespace midi {

class MidiOutputPortAndroid final {
 public:
  MidiOutputPortAndroid(JNIEnv* env, jobject raw);
  ~MidiOutputPortAndroid();

  // Returns the when the operation succeeds or the port is already open.
  bool Open();
  void Close();
  void Send(const std::vector<uint8_t>& data);

 private:
  base::android::ScopedJavaGlobalRef<jobject> raw_port_;
};

}  // namespace midi

#endif  // MEDIA_MIDI_MIDI_OUTPUT_PORT_ANDROID_H_
