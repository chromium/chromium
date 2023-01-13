// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/fuchsia/audio/fake_audio_device_enumerator_local_component.h"

#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace media {

FakeAudioDeviceEnumeratorLocalComponent::
    FakeAudioDeviceEnumeratorLocalComponent() = default;

FakeAudioDeviceEnumeratorLocalComponent::
    ~FakeAudioDeviceEnumeratorLocalComponent() = default;

void FakeAudioDeviceEnumeratorLocalComponent::GetDevices(
    GetDevicesCallback callback) {
  callback(std::vector<fuchsia::media::AudioDeviceInfo>{
      {
          .name = "input",
          .unique_id = "input",
          .token_id = 1,
          .is_input = true,
          .is_default = true,
      },
      {
          .name = "output",
          .unique_id = "output",
          .token_id = 2,
          .is_input = false,
          .is_default = true,
      },
  });
}

void FakeAudioDeviceEnumeratorLocalComponent::NotImplemented_(
    const std::string& name) {
  FAIL() << "Unexpected call to unimplemented method \"" << name << "\"";
}

void FakeAudioDeviceEnumeratorLocalComponent::OnStart() {
  ASSERT_EQ(outgoing()->AddPublicService(bindings_.GetHandler(this)), ZX_OK);
}

}  // namespace media
