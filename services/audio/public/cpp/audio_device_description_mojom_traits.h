// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_PUBLIC_CPP_AUDIO_DEVICE_DESCRIPTION_MOJOM_TRAITS_H_
#define SERVICES_AUDIO_PUBLIC_CPP_AUDIO_DEVICE_DESCRIPTION_MOJOM_TRAITS_H_

#include <string>

#include "media/audio/audio_device_description.h"
#include "services/audio/public/mojom/system_info.mojom.h"

namespace mojo {

template <>
struct StructTraits<audio::mojom::AudioDeviceDescriptionDataView,
                    media::AudioDeviceDescription> {
  static std::string device_name(const media::AudioDeviceDescription& input) {
    return input.device_name;
  }
  static std::string unique_id(const media::AudioDeviceDescription& input) {
    return input.unique_id;
  }
  static std::string group_id(const media::AudioDeviceDescription& input) {
    return input.group_id;
  }
  static bool is_system_default(const media::AudioDeviceDescription& input) {
    return input.is_system_default;
  }
  static bool is_communications_device(
      const media::AudioDeviceDescription& input) {
    return input.is_communications_device;
  }

  static bool Read(audio::mojom::AudioDeviceDescriptionDataView data,
                   media::AudioDeviceDescription* output);
};

}  // namespace mojo

#endif  // SERVICES_AUDIO_PUBLIC_CPP_AUDIO_DEVICE_DESCRIPTION_MOJOM_TRAITS_H_
