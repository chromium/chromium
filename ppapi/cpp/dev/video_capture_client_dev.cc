// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ppapi/cpp/dev/video_capture_client_dev.h"

#include "ppapi/c/dev/ppp_video_capture_dev.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/module.h"

namespace pp {

namespace {

const char kPPPVideoCaptureInterface[] = PPP_VIDEO_CAPTURE_DEV_INTERFACE;

void OnDeviceInfo(PP_Instance instance,
                  PP_Resource resource,
                  const struct PP_VideoCaptureDeviceInfo_Dev* info,
                  uint32_t buffer_count,
                  const PP_Resource buffers[]) {
  VideoCaptureClient_Dev* client = static_cast<VideoCaptureClient_Dev*>(
      Instance::GetPerInstanceObject(instance, kPPPVideoCaptureInterface));
  if (!client)
    return;

  std::vector<Buffer_Dev> buffer_list;
  buffer_list.reserve(buffer_count);
  for (uint32_t i = 0; i < buffer_count; ++i)
    buffer_list.push_back(Buffer_Dev(buffers[i]));

  client->OnDeviceInfo(resource, *info, buffer_list);
}

void OnStatus(PP_Instance instance, PP_Resource resource, uint32_t status) {
  VideoCaptureClient_Dev* client = static_cast<VideoCaptureClient_Dev*>(
      Instance::GetPerInstanceObject(instance, kPPPVideoCaptureInterface));
  if (client)
    client->OnStatus(resource, status);
}

void OnError(PP_Instance instance, PP_Resource resource, uint32_t error_code) {
  VideoCaptureClient_Dev* client = static_cast<VideoCaptureClient_Dev*>(
      Instance::GetPerInstanceObject(instance, kPPPVideoCaptureInterface));
  if (client)
    client->OnError(resource, error_code);
}

void OnBufferReady(PP_Instance instance,
                   PP_Resource resource,
                   uint32_t buffer) {
  VideoCaptureClient_Dev* client = static_cast<VideoCaptureClient_Dev*>(
      Instance::GetPerInstanceObject(instance, kPPPVideoCaptureInterface));
  if (client)
    client->OnBufferReady(resource, buffer);
}

PPP_VideoCapture_Dev ppp_video_capture = {
  OnDeviceInfo,
  OnStatus,
  OnError,
  OnBufferReady
};

}  // namespace

VideoCaptureClient_Dev::VideoCaptureClient_Dev(Instance* instance)
    : instance_(instance) {
  Module::Get()->AddPluginInterface(kPPPVideoCaptureInterface,
                                    &ppp_video_capture);
  instance->AddPerInstanceObject(kPPPVideoCaptureInterface, this);
}

VideoCaptureClient_Dev::~VideoCaptureClient_Dev() {
  Instance::RemovePerInstanceObject(instance_,
                                    kPPPVideoCaptureInterface, this);
}

}  // namespace pp
