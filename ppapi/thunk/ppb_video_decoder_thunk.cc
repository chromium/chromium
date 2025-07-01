// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From ppb_video_decoder.idl modified Wed Aug 12 17:59:47 2015.

#include <stdint.h>

#include "base/logging.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_video_decoder.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppapi_thunk_export.h"
#include "ppapi/thunk/ppb_video_decoder_api.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Resource Create(PP_Instance instance) {
  VLOG(4) << "PPB_VideoDecoder::Create()";
  EnterResourceCreation enter(instance);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateVideoDecoder(instance);
}

PP_Bool IsVideoDecoder(PP_Resource resource) {
  VLOG(4) << "PPB_VideoDecoder::IsVideoDecoder()";
  EnterResource<PPB_VideoDecoder_API> enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

int32_t Initialize_0_1(PP_Resource video_decoder,
                       PP_Resource graphics3d_context,
                       PP_VideoProfile profile,
                       PP_Bool allow_software_fallback,
                       struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_VideoDecoder::Initialize_0_1()";
  EnterResource<PPB_VideoDecoder_API> enter(video_decoder, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->Initialize0_1(
      graphics3d_context, profile, allow_software_fallback, enter.callback()));
}

int32_t Initialize_0_2(PP_Resource video_decoder,
                       PP_Resource graphics3d_context,
                       PP_VideoProfile profile,
                       PP_HardwareAcceleration acceleration,
                       struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_VideoDecoder::Initialize_0_2()";
  EnterResource<PPB_VideoDecoder_API> enter(video_decoder, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->Initialize0_2(
      graphics3d_context, profile, acceleration, enter.callback()));
}

int32_t Initialize(PP_Resource video_decoder,
                   PP_Resource graphics3d_context,
                   PP_VideoProfile profile,
                   PP_HardwareAcceleration acceleration,
                   uint32_t min_picture_count,
                   struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_VideoDecoder::Initialize()";
  EnterResource<PPB_VideoDecoder_API> enter(video_decoder, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(
      enter.object()->Initialize(graphics3d_context, profile, acceleration,
                                 min_picture_count, enter.callback()));
}

int32_t Decode(PP_Resource video_decoder,
               uint32_t decode_id,
               uint32_t size,
               const void* buffer,
               struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_VideoDecoder::Decode()";
  EnterResource<PPB_VideoDecoder_API> enter(video_decoder, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(
      enter.object()->Decode(decode_id, size, buffer, enter.callback()));
}

int32_t GetPicture_0_1(PP_Resource video_decoder,
                       struct PP_VideoPicture_0_1* picture,
                       struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_VideoDecoder::GetPicture_0_1()";
  EnterResource<PPB_VideoDecoder_API> enter(video_decoder, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(
      enter.object()->GetPicture0_1(picture, enter.callback()));
}

int32_t GetPicture(PP_Resource video_decoder,
                   struct PP_VideoPicture* picture,
                   struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_VideoDecoder::GetPicture()";
  EnterResource<PPB_VideoDecoder_API> enter(video_decoder, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->GetPicture(picture, enter.callback()));
}

void RecyclePicture(PP_Resource video_decoder,
                    const struct PP_VideoPicture* picture) {
  VLOG(4) << "PPB_VideoDecoder::RecyclePicture()";
  EnterResource<PPB_VideoDecoder_API> enter(video_decoder, true);
  if (enter.failed())
    return;
  enter.object()->RecyclePicture(picture);
}

int32_t Flush(PP_Resource video_decoder,
              struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_VideoDecoder::Flush()";
  EnterResource<PPB_VideoDecoder_API> enter(video_decoder, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->Flush(enter.callback()));
}

int32_t Reset(PP_Resource video_decoder,
              struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_VideoDecoder::Reset()";
  EnterResource<PPB_VideoDecoder_API> enter(video_decoder, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->Reset(enter.callback()));
}

const PPB_VideoDecoder_0_1 g_ppb_videodecoder_thunk_0_1 = {
    &Create,         &IsVideoDecoder, &Initialize_0_1, &Decode,
    &GetPicture_0_1, &RecyclePicture, &Flush,          &Reset};

const PPB_VideoDecoder_0_2 g_ppb_videodecoder_thunk_0_2 = {
    &Create,         &IsVideoDecoder, &Initialize_0_2, &Decode,
    &GetPicture_0_1, &RecyclePicture, &Flush,          &Reset};

const PPB_VideoDecoder_1_0 g_ppb_videodecoder_thunk_1_0 = {
    &Create,     &IsVideoDecoder, &Initialize_0_2, &Decode,
    &GetPicture, &RecyclePicture, &Flush,          &Reset};

const PPB_VideoDecoder_1_1 g_ppb_videodecoder_thunk_1_1 = {
    &Create,     &IsVideoDecoder, &Initialize, &Decode,
    &GetPicture, &RecyclePicture, &Flush,      &Reset};

}  // namespace

PPAPI_THUNK_EXPORT const PPB_VideoDecoder_0_1* GetPPB_VideoDecoder_0_1_Thunk() {
  return &g_ppb_videodecoder_thunk_0_1;
}

PPAPI_THUNK_EXPORT const PPB_VideoDecoder_0_2* GetPPB_VideoDecoder_0_2_Thunk() {
  return &g_ppb_videodecoder_thunk_0_2;
}

PPAPI_THUNK_EXPORT const PPB_VideoDecoder_1_0* GetPPB_VideoDecoder_1_0_Thunk() {
  return &g_ppb_videodecoder_thunk_1_0;
}

PPAPI_THUNK_EXPORT const PPB_VideoDecoder_1_1* GetPPB_VideoDecoder_1_1_Thunk() {
  return &g_ppb_videodecoder_thunk_1_1;
}

}  // namespace thunk
}  // namespace ppapi
