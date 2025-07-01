// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/video_decoder_dev.h"

#include "ppapi/c/dev/ppb_video_decoder_dev.h"
#include "ppapi/c/dev/ppp_video_decoder_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/graphics_3d.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_VideoDecoder_Dev>() {
  return PPB_VIDEODECODER_DEV_INTERFACE;
}

}  // namespace

VideoDecoder_Dev::VideoDecoder_Dev(const InstanceHandle& instance,
                                   const Graphics3D& context,
                                   PP_VideoDecoder_Profile profile) {
  if (!has_interface<PPB_VideoDecoder_Dev>())
    return;
  PassRefFromConstructor(get_interface<PPB_VideoDecoder_Dev>()->Create(
      instance.pp_instance(), context.pp_resource(), profile));
}

VideoDecoder_Dev::VideoDecoder_Dev(PP_Resource resource) : Resource(resource) {
}

VideoDecoder_Dev::~VideoDecoder_Dev() {
}

void VideoDecoder_Dev::AssignPictureBuffers(
    const std::vector<PP_PictureBuffer_Dev>& buffers) {
  if (!has_interface<PPB_VideoDecoder_Dev>() || !pp_resource())
    return;
  get_interface<PPB_VideoDecoder_Dev>()->AssignPictureBuffers(
      pp_resource(), static_cast<uint32_t>(buffers.size()), &buffers[0]);
}

int32_t VideoDecoder_Dev::Decode(
    const PP_VideoBitstreamBuffer_Dev& bitstream_buffer,
    const CompletionCallback& callback) {
  if (!has_interface<PPB_VideoDecoder_Dev>())
    return callback.MayForce(PP_ERROR_NOINTERFACE);
  return get_interface<PPB_VideoDecoder_Dev>()->Decode(
      pp_resource(), &bitstream_buffer, callback.pp_completion_callback());
}

void VideoDecoder_Dev::ReusePictureBuffer(int32_t picture_buffer_id) {
  if (!has_interface<PPB_VideoDecoder_Dev>() || !pp_resource())
    return;
  get_interface<PPB_VideoDecoder_Dev>()->ReusePictureBuffer(
      pp_resource(), picture_buffer_id);
}

int32_t VideoDecoder_Dev::Flush(const CompletionCallback& callback) {
  if (!has_interface<PPB_VideoDecoder_Dev>())
    return callback.MayForce(PP_ERROR_NOINTERFACE);
  return get_interface<PPB_VideoDecoder_Dev>()->Flush(
      pp_resource(), callback.pp_completion_callback());
}

int32_t VideoDecoder_Dev::Reset(const CompletionCallback& callback) {
  if (!has_interface<PPB_VideoDecoder_Dev>())
    return callback.MayForce(PP_ERROR_NOINTERFACE);
  return get_interface<PPB_VideoDecoder_Dev>()->Reset(
      pp_resource(), callback.pp_completion_callback());
}

}  // namespace pp
