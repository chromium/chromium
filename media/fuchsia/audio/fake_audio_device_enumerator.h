// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FUCHSIA_AUDIO_FAKE_AUDIO_DEVICE_ENUMERATOR_H_
#define MEDIA_FUCHSIA_AUDIO_FAKE_AUDIO_DEVICE_ENUMERATOR_H_

#include <fuchsia/media/cpp/fidl_test_base.h>
#include <lib/vfs/cpp/pseudo_dir.h>

#include "base/fuchsia/scoped_service_binding.h"

namespace media {

class FakeAudioDeviceEnumerator final
    : public fuchsia::media::testing::AudioDeviceEnumerator_TestBase {
 public:
  explicit FakeAudioDeviceEnumerator(vfs::PseudoDir* pseudo_dir);
  ~FakeAudioDeviceEnumerator() override;

  FakeAudioDeviceEnumerator(const FakeAudioDeviceEnumerator&) = delete;
  FakeAudioDeviceEnumerator& operator=(const FakeAudioDeviceEnumerator&) =
      delete;

 private:
  // fuchsia::media::AudioDeviceEnumerator implementation.
  void GetDevices(GetDevicesCallback callback) override;

  // Not-implemented handler.
  void NotImplemented_(const std::string& name) override;

  base::ScopedServiceBinding<fuchsia::media::AudioDeviceEnumerator> binding_;
};

}  // namespace media

#endif  // MEDIA_FUCHSIA_AUDIO_FAKE_AUDIO_DEVICE_ENUMERATOR_H_
