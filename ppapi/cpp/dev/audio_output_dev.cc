// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/audio_output_dev.h"

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <>
const char* interface_name<PPB_AudioOutput_Dev_0_1>() {
  return PPB_AUDIO_OUTPUT_DEV_INTERFACE_0_1;
}

}  // namespace

AudioOutput_Dev::AudioOutput_Dev() {}

AudioOutput_Dev::AudioOutput_Dev(const InstanceHandle& instance) {
  if (has_interface<PPB_AudioOutput_Dev_0_1>()) {
    PassRefFromConstructor(get_interface<PPB_AudioOutput_Dev_0_1>()->Create(
        instance.pp_instance()));
  }
}

AudioOutput_Dev::~AudioOutput_Dev() {}

// static
bool AudioOutput_Dev::IsAvailable() {
  return has_interface<PPB_AudioOutput_Dev_0_1>();
}

int32_t AudioOutput_Dev::EnumerateDevices(
    const CompletionCallbackWithOutput<std::vector<DeviceRef_Dev> >& callback) {
  if (has_interface<PPB_AudioOutput_Dev_0_1>()) {
    return get_interface<PPB_AudioOutput_Dev_0_1>()->EnumerateDevices(
        pp_resource(), callback.output(), callback.pp_completion_callback());
  }

  return callback.MayForce(PP_ERROR_NOINTERFACE);
}

int32_t AudioOutput_Dev::MonitorDeviceChange(
    PP_MonitorDeviceChangeCallback callback,
    void* user_data) {
  if (has_interface<PPB_AudioOutput_Dev_0_1>()) {
    return get_interface<PPB_AudioOutput_Dev_0_1>()->MonitorDeviceChange(
        pp_resource(), callback, user_data);
  }

  return PP_ERROR_NOINTERFACE;
}

int32_t AudioOutput_Dev::Open(const DeviceRef_Dev& device_ref,
                              const AudioConfig& config,
                              PPB_AudioOutput_Callback audio_output_callback,
                              void* user_data,
                              const CompletionCallback& callback) {
  if (has_interface<PPB_AudioOutput_Dev_0_1>()) {
    return get_interface<PPB_AudioOutput_Dev_0_1>()->Open(
        pp_resource(), device_ref.pp_resource(), config.pp_resource(),
        audio_output_callback, user_data, callback.pp_completion_callback());
  }

  return callback.MayForce(PP_ERROR_NOINTERFACE);
}

bool AudioOutput_Dev::StartPlayback() {
  if (has_interface<PPB_AudioOutput_Dev_0_1>()) {
    return PP_ToBool(
        get_interface<PPB_AudioOutput_Dev_0_1>()->StartPlayback(pp_resource()));
  }

  return false;
}

bool AudioOutput_Dev::StopPlayback() {
  if (has_interface<PPB_AudioOutput_Dev_0_1>()) {
    return PP_ToBool(
        get_interface<PPB_AudioOutput_Dev_0_1>()->StopPlayback(pp_resource()));
  }

  return false;
}

void AudioOutput_Dev::Close() {
  if (has_interface<PPB_AudioOutput_Dev_0_1>()) {
    get_interface<PPB_AudioOutput_Dev_0_1>()->Close(pp_resource());
  }
}

}  // namespace pp
