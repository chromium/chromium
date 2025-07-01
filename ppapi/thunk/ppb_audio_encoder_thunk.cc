// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From ppb_audio_encoder.idl modified Wed Jan 27 17:39:22 2016.

#include <stdint.h>

#include "base/logging.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_audio_encoder.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppapi_thunk_export.h"
#include "ppapi/thunk/ppb_audio_encoder_api.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Resource Create(PP_Instance instance) {
  VLOG(4) << "PPB_AudioEncoder::Create()";
  EnterResourceCreation enter(instance);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateAudioEncoder(instance);
}

PP_Bool IsAudioEncoder(PP_Resource resource) {
  VLOG(4) << "PPB_AudioEncoder::IsAudioEncoder()";
  EnterResource<PPB_AudioEncoder_API> enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

int32_t GetSupportedProfiles(PP_Resource audio_encoder,
                             struct PP_ArrayOutput output,
                             struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_AudioEncoder::GetSupportedProfiles()";
  EnterResource<PPB_AudioEncoder_API> enter(audio_encoder, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(
      enter.object()->GetSupportedProfiles(output, enter.callback()));
}

int32_t Initialize(PP_Resource audio_encoder,
                   uint32_t channels,
                   PP_AudioBuffer_SampleRate input_sample_rate,
                   PP_AudioBuffer_SampleSize input_sample_size,
                   PP_AudioProfile output_profile,
                   uint32_t initial_bitrate,
                   PP_HardwareAcceleration acceleration,
                   struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_AudioEncoder::Initialize()";
  EnterResource<PPB_AudioEncoder_API> enter(audio_encoder, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->Initialize(
      channels, input_sample_rate, input_sample_size, output_profile,
      initial_bitrate, acceleration, enter.callback()));
}

int32_t GetNumberOfSamples(PP_Resource audio_encoder) {
  VLOG(4) << "PPB_AudioEncoder::GetNumberOfSamples()";
  EnterResource<PPB_AudioEncoder_API> enter(audio_encoder, true);
  if (enter.failed())
    return enter.retval();
  return enter.object()->GetNumberOfSamples();
}

int32_t GetBuffer(PP_Resource audio_encoder,
                  PP_Resource* audio_buffer,
                  struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_AudioEncoder::GetBuffer()";
  EnterResource<PPB_AudioEncoder_API> enter(audio_encoder, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(
      enter.object()->GetBuffer(audio_buffer, enter.callback()));
}

int32_t Encode(PP_Resource audio_encoder,
               PP_Resource audio_buffer,
               struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_AudioEncoder::Encode()";
  EnterResource<PPB_AudioEncoder_API> enter(audio_encoder, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(
      enter.object()->Encode(audio_buffer, enter.callback()));
}

int32_t GetBitstreamBuffer(PP_Resource audio_encoder,
                           struct PP_AudioBitstreamBuffer* bitstream_buffer,
                           struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_AudioEncoder::GetBitstreamBuffer()";
  EnterResource<PPB_AudioEncoder_API> enter(audio_encoder, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(
      enter.object()->GetBitstreamBuffer(bitstream_buffer, enter.callback()));
}

void RecycleBitstreamBuffer(
    PP_Resource audio_encoder,
    const struct PP_AudioBitstreamBuffer* bitstream_buffer) {
  VLOG(4) << "PPB_AudioEncoder::RecycleBitstreamBuffer()";
  EnterResource<PPB_AudioEncoder_API> enter(audio_encoder, true);
  if (enter.failed())
    return;
  enter.object()->RecycleBitstreamBuffer(bitstream_buffer);
}

void RequestBitrateChange(PP_Resource audio_encoder, uint32_t bitrate) {
  VLOG(4) << "PPB_AudioEncoder::RequestBitrateChange()";
  EnterResource<PPB_AudioEncoder_API> enter(audio_encoder, true);
  if (enter.failed())
    return;
  enter.object()->RequestBitrateChange(bitrate);
}

void Close(PP_Resource audio_encoder) {
  VLOG(4) << "PPB_AudioEncoder::Close()";
  EnterResource<PPB_AudioEncoder_API> enter(audio_encoder, true);
  if (enter.failed())
    return;
  enter.object()->Close();
}

const PPB_AudioEncoder_0_1 g_ppb_audioencoder_thunk_0_1 = {
    &Create,
    &IsAudioEncoder,
    &GetSupportedProfiles,
    &Initialize,
    &GetNumberOfSamples,
    &GetBuffer,
    &Encode,
    &GetBitstreamBuffer,
    &RecycleBitstreamBuffer,
    &RequestBitrateChange,
    &Close};

}  // namespace

PPAPI_THUNK_EXPORT const PPB_AudioEncoder_0_1* GetPPB_AudioEncoder_0_1_Thunk() {
  return &g_ppb_audioencoder_thunk_0_1;
}

}  // namespace thunk
}  // namespace ppapi
