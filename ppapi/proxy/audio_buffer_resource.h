// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_AUDIO_BUFFER_RESOURCE_H_
#define PPAPI_PROXY_AUDIO_BUFFER_RESOURCE_H_

#include <stdint.h>

#include "base/compiler_specific.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/thunk/ppb_audio_buffer_api.h"

namespace ppapi {

union MediaStreamBuffer;

namespace proxy {

class PPAPI_PROXY_EXPORT AudioBufferResource
    : public Resource,
      public thunk::PPB_AudioBuffer_API {
 public:
  AudioBufferResource(PP_Instance instance,
                     int32_t index,
                     MediaStreamBuffer* buffer);

  AudioBufferResource(const AudioBufferResource&) = delete;
  AudioBufferResource& operator=(const AudioBufferResource&) = delete;

  ~AudioBufferResource() override;

  // PluginResource overrides:
  thunk::PPB_AudioBuffer_API* AsPPB_AudioBuffer_API() override;

  // PPB_AudioBuffer_API overrides:
  PP_TimeDelta GetTimestamp() override;
  void SetTimestamp(PP_TimeDelta timestamp) override;
  PP_AudioBuffer_SampleRate GetSampleRate() override;
  PP_AudioBuffer_SampleSize GetSampleSize() override;
  uint32_t GetNumberOfChannels() override;
  uint32_t GetNumberOfSamples() override;
  void* GetDataBuffer() override;
  uint32_t GetDataBufferSize() override;
  MediaStreamBuffer* GetBuffer() override;
  int32_t GetBufferIndex() override;
  void Invalidate() override;

  // Buffer index
  int32_t index_;

  MediaStreamBuffer* buffer_;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_AUDIO_BUFFER_RESOURCE_H_
