// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/video_decoder_client_dev.h"

#include "ppapi/c/dev/ppp_video_decoder_dev.h"
#include "ppapi/cpp/dev/video_decoder_dev.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

const char kPPPVideoDecoderInterface[] = PPP_VIDEODECODER_DEV_INTERFACE;

// Callback to provide buffers for the decoded output pictures.
void ProvidePictureBuffers(PP_Instance instance,
                           PP_Resource decoder,
                           uint32_t req_num_of_bufs,
                           const PP_Size* dimensions,
                           uint32_t texture_target) {
  void* object = Instance::GetPerInstanceObject(instance,
                                                kPPPVideoDecoderInterface);
  if (!object)
    return;
  static_cast<VideoDecoderClient_Dev*>(object)->ProvidePictureBuffers(
      decoder, req_num_of_bufs, *dimensions, texture_target);
}

void DismissPictureBuffer(PP_Instance instance,
                          PP_Resource decoder,
                          int32_t picture_buffer_id) {
  void* object = Instance::GetPerInstanceObject(instance,
                                                kPPPVideoDecoderInterface);
  if (!object)
    return;
  static_cast<VideoDecoderClient_Dev*>(object)->DismissPictureBuffer(
      decoder, picture_buffer_id);
}

void PictureReady(PP_Instance instance,
                  PP_Resource decoder,
                  const PP_Picture_Dev* picture) {
  void* object = Instance::GetPerInstanceObject(instance,
                                                kPPPVideoDecoderInterface);
  if (!object)
    return;
  static_cast<VideoDecoderClient_Dev*>(object)->PictureReady(decoder, *picture);
}

void NotifyError(PP_Instance instance,
                 PP_Resource decoder,
                 PP_VideoDecodeError_Dev error) {
  void* object = Instance::GetPerInstanceObject(instance,
                                                kPPPVideoDecoderInterface);
  if (!object)
    return;
  static_cast<VideoDecoderClient_Dev*>(object)->NotifyError(decoder, error);
}

static PPP_VideoDecoder_Dev videodecoder_interface = {
  &ProvidePictureBuffers,
  &DismissPictureBuffer,
  &PictureReady,
  &NotifyError,
};

}  // namespace

VideoDecoderClient_Dev::VideoDecoderClient_Dev(Instance* instance)
    : associated_instance_(instance) {
  Module::Get()->AddPluginInterface(kPPPVideoDecoderInterface,
                                    &videodecoder_interface);
  instance->AddPerInstanceObject(kPPPVideoDecoderInterface, this);
}

VideoDecoderClient_Dev::~VideoDecoderClient_Dev() {
  Instance::RemovePerInstanceObject(associated_instance_,
                                    kPPPVideoDecoderInterface, this);
}

}  // namespace pp
