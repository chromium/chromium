// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/audio_input_dev.h"

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_AudioInput_Dev_0_3>() {
  return PPB_AUDIO_INPUT_DEV_INTERFACE_0_3;
}

template <> const char* interface_name<PPB_AudioInput_Dev_0_4>() {
  return PPB_AUDIO_INPUT_DEV_INTERFACE_0_4;
}

}  // namespace

AudioInput_Dev::AudioInput_Dev() {
}

AudioInput_Dev::AudioInput_Dev(const InstanceHandle& instance) {
  if (has_interface<PPB_AudioInput_Dev_0_4>()) {
    PassRefFromConstructor(get_interface<PPB_AudioInput_Dev_0_4>()->Create(
        instance.pp_instance()));
  } else if (has_interface<PPB_AudioInput_Dev_0_3>()) {
    PassRefFromConstructor(get_interface<PPB_AudioInput_Dev_0_3>()->Create(
        instance.pp_instance()));
  }
}

AudioInput_Dev::~AudioInput_Dev() {
}

// static
bool AudioInput_Dev::IsAvailable() {
  return has_interface<PPB_AudioInput_Dev_0_4>() ||
         has_interface<PPB_AudioInput_Dev_0_3>();
}

int32_t AudioInput_Dev::EnumerateDevices(
    const CompletionCallbackWithOutput<std::vector<DeviceRef_Dev> >& callback) {
  if (has_interface<PPB_AudioInput_Dev_0_4>()) {
    return get_interface<PPB_AudioInput_Dev_0_4>()->EnumerateDevices(
        pp_resource(), callback.output(), callback.pp_completion_callback());
  }
  if (has_interface<PPB_AudioInput_Dev_0_3>()) {
    return get_interface<PPB_AudioInput_Dev_0_3>()->EnumerateDevices(
        pp_resource(), callback.output(), callback.pp_completion_callback());
  }

  return callback.MayForce(PP_ERROR_NOINTERFACE);
}

int32_t AudioInput_Dev::MonitorDeviceChange(
    PP_MonitorDeviceChangeCallback callback,
    void* user_data) {
  if (has_interface<PPB_AudioInput_Dev_0_4>()) {
    return get_interface<PPB_AudioInput_Dev_0_4>()->MonitorDeviceChange(
        pp_resource(), callback, user_data);
  }
  if (has_interface<PPB_AudioInput_Dev_0_3>()) {
    return get_interface<PPB_AudioInput_Dev_0_3>()->MonitorDeviceChange(
        pp_resource(), callback, user_data);
  }

  return PP_ERROR_NOINTERFACE;
}

int32_t AudioInput_Dev::Open(const DeviceRef_Dev& device_ref,
                             const AudioConfig& config,
                             PPB_AudioInput_Callback audio_input_callback,
                             void* user_data,
                             const CompletionCallback& callback) {
  if (has_interface<PPB_AudioInput_Dev_0_4>()) {
    return get_interface<PPB_AudioInput_Dev_0_4>()->Open(
        pp_resource(), device_ref.pp_resource(), config.pp_resource(),
        audio_input_callback, user_data, callback.pp_completion_callback());
  }

  return callback.MayForce(PP_ERROR_NOINTERFACE);
}

int32_t AudioInput_Dev::Open(
    const DeviceRef_Dev& device_ref,
    const AudioConfig& config,
    PPB_AudioInput_Callback_0_3 audio_input_callback_0_3,
    void* user_data,
    const CompletionCallback& callback) {
  if (has_interface<PPB_AudioInput_Dev_0_3>()) {
    return get_interface<PPB_AudioInput_Dev_0_3>()->Open(
        pp_resource(), device_ref.pp_resource(), config.pp_resource(),
        audio_input_callback_0_3, user_data, callback.pp_completion_callback());
  }

  return callback.MayForce(PP_ERROR_NOINTERFACE);
}

bool AudioInput_Dev::StartCapture() {
  if (has_interface<PPB_AudioInput_Dev_0_4>()) {
    return PP_ToBool(get_interface<PPB_AudioInput_Dev_0_4>()->StartCapture(
        pp_resource()));
  }
  if (has_interface<PPB_AudioInput_Dev_0_3>()) {
    return PP_ToBool(get_interface<PPB_AudioInput_Dev_0_3>()->StartCapture(
        pp_resource()));
  }

  return false;
}

bool AudioInput_Dev::StopCapture() {
  if (has_interface<PPB_AudioInput_Dev_0_4>()) {
    return PP_ToBool(get_interface<PPB_AudioInput_Dev_0_4>()->StopCapture(
        pp_resource()));
  }
  if (has_interface<PPB_AudioInput_Dev_0_3>()) {
    return PP_ToBool(get_interface<PPB_AudioInput_Dev_0_3>()->StopCapture(
        pp_resource()));
  }

  return false;
}

void AudioInput_Dev::Close() {
  if (has_interface<PPB_AudioInput_Dev_0_4>()) {
    get_interface<PPB_AudioInput_Dev_0_4>()->Close(pp_resource());
  } else if (has_interface<PPB_AudioInput_Dev_0_3>()) {
    get_interface<PPB_AudioInput_Dev_0_3>()->Close(pp_resource());
  }
}

}  // namespace pp
