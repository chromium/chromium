// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MIDI_MIDI_INPUT_PORT_ANDROID_H_
#define MEDIA_MIDI_MIDI_INPUT_PORT_ANDROID_H_

#include <jni.h>
#include <stddef.h>
#include <stdint.h>

#include "base/android/scoped_java_ref.h"
#include "base/time/time.h"

namespace midi {

class MidiInputPortAndroid final {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}
    virtual void OnReceivedData(MidiInputPortAndroid* port,
                                const uint8_t* data,
                                size_t size,
                                base::TimeTicks time) = 0;
  };
  MidiInputPortAndroid(JNIEnv* env, jobject raw, Delegate* delegate);
  ~MidiInputPortAndroid();

  // Returns true when the operation succeeds.
  bool Open();
  void Close();

  // Called by the Java world.
  void OnData(JNIEnv* env,
              const base::android::JavaParamRef<jbyteArray>& data,
              jint offset,
              jint size,
              jlong timestamp);

 private:
  base::android::ScopedJavaGlobalRef<jobject> raw_port_;
  Delegate* const delegate_;
};

}  // namespace midi

#endif  // MEDIA_MIDI_MIDI_INPUT_PORT_ANDROID_H_
