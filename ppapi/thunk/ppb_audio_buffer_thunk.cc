// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From ppb_audio_buffer.idl modified Wed Jan 27 17:10:16 2016.

#include <stdint.h>

#include "base/logging.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_audio_buffer.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppapi_thunk_export.h"
#include "ppapi/thunk/ppb_audio_buffer_api.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Bool IsAudioBuffer(PP_Resource resource) {
  VLOG(4) << "PPB_AudioBuffer::IsAudioBuffer()";
  EnterResource<PPB_AudioBuffer_API> enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

PP_TimeDelta GetTimestamp(PP_Resource buffer) {
  VLOG(4) << "PPB_AudioBuffer::GetTimestamp()";
  EnterResource<PPB_AudioBuffer_API> enter(buffer, true);
  if (enter.failed())
    return 0.0;
  return enter.object()->GetTimestamp();
}

void SetTimestamp(PP_Resource buffer, PP_TimeDelta timestamp) {
  VLOG(4) << "PPB_AudioBuffer::SetTimestamp()";
  EnterResource<PPB_AudioBuffer_API> enter(buffer, true);
  if (enter.failed())
    return;
  enter.object()->SetTimestamp(timestamp);
}

PP_AudioBuffer_SampleRate GetSampleRate(PP_Resource buffer) {
  VLOG(4) << "PPB_AudioBuffer::GetSampleRate()";
  EnterResource<PPB_AudioBuffer_API> enter(buffer, true);
  if (enter.failed())
    return PP_AUDIOBUFFER_SAMPLERATE_UNKNOWN;
  return enter.object()->GetSampleRate();
}

PP_AudioBuffer_SampleSize GetSampleSize(PP_Resource buffer) {
  VLOG(4) << "PPB_AudioBuffer::GetSampleSize()";
  EnterResource<PPB_AudioBuffer_API> enter(buffer, true);
  if (enter.failed())
    return PP_AUDIOBUFFER_SAMPLESIZE_UNKNOWN;
  return enter.object()->GetSampleSize();
}

uint32_t GetNumberOfChannels(PP_Resource buffer) {
  VLOG(4) << "PPB_AudioBuffer::GetNumberOfChannels()";
  EnterResource<PPB_AudioBuffer_API> enter(buffer, true);
  if (enter.failed())
    return 0;
  return enter.object()->GetNumberOfChannels();
}

uint32_t GetNumberOfSamples(PP_Resource buffer) {
  VLOG(4) << "PPB_AudioBuffer::GetNumberOfSamples()";
  EnterResource<PPB_AudioBuffer_API> enter(buffer, true);
  if (enter.failed())
    return 0;
  return enter.object()->GetNumberOfSamples();
}

void* GetDataBuffer(PP_Resource buffer) {
  VLOG(4) << "PPB_AudioBuffer::GetDataBuffer()";
  EnterResource<PPB_AudioBuffer_API> enter(buffer, true);
  if (enter.failed())
    return NULL;
  return enter.object()->GetDataBuffer();
}

uint32_t GetDataBufferSize(PP_Resource buffer) {
  VLOG(4) << "PPB_AudioBuffer::GetDataBufferSize()";
  EnterResource<PPB_AudioBuffer_API> enter(buffer, true);
  if (enter.failed())
    return 0;
  return enter.object()->GetDataBufferSize();
}

const PPB_AudioBuffer_0_1 g_ppb_audiobuffer_thunk_0_1 = {
    &IsAudioBuffer,      &GetTimestamp,  &SetTimestamp,
    &GetSampleRate,      &GetSampleSize, &GetNumberOfChannels,
    &GetNumberOfSamples, &GetDataBuffer, &GetDataBufferSize};

}  // namespace

PPAPI_THUNK_EXPORT const PPB_AudioBuffer_0_1* GetPPB_AudioBuffer_0_1_Thunk() {
  return &g_ppb_audiobuffer_thunk_0_1;
}

}  // namespace thunk
}  // namespace ppapi
