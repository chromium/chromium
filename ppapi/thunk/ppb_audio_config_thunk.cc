// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/logging.h"
#include "ppapi/shared_impl/ppb_audio_config_shared.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_audio_config_api.h"
#include "ppapi/thunk/resource_creation_api.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Resource CreateStereo16bit(PP_Instance instance,
                              PP_AudioSampleRate sample_rate,
                              uint32_t sample_frame_count) {
  VLOG(4) << "PPB_AudioConfig::CreateStereo16Bit()";
  EnterResourceCreation enter(instance);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateAudioConfig(instance, sample_rate,
                                              sample_frame_count);
}

uint32_t RecommendSampleFrameCount_1_0(PP_AudioSampleRate sample_rate,
                                       uint32_t requested_sample_frame_count) {
  VLOG(4) << "PPB_AudioConfig::RecommendSampleFrameCount()";
  return PPB_AudioConfig_Shared::RecommendSampleFrameCount_1_0(sample_rate,
      requested_sample_frame_count);
}

uint32_t RecommendSampleFrameCount_1_1(PP_Instance instance,
                                       PP_AudioSampleRate sample_rate,
                                       uint32_t requested_sample_frame_count) {
  VLOG(4) << "PPB_AudioConfig::RecommendSampleFrameCount()";
  EnterInstance enter(instance);
  if (enter.failed())
    return 0;
  return PPB_AudioConfig_Shared::RecommendSampleFrameCount_1_1(instance,
      sample_rate, requested_sample_frame_count);
}


PP_Bool IsAudioConfig(PP_Resource resource) {
  VLOG(4) << "PPB_AudioConfig::IsAudioConfig()";
  EnterResource<PPB_AudioConfig_API> enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

PP_AudioSampleRate GetSampleRate(PP_Resource config_id) {
  VLOG(4) << "PPB_AudioConfig::GetSampleRate()";
  EnterResource<PPB_AudioConfig_API> enter(config_id, true);
  if (enter.failed())
    return PP_AUDIOSAMPLERATE_NONE;
  return enter.object()->GetSampleRate();
}

uint32_t GetSampleFrameCount(PP_Resource config_id) {
  VLOG(4) << "PPB_AudioConfig::GetSampleFrameCount()";
  EnterResource<PPB_AudioConfig_API> enter(config_id, true);
  if (enter.failed())
    return 0;
  return enter.object()->GetSampleFrameCount();
}

PP_AudioSampleRate RecommendSampleRate(PP_Instance instance) {
  VLOG(4) << "PPB_AudioConfig::RecommendSampleRate()";
  EnterInstance enter(instance);
  if (enter.failed())
    return PP_AUDIOSAMPLERATE_NONE;
  return PPB_AudioConfig_Shared::RecommendSampleRate(instance);
}

const PPB_AudioConfig_1_0 g_ppb_audio_config_thunk_1_0 = {
  &CreateStereo16bit,
  &RecommendSampleFrameCount_1_0,
  &IsAudioConfig,
  &GetSampleRate,
  &GetSampleFrameCount
};

const PPB_AudioConfig_1_1 g_ppb_audio_config_thunk_1_1 = {
  &CreateStereo16bit,
  &RecommendSampleFrameCount_1_1,
  &IsAudioConfig,
  &GetSampleRate,
  &GetSampleFrameCount,
  &RecommendSampleRate
};


}  // namespace

const PPB_AudioConfig_1_0* GetPPB_AudioConfig_1_0_Thunk() {
  return &g_ppb_audio_config_thunk_1_0;
}

const PPB_AudioConfig_1_1* GetPPB_AudioConfig_1_1_Thunk() {
  return &g_ppb_audio_config_thunk_1_1;
}

}  // namespace thunk
}  // namespace ppapi
