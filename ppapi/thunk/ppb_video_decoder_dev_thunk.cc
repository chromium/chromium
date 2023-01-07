// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "ppapi/c/pp_errors.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_video_decoder_dev_api.h"
#include "ppapi/thunk/resource_creation_api.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace thunk {

namespace {

typedef EnterResource<PPB_VideoDecoder_Dev_API> EnterVideoDecoder;

PP_Resource Create(PP_Instance instance,
                   PP_Resource graphics_3d,
                   PP_VideoDecoder_Profile profile) {
  EnterResourceCreation enter(instance);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateVideoDecoderDev(
      instance, graphics_3d, profile);
}

PP_Bool IsVideoDecoder(PP_Resource resource) {
  EnterVideoDecoder enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

int32_t Decode(PP_Resource video_decoder,
               const PP_VideoBitstreamBuffer_Dev* bitstream_buffer,
               PP_CompletionCallback callback) {
  EnterVideoDecoder enter(video_decoder, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(
      enter.object()->Decode(bitstream_buffer, enter.callback()));
}

void AssignPictureBuffers(PP_Resource video_decoder,
                          uint32_t no_of_buffers,
                          const PP_PictureBuffer_Dev* buffers) {
  EnterVideoDecoder enter(video_decoder, true);
  if (enter.succeeded())
    enter.object()->AssignPictureBuffers(no_of_buffers, buffers);
}

void ReusePictureBuffer(PP_Resource video_decoder, int32_t picture_buffer_id) {
  EnterVideoDecoder enter(video_decoder, true);
  if (enter.succeeded())
    enter.object()->ReusePictureBuffer(picture_buffer_id);
}

int32_t Flush(PP_Resource video_decoder, PP_CompletionCallback callback) {
  EnterVideoDecoder enter(video_decoder, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->Flush(enter.callback()));
}

int32_t Reset(PP_Resource video_decoder, PP_CompletionCallback callback) {
  EnterVideoDecoder enter(video_decoder, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->Reset(enter.callback()));
}

void Destroy(PP_Resource video_decoder) {
  EnterVideoDecoder enter(video_decoder, true);
  if (enter.succeeded())
    enter.object()->Destroy();
}

const PPB_VideoDecoder_Dev g_ppb_videodecoder_dev_thunk = {
  &Create,
  &IsVideoDecoder,
  &Decode,
  &AssignPictureBuffers,
  &ReusePictureBuffer,
  &Flush,
  &Reset,
  &Destroy
};

}  // namespace

const PPB_VideoDecoder_Dev_0_16* GetPPB_VideoDecoder_Dev_0_16_Thunk() {
  return &g_ppb_videodecoder_dev_thunk;
}

}  // namespace thunk
}  // namespace ppapi
