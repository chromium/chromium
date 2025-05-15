// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_ANDROID_AUDIO_DEVICE_H_
#define MEDIA_AUDIO_ANDROID_AUDIO_DEVICE_H_

#include "media/audio/android/audio_device_id.h"
#include "media/audio/android/audio_device_type.h"

namespace media::android {

class AudioDevice {
 public:
  AudioDevice(AudioDeviceId id, AudioDeviceType type);
  static AudioDevice Default();

  bool IsDefault() const;

  AudioDeviceId GetId() const { return id_; }
  AudioDeviceType GetType() const { return type_; }

 private:
  AudioDeviceId id_;
  AudioDeviceType type_;
};

}  // namespace media::android

#endif  // MEDIA_AUDIO_ANDROID_AUDIO_DEVICE_H_
