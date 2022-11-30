// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/fuchsia/audio/fake_audio_device_enumerator.h"

#include "base/logging.h"

namespace media {

FakeAudioDeviceEnumerator::FakeAudioDeviceEnumerator(vfs::PseudoDir* pseudo_dir)
    : binding_(pseudo_dir, this) {}

FakeAudioDeviceEnumerator::~FakeAudioDeviceEnumerator() = default;

void FakeAudioDeviceEnumerator::GetDevices(GetDevicesCallback callback) {
  std::vector<fuchsia::media::AudioDeviceInfo> result = {
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
  };
  callback(std::move(result));
}

void FakeAudioDeviceEnumerator::NotImplemented_(const std::string& name) {
  LOG(FATAL) << "Reached non-implemented " << name;
}

}  // namespace media
