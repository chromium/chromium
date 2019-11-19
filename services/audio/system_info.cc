// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/system_info.h"

#include <utility>

#include "base/trace_event/trace_event.h"
#include "services/service_manager/public/cpp/service_context_ref.h"

namespace audio {

SystemInfo::SystemInfo(media::AudioManager* audio_manager)
    : helper_(audio_manager) {
  DETACH_FROM_SEQUENCE(binding_sequence_checker_);
}

SystemInfo::~SystemInfo() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(binding_sequence_checker_);
}

void SystemInfo::Bind(mojo::PendingReceiver<mojom::SystemInfo> receiver,
                      TracedServiceRef context_ref) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(binding_sequence_checker_);
  receivers_.Add(this, std::move(receiver), std::move(context_ref));
}

void SystemInfo::GetInputStreamParameters(
    const std::string& device_id,
    GetInputStreamParametersCallback callback) {
  TRACE_EVENT1("audio", "audio::SystemInfo::GetInputStreamParameters",
               "device_id", device_id);
  helper_.GetInputStreamParameters(device_id, std::move(callback));
}

void SystemInfo::GetOutputStreamParameters(
    const std::string& device_id,
    GetOutputStreamParametersCallback callback) {
  TRACE_EVENT1("audio", "audio::SystemInfo::GetOutputStreamParameters",
               "device_id", device_id);
  helper_.GetOutputStreamParameters(device_id, std::move(callback));
}

void SystemInfo::HasInputDevices(HasInputDevicesCallback callback) {
  TRACE_EVENT0("audio", "audio::SystemInfo::HasInputDevices");
  helper_.HasInputDevices(std::move(callback));
}

void SystemInfo::HasOutputDevices(HasOutputDevicesCallback callback) {
  TRACE_EVENT0("audio", "audio::SystemInfo::HasOutputDevices");
  helper_.HasOutputDevices(std::move(callback));
}

void SystemInfo::GetInputDeviceDescriptions(
    GetInputDeviceDescriptionsCallback callback) {
  TRACE_EVENT0("audio", "audio::SystemInfo::GetInputDeviceDescriptions");
  helper_.GetDeviceDescriptions(true /* for_input */, std::move(callback));
}

void SystemInfo::GetOutputDeviceDescriptions(
    GetOutputDeviceDescriptionsCallback callback) {
  TRACE_EVENT0("audio", "audio::SystemInfo::GetOutputDeviceDescriptions");
  helper_.GetDeviceDescriptions(false /* for_input */, std::move(callback));
}

void SystemInfo::GetAssociatedOutputDeviceID(
    const std::string& input_device_id,
    GetAssociatedOutputDeviceIDCallback callback) {
  TRACE_EVENT1("audio", "audio::SystemInfo::GetAssociatedOutputDeviceID",
               "input_device_id", input_device_id);
  helper_.GetAssociatedOutputDeviceID(input_device_id, std::move(callback));
}

void SystemInfo::GetInputDeviceInfo(const std::string& input_device_id,
                                    GetInputDeviceInfoCallback callback) {
  TRACE_EVENT1("audio", "audio::SystemInfo::GetInputDeviceInfo",
               "input_device_id", input_device_id);
  helper_.GetInputDeviceInfo(input_device_id, std::move(callback));
}

}  // namespace audio
