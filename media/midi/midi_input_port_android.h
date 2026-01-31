// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MIDI_MIDI_INPUT_PORT_ANDROID_H_
#define MEDIA_MIDI_MIDI_INPUT_PORT_ANDROID_H_

#include <jni.h>
#include <stddef.h>
#include <stdint.h>

#include "base/android/scoped_java_ref.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"

namespace midi {

class MidiInputPortAndroid final {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}
    virtual void OnReceivedData(MidiInputPortAndroid* port,
                                base::span<const uint8_t> data,
                                base::TimeTicks time) = 0;
  };
  MidiInputPortAndroid(JNIEnv* env,
                       const base::android::JavaRef<jobject>& raw,
                       Delegate* delegate);
  ~MidiInputPortAndroid();

  // Returns true when the operation succeeds.
  bool Open();
  void Close();

  // Called by the Java world.
  void OnData(JNIEnv* env,
              const base::android::JavaRef<jbyteArray>& data,
              int32_t offset,
              int32_t size,
              int64_t timestamp);

 private:
  base::android::ScopedJavaGlobalRef<jobject> raw_port_;
  const raw_ptr<Delegate> delegate_;
};

}  // namespace midi

#endif  // MEDIA_MIDI_MIDI_INPUT_PORT_ANDROID_H_
