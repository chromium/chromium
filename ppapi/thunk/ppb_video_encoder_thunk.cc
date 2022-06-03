// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From ppb_video_encoder.idl modified Mon May 18 12:43:25 2015.

#include <stdint.h>

#include "base/logging.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_video_encoder.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppapi_thunk_export.h"
#include "ppapi/thunk/ppb_video_encoder_api.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Resource Create(PP_Instance instance) {
  VLOG(4) << "PPB_VideoEncoder::Create()";
  EnterResourceCreation enter(instance);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateVideoEncoder(instance);
}

PP_Bool IsVideoEncoder(PP_Resource resource) {
  VLOG(4) << "PPB_VideoEncoder::IsVideoEncoder()";
  EnterResource<PPB_VideoEncoder_API> enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

int32_t GetSupportedProfiles_0_1(PP_Resource video_encoder,
                                 struct PP_ArrayOutput output,
                                 struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_VideoEncoder::GetSupportedProfiles_0_1()";
  EnterResource<PPB_VideoEncoder_API> enter(video_encoder, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(
      enter.object()->GetSupportedProfiles0_1(output, enter.callback()));
}

int32_t GetSupportedProfiles(PP_Resource video_encoder,
                             struct PP_ArrayOutput output,
                             struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_VideoEncoder::GetSupportedProfiles()";
  EnterResource<PPB_VideoEncoder_API> enter(video_encoder, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(
      enter.object()->GetSupportedProfiles(output, enter.callback()));
}

int32_t Initialize(PP_Resource video_encoder,
                   PP_VideoFrame_Format input_format,
                   const struct PP_Size* input_visible_size,
                   PP_VideoProfile output_profile,
                   uint32_t initial_bitrate,
                   PP_HardwareAcceleration acceleration,
                   struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_VideoEncoder::Initialize()";
  EnterResource<PPB_VideoEncoder_API> enter(video_encoder, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->Initialize(
      input_format, input_visible_size, output_profile, initial_bitrate,
      acceleration, enter.callback()));
}

int32_t GetFramesRequired(PP_Resource video_encoder) {
  VLOG(4) << "PPB_VideoEncoder::GetFramesRequired()";
  EnterResource<PPB_VideoEncoder_API> enter(video_encoder, true);
  if (enter.failed())
    return enter.retval();
  return enter.object()->GetFramesRequired();
}

int32_t GetFrameCodedSize(PP_Resource video_encoder,
                          struct PP_Size* coded_size) {
  VLOG(4) << "PPB_VideoEncoder::GetFrameCodedSize()";
  EnterResource<PPB_VideoEncoder_API> enter(video_encoder, true);
  if (enter.failed())
    return enter.retval();
  return enter.object()->GetFrameCodedSize(coded_size);
}

int32_t GetVideoFrame(PP_Resource video_encoder,
                      PP_Resource* video_frame,
                      struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_VideoEncoder::GetVideoFrame()";
  EnterResource<PPB_VideoEncoder_API> enter(video_encoder, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(
      enter.object()->GetVideoFrame(video_frame, enter.callback()));
}

int32_t Encode(PP_Resource video_encoder,
               PP_Resource video_frame,
               PP_Bool force_keyframe,
               struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_VideoEncoder::Encode()";
  EnterResource<PPB_VideoEncoder_API> enter(video_encoder, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(
      enter.object()->Encode(video_frame, force_keyframe, enter.callback()));
}

int32_t GetBitstreamBuffer(PP_Resource video_encoder,
                           struct PP_BitstreamBuffer* bitstream_buffer,
                           struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_VideoEncoder::GetBitstreamBuffer()";
  EnterResource<PPB_VideoEncoder_API> enter(video_encoder, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(
      enter.object()->GetBitstreamBuffer(bitstream_buffer, enter.callback()));
}

void RecycleBitstreamBuffer(PP_Resource video_encoder,
                            const struct PP_BitstreamBuffer* bitstream_buffer) {
  VLOG(4) << "PPB_VideoEncoder::RecycleBitstreamBuffer()";
  EnterResource<PPB_VideoEncoder_API> enter(video_encoder, true);
  if (enter.failed())
    return;
  enter.object()->RecycleBitstreamBuffer(bitstream_buffer);
}

void RequestEncodingParametersChange(PP_Resource video_encoder,
                                     uint32_t bitrate,
                                     uint32_t framerate) {
  VLOG(4) << "PPB_VideoEncoder::RequestEncodingParametersChange()";
  EnterResource<PPB_VideoEncoder_API> enter(video_encoder, true);
  if (enter.failed())
    return;
  enter.object()->RequestEncodingParametersChange(bitrate, framerate);
}

void Close(PP_Resource video_encoder) {
  VLOG(4) << "PPB_VideoEncoder::Close()";
  EnterResource<PPB_VideoEncoder_API> enter(video_encoder, true);
  if (enter.failed())
    return;
  enter.object()->Close();
}

const PPB_VideoEncoder_0_1 g_ppb_videoencoder_thunk_0_1 = {
    &Create,
    &IsVideoEncoder,
    &GetSupportedProfiles_0_1,
    &Initialize,
    &GetFramesRequired,
    &GetFrameCodedSize,
    &GetVideoFrame,
    &Encode,
    &GetBitstreamBuffer,
    &RecycleBitstreamBuffer,
    &RequestEncodingParametersChange,
    &Close};

const PPB_VideoEncoder_0_2 g_ppb_videoencoder_thunk_0_2 = {
    &Create,
    &IsVideoEncoder,
    &GetSupportedProfiles,
    &Initialize,
    &GetFramesRequired,
    &GetFrameCodedSize,
    &GetVideoFrame,
    &Encode,
    &GetBitstreamBuffer,
    &RecycleBitstreamBuffer,
    &RequestEncodingParametersChange,
    &Close};

}  // namespace

PPAPI_THUNK_EXPORT const PPB_VideoEncoder_0_1* GetPPB_VideoEncoder_0_1_Thunk() {
  return &g_ppb_videoencoder_thunk_0_1;
}

PPAPI_THUNK_EXPORT const PPB_VideoEncoder_0_2* GetPPB_VideoEncoder_0_2_Thunk() {
  return &g_ppb_videoencoder_thunk_0_2;
}

}  // namespace thunk
}  // namespace ppapi
