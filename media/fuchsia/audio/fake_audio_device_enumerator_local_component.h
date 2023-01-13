// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FUCHSIA_AUDIO_FAKE_AUDIO_DEVICE_ENUMERATOR_LOCAL_COMPONENT_H_
#define MEDIA_FUCHSIA_AUDIO_FAKE_AUDIO_DEVICE_ENUMERATOR_LOCAL_COMPONENT_H_

#include <fuchsia/media/cpp/fidl_test_base.h>
#include <lib/fidl/cpp/binding_set.h>
#include <lib/sys/component/cpp/testing/realm_builder.h>

#include <string>

namespace media {

// A fake AudioDeviceEnumerator for use in tests that use RealmBuilder.
class FakeAudioDeviceEnumeratorLocalComponent final
    : public ::fuchsia::media::testing::AudioDeviceEnumerator_TestBase,
      public ::component_testing::LocalComponentImpl {
 public:
  FakeAudioDeviceEnumeratorLocalComponent();
  FakeAudioDeviceEnumeratorLocalComponent(
      const FakeAudioDeviceEnumeratorLocalComponent&) = delete;
  FakeAudioDeviceEnumeratorLocalComponent& operator=(
      const FakeAudioDeviceEnumeratorLocalComponent&) = delete;
  ~FakeAudioDeviceEnumeratorLocalComponent() override;

  // ::fuchsia::media::AudioDeviceEnumerator_TestBase:
  void GetDevices(GetDevicesCallback callback) override;
  void NotImplemented_(const std::string& name) override;

  // ::component_testing::LocalComponentImpl:
  void OnStart() override;

 private:
  fidl::BindingSet<::fuchsia::media::AudioDeviceEnumerator> bindings_;
};

}  // namespace media

#endif  // MEDIA_FUCHSIA_AUDIO_FAKE_AUDIO_DEVICE_ENUMERATOR_LOCAL_COMPONENT_H_
