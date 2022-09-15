// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_PPB_AUDIO_CONFIG_SHARED_H_
#define PPAPI_SHARED_IMPL_PPB_AUDIO_CONFIG_SHARED_H_

#include <stdint.h>

#include "base/compiler_specific.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/thunk/ppb_audio_config_api.h"

namespace ppapi {

const int kBitsPerAudioInputSample = 16;
const int kAudioInputChannels = 1;

// TODO(dalecurtis, yzshen): PPAPI shouldn't hard code these values for all
// clients.
const int kBitsPerAudioOutputSample = 16;
const int kAudioOutputChannels = 2;

class PPAPI_SHARED_EXPORT PPB_AudioConfig_Shared
    : public Resource,
      public thunk::PPB_AudioConfig_API {
 public:
  PPB_AudioConfig_Shared(const PPB_AudioConfig_Shared&) = delete;
  PPB_AudioConfig_Shared& operator=(const PPB_AudioConfig_Shared&) = delete;

  ~PPB_AudioConfig_Shared() override;

  static PP_Resource Create(ResourceObjectType type,
                            PP_Instance instance,
                            PP_AudioSampleRate sample_rate,
                            uint32_t sample_frame_count);
  static uint32_t RecommendSampleFrameCount_1_0(
      PP_AudioSampleRate sample_rate,
      uint32_t request_sample_frame_count);
  static uint32_t RecommendSampleFrameCount_1_1(
      PP_Instance instance,
      PP_AudioSampleRate sample_rate,
      uint32_t request_sample_frame_count);
  static PP_AudioSampleRate RecommendSampleRate(PP_Instance);

  // Resource overrides.
  thunk::PPB_AudioConfig_API* AsPPB_AudioConfig_API() override;

  // PPB_AudioConfig_API implementation.
  PP_AudioSampleRate GetSampleRate() override;
  uint32_t GetSampleFrameCount() override;

 private:
  // You must call Init before using this object.
  PPB_AudioConfig_Shared(ResourceObjectType type, PP_Instance instance);

  // Returns false if the arguments are invalid, the object should not be
  // used in this case.
  bool Init(PP_AudioSampleRate sample_rate, uint32_t sample_frame_count);

  PP_AudioSampleRate sample_rate_;
  uint32_t sample_frame_count_;
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_PPB_AUDIO_CONFIG_SHARED_H_
