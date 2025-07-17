// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MIDI_MIDI_DEVICE_ANDROID_H_
#define MEDIA_MIDI_MIDI_DEVICE_ANDROID_H_

#include <jni.h>
#include <memory>
#include <string>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "media/midi/midi_input_port_android.h"

namespace midi {

class MidiOutputPortAndroid;

class MidiDeviceAndroid final {
 public:
  MidiDeviceAndroid(JNIEnv* env,
                    const base::android::JavaRef<jobject>& raw_device,
                    MidiInputPortAndroid::Delegate* delegate);
  ~MidiDeviceAndroid();

  std::string GetManufacturer();
  std::string GetProductName();
  std::string GetDeviceVersion();

  const std::vector<std::unique_ptr<MidiInputPortAndroid>>& input_ports()
      const {
    return input_ports_;
  }
  const std::vector<std::unique_ptr<MidiOutputPortAndroid>>& output_ports()
      const {
    return output_ports_;
  }
  bool HasRawDevice(JNIEnv* env, jobject raw_device) const {
    return env->IsSameObject(raw_device_.obj(), raw_device);
  }

 private:
  base::android::ScopedJavaGlobalRef<jobject> raw_device_;
  std::vector<std::unique_ptr<MidiInputPortAndroid>> input_ports_;
  std::vector<std::unique_ptr<MidiOutputPortAndroid>> output_ports_;
};

}  // namespace midi

#endif  // MEDIA_MIDI_MIDI_DEVICE_ANDROID_H_
