// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From dev/ppb_audio_output_dev.idl modified Fri Mar 31 08:08:16 2017.

#include <stdint.h>

#include "base/logging.h"
#include "ppapi/c/dev/ppb_audio_output_dev.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppapi_thunk_export.h"
#include "ppapi/thunk/ppb_audio_output_api.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Resource Create(PP_Instance instance) {
  VLOG(4) << "PPB_AudioOutput_Dev::Create()";
  EnterResourceCreation enter(instance);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateAudioOutput(instance);
}

PP_Bool IsAudioOutput(PP_Resource resource) {
  VLOG(4) << "PPB_AudioOutput_Dev::IsAudioOutput()";
  EnterResource<PPB_AudioOutput_API> enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

int32_t EnumerateDevices(PP_Resource audio_output,
                         struct PP_ArrayOutput output,
                         struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_AudioOutput_Dev::EnumerateDevices()";
  EnterResource<PPB_AudioOutput_API> enter(audio_output, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(
      enter.object()->EnumerateDevices(output, enter.callback()));
}

int32_t MonitorDeviceChange(PP_Resource audio_output,
                            PP_MonitorDeviceChangeCallback callback,
                            void* user_data) {
  VLOG(4) << "PPB_AudioOutput_Dev::MonitorDeviceChange()";
  EnterResource<PPB_AudioOutput_API> enter(audio_output, true);
  if (enter.failed())
    return enter.retval();
  return enter.object()->MonitorDeviceChange(callback, user_data);
}

int32_t Open(PP_Resource audio_output,
             PP_Resource device_ref,
             PP_Resource config,
             PPB_AudioOutput_Callback audio_output_callback,
             void* user_data,
             struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_AudioOutput_Dev::Open()";
  EnterResource<PPB_AudioOutput_API> enter(audio_output, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->Open(
      device_ref, config, audio_output_callback, user_data, enter.callback()));
}

PP_Resource GetCurrentConfig(PP_Resource audio_output) {
  VLOG(4) << "PPB_AudioOutput_Dev::GetCurrentConfig()";
  EnterResource<PPB_AudioOutput_API> enter(audio_output, true);
  if (enter.failed())
    return 0;
  return enter.object()->GetCurrentConfig();
}

PP_Bool StartPlayback(PP_Resource audio_output) {
  VLOG(4) << "PPB_AudioOutput_Dev::StartPlayback()";
  EnterResource<PPB_AudioOutput_API> enter(audio_output, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.object()->StartPlayback();
}

PP_Bool StopPlayback(PP_Resource audio_output) {
  VLOG(4) << "PPB_AudioOutput_Dev::StopPlayback()";
  EnterResource<PPB_AudioOutput_API> enter(audio_output, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.object()->StopPlayback();
}

void Close(PP_Resource audio_output) {
  VLOG(4) << "PPB_AudioOutput_Dev::Close()";
  EnterResource<PPB_AudioOutput_API> enter(audio_output, true);
  if (enter.failed())
    return;
  enter.object()->Close();
}

const PPB_AudioOutput_Dev_0_1 g_ppb_audiooutput_dev_thunk_0_1 = {
    &Create, &IsAudioOutput,    &EnumerateDevices, &MonitorDeviceChange,
    &Open,   &GetCurrentConfig, &StartPlayback,    &StopPlayback,
    &Close};

}  // namespace

PPAPI_THUNK_EXPORT const PPB_AudioOutput_Dev_0_1*
GetPPB_AudioOutput_Dev_0_1_Thunk() {
  return &g_ppb_audiooutput_dev_thunk_0_1;
}

}  // namespace thunk
}  // namespace ppapi
