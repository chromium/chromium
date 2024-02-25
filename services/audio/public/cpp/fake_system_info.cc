// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/public/cpp/fake_system_info.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "services/audio/service.h"

namespace audio {

FakeSystemInfo::FakeSystemInfo() {}

FakeSystemInfo::~FakeSystemInfo() {}

// static
void FakeSystemInfo::OverrideGlobalBinderForAudioService(
    FakeSystemInfo* fake_system_info) {
  Service::SetSystemInfoBinderForTesting(base::BindRepeating(
      &FakeSystemInfo::Bind, base::Unretained(fake_system_info)));
}

// static
void FakeSystemInfo::ClearGlobalBinderForAudioService() {
  Service::SetSystemInfoBinderForTesting(base::NullCallback());
}

void FakeSystemInfo::GetInputStreamParameters(
    const std::string& device_id,
    GetInputStreamParametersCallback callback) {
  std::move(callback).Run(std::nullopt);
}

void FakeSystemInfo::GetOutputStreamParameters(
    const std::string& device_id,
    GetOutputStreamParametersCallback callback) {
  std::move(callback).Run(std::nullopt);
}

void FakeSystemInfo::HasInputDevices(HasInputDevicesCallback callback) {
  std::move(callback).Run(false);
}

void FakeSystemInfo::HasOutputDevices(HasOutputDevicesCallback callback) {
  std::move(callback).Run(false);
}

void FakeSystemInfo::GetInputDeviceDescriptions(
    GetInputDeviceDescriptionsCallback callback) {
  std::move(callback).Run(media::AudioDeviceDescriptions());
}

void FakeSystemInfo::GetOutputDeviceDescriptions(
    GetOutputDeviceDescriptionsCallback callback) {
  std::move(callback).Run(media::AudioDeviceDescriptions());
}

void FakeSystemInfo::GetAssociatedOutputDeviceID(
    const std::string& input_device_id,
    GetAssociatedOutputDeviceIDCallback callback) {
  std::move(callback).Run(std::nullopt);
}

void FakeSystemInfo::GetInputDeviceInfo(const std::string& input_device_id,
                                        GetInputDeviceInfoCallback callback) {
  std::move(callback).Run(std::nullopt, std::nullopt);
}

void FakeSystemInfo::Bind(mojo::PendingReceiver<mojom::SystemInfo> receiver) {
  receivers_.Add(this, std::move(receiver));
}

}  // namespace audio
