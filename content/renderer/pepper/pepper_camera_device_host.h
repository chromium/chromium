// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_CAMERA_DEVICE_HOST_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_CAMERA_DEVICE_HOST_H_

#include <stdint.h>

#include <memory>

#include "base/memory/raw_ptr.h"
#include "content/public/renderer/renderer_ppapi_host.h"
#include "content/renderer/pepper/ppb_buffer_impl.h"
#include "ppapi/c/pp_size.h"
#include "ppapi/c/private/pp_video_capture_format.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/resource_host.h"

namespace content {
class PepperPlatformCameraDevice;
class RendererPpapiHostImpl;

class PepperCameraDeviceHost : public ppapi::host::ResourceHost {
 public:
  PepperCameraDeviceHost(RendererPpapiHostImpl* host,
                         PP_Instance instance,
                         PP_Resource resource);

  PepperCameraDeviceHost(const PepperCameraDeviceHost&) = delete;
  PepperCameraDeviceHost& operator=(const PepperCameraDeviceHost&) = delete;

  ~PepperCameraDeviceHost() override;

  bool Init();

  int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) override;

  // These methods are called by PepperPlatformCameraDevice only.

  // Called when camera device is initialized.
  void OnInitialized(bool succeeded);

  // Called when the video capture formats are enumerated.
  void OnVideoCaptureFormatsEnumerated(
      const std::vector<PP_VideoCaptureFormat>& formats);

 private:
  // Plugin -> host message handlers.
  int32_t OnOpen(ppapi::host::HostMessageContext* context,
                 const std::string& device_id);
  int32_t OnClose(ppapi::host::HostMessageContext* context);
  int32_t OnGetSupportedVideoCaptureFormats(
      ppapi::host::HostMessageContext* context);

  // Utility methods.
  void DetachPlatformCameraDevice();

  std::unique_ptr<PepperPlatformCameraDevice> platform_camera_device_;

  raw_ptr<RendererPpapiHostImpl> renderer_ppapi_host_;

  ppapi::host::ReplyMessageContext open_reply_context_;

  ppapi::host::ReplyMessageContext video_capture_formats_reply_context_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_CAMERA_DEVICE_HOST_H_
