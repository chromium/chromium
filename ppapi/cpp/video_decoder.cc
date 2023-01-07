// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/video_decoder.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_video_decoder.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <>
const char* interface_name<PPB_VideoDecoder_0_1>() {
  return PPB_VIDEODECODER_INTERFACE_0_1;
}

template <>
const char* interface_name<PPB_VideoDecoder_0_2>() {
  return PPB_VIDEODECODER_INTERFACE_0_2;
}

template <>
const char* interface_name<PPB_VideoDecoder_1_0>() {
  return PPB_VIDEODECODER_INTERFACE_1_0;
}

template <>
const char* interface_name<PPB_VideoDecoder_1_1>() {
  return PPB_VIDEODECODER_INTERFACE_1_1;
}

// This struct is used to adapt CompletionCallbackWithOutput<PP_VideoPicture> to
// the pre-1.0 APIs, which return PP_VideoPicture_0_1. This struct is allocated
// on the heap, and deleted in CallbackConverter.
struct CallbackData_0_1 {
  explicit CallbackData_0_1(
      const CompletionCallbackWithOutput<PP_VideoPicture>& cc)
      : original_picture(cc.output()),
        original_callback(cc.pp_completion_callback()) {}
  PP_VideoPicture_0_1 picture;
  PP_VideoPicture* original_picture;
  PP_CompletionCallback original_callback;
};

// Convert a 1.0 style callback to pre-1.0 callback.
void CallbackConverter(void* user_data, int32_t result) {
  CallbackData_0_1* data = static_cast<CallbackData_0_1*>(user_data);
  if (result == PP_OK) {
    PP_VideoPicture_0_1* picture = &data->picture;
    PP_VideoPicture* original_picture = data->original_picture;
    original_picture->decode_id = picture->decode_id;
    original_picture->texture_id = picture->texture_id;
    original_picture->texture_target = picture->texture_target;
    original_picture->texture_size = picture->texture_size;
    // Set visible_rect to the entire picture.
    original_picture->visible_rect = PP_MakeRectFromXYWH(
        0, 0, picture->texture_size.width, picture->texture_size.height);
  }

  // Now execute the original callback.
  PP_RunCompletionCallback(&data->original_callback, result);
  delete data;
}

}  // namespace

VideoDecoder::VideoDecoder() {
}

VideoDecoder::VideoDecoder(const InstanceHandle& instance) {
  if (has_interface<PPB_VideoDecoder_1_1>()) {
    PassRefFromConstructor(
        get_interface<PPB_VideoDecoder_1_1>()->Create(instance.pp_instance()));
  } else if (has_interface<PPB_VideoDecoder_1_0>()) {
    PassRefFromConstructor(
        get_interface<PPB_VideoDecoder_1_0>()->Create(instance.pp_instance()));
  } else if (has_interface<PPB_VideoDecoder_0_2>()) {
    PassRefFromConstructor(
        get_interface<PPB_VideoDecoder_0_2>()->Create(instance.pp_instance()));
  } else if (has_interface<PPB_VideoDecoder_0_1>()) {
    PassRefFromConstructor(
        get_interface<PPB_VideoDecoder_0_1>()->Create(instance.pp_instance()));
  }
}

VideoDecoder::VideoDecoder(const VideoDecoder& other) : Resource(other) {
}

int32_t VideoDecoder::Initialize(const Graphics3D& context,
                                 PP_VideoProfile profile,
                                 PP_HardwareAcceleration acceleration,
                                 uint32_t min_picture_count,
                                 const CompletionCallback& cc) {
  if (has_interface<PPB_VideoDecoder_1_1>()) {
    return get_interface<PPB_VideoDecoder_1_1>()->Initialize(
        pp_resource(), context.pp_resource(), profile, acceleration,
        min_picture_count, cc.pp_completion_callback());
  }
  if (has_interface<PPB_VideoDecoder_1_0>()) {
    if (min_picture_count != 0)
        return cc.MayForce(PP_ERROR_NOTSUPPORTED);
    return get_interface<PPB_VideoDecoder_1_0>()->Initialize(
        pp_resource(), context.pp_resource(), profile, acceleration,
        cc.pp_completion_callback());
  }
  if (has_interface<PPB_VideoDecoder_0_2>()) {
    if (min_picture_count != 0)
        return cc.MayForce(PP_ERROR_NOTSUPPORTED);
    return get_interface<PPB_VideoDecoder_0_2>()->Initialize(
        pp_resource(), context.pp_resource(), profile, acceleration,
        cc.pp_completion_callback());
  }
  if (has_interface<PPB_VideoDecoder_0_1>()) {
    if (min_picture_count != 0)
        return cc.MayForce(PP_ERROR_NOTSUPPORTED);
    if (acceleration == PP_HARDWAREACCELERATION_NONE)
      return cc.MayForce(PP_ERROR_NOTSUPPORTED);
    return get_interface<PPB_VideoDecoder_0_1>()->Initialize(
        pp_resource(),
        context.pp_resource(),
        profile,
        acceleration == PP_HARDWAREACCELERATION_WITHFALLBACK
            ? PP_TRUE
            : PP_FALSE,
        cc.pp_completion_callback());
  }
  return cc.MayForce(PP_ERROR_NOINTERFACE);
}

