// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_ANDROID_AUDIO_DEVICE_H_
#define MEDIA_AUDIO_ANDROID_AUDIO_DEVICE_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "media/audio/android/audio_device_id.h"
#include "media/audio/android/audio_device_type.h"
#include "media/base/media_export.h"

namespace media::android {

class MEDIA_EXPORT AudioDevice {
 public:
  AudioDevice(AudioDeviceId id,
              AudioDeviceType type,
              std::optional<std::string> name,
              std::optional<std::vector<int>> sample_rates);

  AudioDevice(const AudioDevice& other);
  AudioDevice& operator=(const AudioDevice&);
  AudioDevice(AudioDevice&& other);
  AudioDevice& operator=(AudioDevice&&);

  ~AudioDevice();

  static AudioDevice Default();

  bool IsDefault() const;

  AudioDeviceId GetId() const { return id_; }
  AudioDeviceType GetType() const { return type_; }
  std::optional<std::string_view> GetName() const { return name_; }

  // Returns information about the sample rates supported by the default device.
  // If equal to `std::nullopt`, the supported sample rates are unknown.
  // Otherwise, if empty, arbitrary sample rates are supported. Otherwise,
  // represents the list of sample rates supported by the device.
  const std::optional<std::vector<int>>& GetSampleRates() const {
    return sample_rates_;
  }

  // Returns the associated SCO device, or `std::nullopt` if there is no
  // associated SCO device.
  std::optional<AudioDevice> GetAssociatedScoDevice() const;

  // Associates this `AudioDevice`, expected to be a Bluetooth A2DP device, with
  // a Bluetooth SCO device. Although Android treats these two device types as
  // separate, when two outputs of these types coexist, they correspond with the
  // same physical Bluetooth Classic device, and only one of them will be
  // functional at a given time. Thus, it is more appropriate and more intuitive
  // to the user to group them as a single device.
  void SetAssociatedScoDevice(std::unique_ptr<const AudioDevice> sco_device);

 private:
  AudioDeviceId id_;
  AudioDeviceType type_;
  std::optional<std::string> name_;
  std::optional<std::vector<int>> sample_rates_;

  std::unique_ptr<const AudioDevice> associated_sco_device_;
};

}  // namespace media::android

#endif  // MEDIA_AUDIO_ANDROID_AUDIO_DEVICE_H_
