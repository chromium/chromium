// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_PUBLIC_CPP_FAKE_SYSTEM_INFO_H_
#define SERVICES_AUDIO_PUBLIC_CPP_FAKE_SYSTEM_INFO_H_

#include <string>

#include "base/macros.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/audio/public/mojom/system_info.mojom.h"

namespace audio {

// An instance of this class can be used to override the global binding for
// audio::SystemInfo. By default it behaves as if the system has no audio
// devices. Inherit from it to override the behavior.
class FakeSystemInfo : public mojom::SystemInfo {
 public:
  FakeSystemInfo();
  ~FakeSystemInfo() override;

  static void OverrideGlobalBinderForAudioService(
      FakeSystemInfo* fake_system_info);
  static void ClearGlobalBinderForAudioService();

 protected:
  // audio::mojom::SystemInfo implementation.
  void GetInputStreamParameters(
      const std::string& device_id,
      GetInputStreamParametersCallback callback) override;
  void GetOutputStreamParameters(
      const std::string& device_id,
      GetOutputStreamParametersCallback callback) override;
  void HasInputDevices(HasInputDevicesCallback callback) override;
  void HasOutputDevices(HasOutputDevicesCallback callback) override;
  void GetInputDeviceDescriptions(
      GetInputDeviceDescriptionsCallback callback) override;
  void GetOutputDeviceDescriptions(
      GetOutputDeviceDescriptionsCallback callback) override;
  void GetAssociatedOutputDeviceID(
      const std::string& input_device_id,
      GetAssociatedOutputDeviceIDCallback callback) override;
  void GetInputDeviceInfo(const std::string& input_device_id,
                          GetInputDeviceInfoCallback callback) override;

 private:
  void Bind(mojo::PendingReceiver<mojom::SystemInfo> receiver);

  mojo::ReceiverSet<mojom::SystemInfo> receivers_;
  DISALLOW_COPY_AND_ASSIGN(FakeSystemInfo);
};

}  // namespace audio

#endif  // SERVICES_AUDIO_PUBLIC_CPP_FAKE_SYSTEM_INFO_H_
