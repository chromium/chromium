// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_system.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "media/audio/audio_device_description.h"

namespace media {

// static
AudioSystem::OnDeviceDescriptionsCallback
AudioSystem::WrapCallbackWithDeviceNameLocalization(
    OnDeviceDescriptionsCallback callback) {
  return base::BindOnce(
      [](OnDeviceDescriptionsCallback cb,
         media::AudioDeviceDescriptions descriptions) {
        media::AudioDeviceDescription::LocalizeDeviceDescriptions(
            &descriptions);
        std::move(cb).Run(std::move(descriptions));
      },
      std::move(callback));
}

}  // namespace media
