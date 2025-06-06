// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_ANDROID_AUDIO_DEVICE_H_
#define MEDIA_AUDIO_ANDROID_AUDIO_DEVICE_H_

#include "media/audio/android/audio_device_id.h"
#include "media/audio/android/audio_device_type.h"
#include "media/base/media_export.h"

namespace media::android {

class MEDIA_EXPORT AudioDevice {
 public:
  AudioDevice(AudioDeviceId id, AudioDeviceType type);
  static AudioDevice Default();

  bool IsDefault() const;

  AudioDeviceId GetId() const { return id_; }
  AudioDeviceType GetType() const { return type_; }

  // Returns the associated SCO device, or `std::nullopt` if there is no
  // associated SCO device.
  std::optional<AudioDevice> GetAssociatedScoDevice() const;

  // Associates this `AudioDevice`, expected to be a Bluetooth A2DP device, with
  // a Bluetooth SCO device. Although Android treats these two device types as
  // separate, when two outputs of these types coexist, they correspond with the
  // same physical Bluetooth Classic device, and only one of them will be
  // functional at a given time. Thus, it is more appropriate and more intuitive
  // to the user to group them as a single device.
  void SetAssociatedScoDeviceId(AudioDeviceId sco_device_id);

 private:
  AudioDeviceId id_;
  AudioDeviceType type_;
  std::optional<AudioDeviceId> associated_sco_device_id_;
};

}  // namespace media::android

#endif  // MEDIA_AUDIO_ANDROID_AUDIO_DEVICE_H_