int32_t VideoDecoder::Decode(uint32_t decode_id,
                             uint32_t size,
                             const void* buffer,
                             const CompletionCallback& cc) {
  if (has_interface<PPB_VideoDecoder_1_0>()) {
    return get_interface<PPB_VideoDecoder_1_0>()->Decode(
        pp_resource(), decode_id, size, buffer, cc.pp_completion_callback());
  }
  if (has_interface<PPB_VideoDecoder_0_2>()) {
    return get_interface<PPB_VideoDecoder_0_2>()->Decode(
        pp_resource(), decode_id, size, buffer, cc.pp_completion_callback());
  }
  if (has_interface<PPB_VideoDecoder_0_1>()) {
    return get_interface<PPB_VideoDecoder_0_1>()->Decode(
        pp_resource(), decode_id, size, buffer, cc.pp_completion_callback());
  }
  return cc.MayForce(PP_ERROR_NOINTERFACE);
}

int32_t VideoDecoder::GetPicture(
    const CompletionCallbackWithOutput<PP_VideoPicture>& cc) {
  if (has_interface<PPB_VideoDecoder_1_0>()) {
    return get_interface<PPB_VideoDecoder_1_0>()->GetPicture(
        pp_resource(), cc.output(), cc.pp_completion_callback());
  }
  if (has_interface<PPB_VideoDecoder_0_2>()) {
    // Data for our callback wrapper. The callback handler will delete it.
    CallbackData_0_1* data = new CallbackData_0_1(cc);
    return get_interface<PPB_VideoDecoder_0_2>()->GetPicture(
        pp_resource(), &data->picture,
        PP_MakeCompletionCallback(&CallbackConverter, data));
  }
  if (has_interface<PPB_VideoDecoder_0_1>()) {
    // Data for our callback wrapper. The callback handler will delete it.
    CallbackData_0_1* data = new CallbackData_0_1(cc);
    return get_interface<PPB_VideoDecoder_0_1>()->GetPicture(
        pp_resource(), &data->picture,
        PP_MakeCompletionCallback(&CallbackConverter, data));
  }
  return cc.MayForce(PP_ERROR_NOINTERFACE);
}

void VideoDecoder::RecyclePicture(const PP_VideoPicture& picture) {
  if (has_interface<PPB_VideoDecoder_1_0>()) {
    get_interface<PPB_VideoDecoder_1_0>()->RecyclePicture(pp_resource(),
                                                          &picture);
  } else if (has_interface<PPB_VideoDecoder_0_2>()) {
    get_interface<PPB_VideoDecoder_0_2>()->RecyclePicture(pp_resource(),
                                                          &picture);
  } else if (has_interface<PPB_VideoDecoder_0_1>()) {
    get_interface<PPB_VideoDecoder_0_1>()->RecyclePicture(pp_resource(),
                                                          &picture);
  }
}

int32_t VideoDecoder::Flush(const CompletionCallback& cc) {
  if (has_interface<PPB_VideoDecoder_1_0>()) {
    return get_interface<PPB_VideoDecoder_1_0>()->Flush(
        pp_resource(), cc.pp_completion_callback());
  }
  if (has_interface<PPB_VideoDecoder_0_2>()) {
    return get_interface<PPB_VideoDecoder_0_2>()->Flush(
        pp_resource(), cc.pp_completion_callback());
  }
  if (has_interface<PPB_VideoDecoder_0_1>()) {
    return get_interface<PPB_VideoDecoder_0_1>()->Flush(
        pp_resource(), cc.pp_completion_callback());
  }
  return cc.MayForce(PP_ERROR_NOINTERFACE);
}

int32_t VideoDecoder::Reset(const CompletionCallback& cc) {
  if (has_interface<PPB_VideoDecoder_1_0>()) {
    return get_interface<PPB_VideoDecoder_1_0>()->Reset(
        pp_resource(), cc.pp_completion_callback());
  }
  if (has_interface<PPB_VideoDecoder_0_2>()) {
    return get_interface<PPB_VideoDecoder_0_2>()->Reset(
        pp_resource(), cc.pp_completion_callback());
  }
  if (has_interface<PPB_VideoDecoder_0_1>()) {
    return get_interface<PPB_VideoDecoder_0_1>()->Reset(
        pp_resource(), cc.pp_completion_callback());
  }
  return cc.MayForce(PP_ERROR_NOINTERFACE);
}

}  // namespace pp
