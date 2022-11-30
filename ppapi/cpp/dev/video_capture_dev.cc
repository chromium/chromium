// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/video_capture_dev.h"

#include "ppapi/c/dev/ppb_video_capture_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_VideoCapture_Dev_0_3>() {
  return PPB_VIDEOCAPTURE_DEV_INTERFACE_0_3;
}

}  // namespace

VideoCapture_Dev::VideoCapture_Dev(const InstanceHandle& instance) {
  if (has_interface<PPB_VideoCapture_Dev_0_3>()) {
    PassRefFromConstructor(get_interface<PPB_VideoCapture_Dev_0_3>()->Create(
        instance.pp_instance()));
  }
}

VideoCapture_Dev::VideoCapture_Dev(PP_Resource resource)
    : Resource(resource) {
}

VideoCapture_Dev::~VideoCapture_Dev() {
}

// static
bool VideoCapture_Dev::IsAvailable() {
  return has_interface<PPB_VideoCapture_Dev_0_3>();
}

int32_t VideoCapture_Dev::EnumerateDevices(
    const CompletionCallbackWithOutput<std::vector<DeviceRef_Dev> >& callback) {
  if (has_interface<PPB_VideoCapture_Dev_0_3>()) {
    return get_interface<PPB_VideoCapture_Dev_0_3>()->EnumerateDevices(
        pp_resource(), callback.output(), callback.pp_completion_callback());
  }

  return callback.MayForce(PP_ERROR_NOINTERFACE);
}

int32_t VideoCapture_Dev::MonitorDeviceChange(
    PP_MonitorDeviceChangeCallback callback,
    void* user_data) {
  if (has_interface<PPB_VideoCapture_Dev_0_3>()) {
    return get_interface<PPB_VideoCapture_Dev_0_3>()->MonitorDeviceChange(
        pp_resource(), callback, user_data);
  }

  return PP_ERROR_NOINTERFACE;
}

int32_t VideoCapture_Dev::Open(
    const DeviceRef_Dev& device_ref,
    const PP_VideoCaptureDeviceInfo_Dev& requested_info,
    uint32_t buffer_count,
    const CompletionCallback& callback) {
  if (has_interface<PPB_VideoCapture_Dev_0_3>()) {
    return get_interface<PPB_VideoCapture_Dev_0_3>()->Open(
        pp_resource(), device_ref.pp_resource(), &requested_info, buffer_count,
        callback.pp_completion_callback());
  }

  return callback.MayForce(PP_ERROR_NOINTERFACE);
}

int32_t VideoCapture_Dev::StartCapture() {
  if (has_interface<PPB_VideoCapture_Dev_0_3>()) {
    return get_interface<PPB_VideoCapture_Dev_0_3>()->StartCapture(
        pp_resource());
  }

  return PP_ERROR_NOINTERFACE;
}

int32_t VideoCapture_Dev::ReuseBuffer(uint32_t buffer) {
  if (has_interface<PPB_VideoCapture_Dev_0_3>()) {
    return get_interface<PPB_VideoCapture_Dev_0_3>()->ReuseBuffer(pp_resource(),
                                                                  buffer);
  }

  return PP_ERROR_NOINTERFACE;
}

int32_t VideoCapture_Dev::StopCapture() {
  if (has_interface<PPB_VideoCapture_Dev_0_3>()) {
    return get_interface<PPB_VideoCapture_Dev_0_3>()->StopCapture(
        pp_resource());
  }

  return PP_ERROR_NOINTERFACE;
}

void VideoCapture_Dev::Close() {
  if (has_interface<PPB_VideoCapture_Dev_0_3>()) {
    get_interface<PPB_VideoCapture_Dev_0_3>()->Close(pp_resource());
  }
}

}  // namespace pp
