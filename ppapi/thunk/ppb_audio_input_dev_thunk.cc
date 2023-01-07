// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/logging.h"
#include "ppapi/c/dev/ppb_audio_input_dev.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_audio_input_api.h"
#include "ppapi/thunk/ppb_instance_api.h"
#include "ppapi/thunk/resource_creation_api.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Resource Create(PP_Instance instance) {
  VLOG(4) << "PPB_AudioInput_Dev::Create()";
  EnterResourceCreation enter(instance);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateAudioInput(instance);
}

PP_Bool IsAudioInput(PP_Resource resource) {
  VLOG(4) << "PPB_AudioInput_Dev::IsAudioInput()";
  EnterResource<PPB_AudioInput_API> enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

int32_t EnumerateDevices(PP_Resource audio_input,
                         struct PP_ArrayOutput output,
                         struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_AudioInput_Dev::EnumerateDevices()";
  EnterResource<PPB_AudioInput_API> enter(audio_input, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->EnumerateDevices(output,
                                                          enter.callback()));
}

int32_t MonitorDeviceChange(PP_Resource audio_input,
                            PP_MonitorDeviceChangeCallback callback,
                            void* user_data) {
  VLOG(4) << "PPB_AudioInput_Dev::MonitorDeviceChange()";
  EnterResource<PPB_AudioInput_API> enter(audio_input, true);
  if (enter.failed())
    return enter.retval();
  return enter.object()->MonitorDeviceChange(callback, user_data);
}

int32_t Open_0_3(PP_Resource audio_input,
                 PP_Resource device_ref,
                 PP_Resource config,
                 PPB_AudioInput_Callback_0_3 audio_input_callback,
                 void* user_data,
                 struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_AudioInput_Dev::Open()";
  EnterResource<PPB_AudioInput_API> enter(audio_input, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->Open0_3(device_ref,
                                                 config,
                                                 audio_input_callback,
                                                 user_data,
                                                 enter.callback()));
}

int32_t Open(PP_Resource audio_input,
             PP_Resource device_ref,
             PP_Resource config,
             PPB_AudioInput_Callback audio_input_callback,
             void* user_data,
             struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_AudioInput_Dev::Open()";
  EnterResource<PPB_AudioInput_API> enter(audio_input, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->Open(device_ref,
                                              config,
                                              audio_input_callback,
                                              user_data,
                                              enter.callback()));
}

PP_Resource GetCurrentConfig(PP_Resource audio_input) {
  VLOG(4) << "PPB_AudioInput_Dev::GetCurrentConfig()";
  EnterResource<PPB_AudioInput_API> enter(audio_input, true);
  if (enter.failed())
    return 0;
  return enter.object()->GetCurrentConfig();
}

PP_Bool StartCapture(PP_Resource audio_input) {
  VLOG(4) << "PPB_AudioInput_Dev::StartCapture()";
  EnterResource<PPB_AudioInput_API> enter(audio_input, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.object()->StartCapture();
}

PP_Bool StopCapture(PP_Resource audio_input) {
  VLOG(4) << "PPB_AudioInput_Dev::StopCapture()";
  EnterResource<PPB_AudioInput_API> enter(audio_input, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.object()->StopCapture();
}

void Close(PP_Resource audio_input) {
  VLOG(4) << "PPB_AudioInput_Dev::Close()";
  EnterResource<PPB_AudioInput_API> enter(audio_input, true);
  if (enter.failed())
    return;
  enter.object()->Close();
}

const PPB_AudioInput_Dev_0_3 g_ppb_audioinput_dev_thunk_0_3 = {
  &Create,
  &IsAudioInput,
  &EnumerateDevices,
  &MonitorDeviceChange,
  &Open_0_3,
  &GetCurrentConfig,
  &StartCapture,
  &StopCapture,
  &Close
};

const PPB_AudioInput_Dev_0_4 g_ppb_audioinput_dev_thunk_0_4 = {
  &Create,
  &IsAudioInput,
  &EnumerateDevices,
  &MonitorDeviceChange,
  &Open,
  &GetCurrentConfig,
  &StartCapture,
  &StopCapture,
  &Close
};

}  // namespace

const PPB_AudioInput_Dev_0_3* GetPPB_AudioInput_Dev_0_3_Thunk() {
  return &g_ppb_audioinput_dev_thunk_0_3;
}

const PPB_AudioInput_Dev_0_4* GetPPB_AudioInput_Dev_0_4_Thunk() {
  return &g_ppb_audioinput_dev_thunk_0_4;
}

}  // namespace thunk
}  // namespace ppapi
