// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/audio.h"

#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_Audio_1_0>() {
  return PPB_AUDIO_INTERFACE_1_0;
}

template <> const char* interface_name<PPB_Audio_1_1>() {
  return PPB_AUDIO_INTERFACE_1_1;
}

}  // namespace

Audio::Audio(const InstanceHandle& instance,
             const AudioConfig& config,
             PPB_Audio_Callback callback,
             void* user_data)
    : config_(config),
      use_1_0_interface_(false) {
  if (has_interface<PPB_Audio_1_1>()) {
    PassRefFromConstructor(get_interface<PPB_Audio_1_1>()->Create(
        instance.pp_instance(), config.pp_resource(), callback, user_data));
  }
}

Audio::Audio(const InstanceHandle& instance,
             const AudioConfig& config,
             PPB_Audio_Callback_1_0 callback,
             void* user_data)
    : config_(config),
      use_1_0_interface_(true) {
  if (has_interface<PPB_Audio_1_0>()) {
    PassRefFromConstructor(get_interface<PPB_Audio_1_0>()->Create(
        instance.pp_instance(), config.pp_resource(), callback, user_data));
  }
}

bool Audio::StartPlayback() {
  if (has_interface<PPB_Audio_1_1>() && !use_1_0_interface_) {
    return PP_ToBool(get_interface<PPB_Audio_1_1>()->StartPlayback(
        pp_resource()));
  }
  if (has_interface<PPB_Audio_1_0>()) {
    return PP_ToBool(get_interface<PPB_Audio_1_0>()->StartPlayback(
        pp_resource()));
  }
  return false;
}

bool Audio::StopPlayback() {
  if (has_interface<PPB_Audio_1_1>() && !use_1_0_interface_) {
    return PP_ToBool(get_interface<PPB_Audio_1_1>()->StopPlayback(
        pp_resource()));
  }
  if (has_interface<PPB_Audio_1_0>()) {
    return PP_ToBool(get_interface<PPB_Audio_1_0>()->StopPlayback(
        pp_resource()));
  }
  return false;
}

}  // namespace pp
