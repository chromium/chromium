// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_camera_device_host.h"

#include <memory>

#include "content/public/renderer/render_frame.h"
#include "content/renderer/pepper/pepper_platform_camera_device.h"
#include "content/renderer/pepper/renderer_ppapi_host_impl.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/proxy/ppapi_messages.h"

namespace content {

PepperCameraDeviceHost::PepperCameraDeviceHost(RendererPpapiHostImpl* host,
                                               PP_Instance instance,
                                               PP_Resource resource)
    : ResourceHost(host->GetPpapiHost(), instance, resource),
      renderer_ppapi_host_(host) {
}

PepperCameraDeviceHost::~PepperCameraDeviceHost() {
  DetachPlatformCameraDevice();
}

bool PepperCameraDeviceHost::Init() {
  return !!renderer_ppapi_host_->GetPluginInstance(pp_instance());
}

int32_t PepperCameraDeviceHost::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  int32_t result = PP_ERROR_FAILED;

  PPAPI_BEGIN_MESSAGE_MAP(PepperCameraDeviceHost, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_CameraDevice_Open, OnOpen)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(
        PpapiHostMsg_CameraDevice_GetSupportedVideoCaptureFormats,
        OnGetSupportedVideoCaptureFormats)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(PpapiHostMsg_CameraDevice_Close,
                                        OnClose)
  PPAPI_END_MESSAGE_MAP()
  return result;
}

void PepperCameraDeviceHost::OnInitialized(bool succeeded) {
  if (!open_reply_context_.is_valid())
    return;

  if (succeeded) {
    open_reply_context_.params.set_result(PP_OK);
  } else {
    DetachPlatformCameraDevice();
    open_reply_context_.params.set_result(PP_ERROR_FAILED);
  }

  host()->SendReply(open_reply_context_,
                    PpapiPluginMsg_CameraDevice_OpenReply());
  open_reply_context_ = ppapi::host::ReplyMessageContext();
}

void PepperCameraDeviceHost::OnVideoCaptureFormatsEnumerated(
    const std::vector<PP_VideoCaptureFormat>& formats) {
  if (!video_capture_formats_reply_context_.is_valid())
    return;

  if (formats.size() > 0)
    video_capture_formats_reply_context_.params.set_result(PP_OK);
  else
    video_capture_formats_reply_context_.params.set_result(PP_ERROR_FAILED);
  host()->SendReply(
      video_capture_formats_reply_context_,
      PpapiPluginMsg_CameraDevice_GetSupportedVideoCaptureFormatsReply(
          formats));
  video_capture_formats_reply_context_ = ppapi::host::ReplyMessageContext();
}

int32_t PepperCameraDeviceHost::OnOpen(ppapi::host::HostMessageContext* context,
                                       const std::string& device_id) {
  if (open_reply_context_.is_valid())
    return PP_ERROR_INPROGRESS;

  if (platform_camera_device_.get())
    return PP_ERROR_FAILED;

  GURL document_url = renderer_ppapi_host_->GetDocumentURL(pp_instance());
  if (!document_url.is_valid())
    return PP_ERROR_FAILED;

  platform_camera_device_ = std::make_unique<PepperPlatformCameraDevice>(
      renderer_ppapi_host_->GetRenderFrameForInstance(pp_instance())
          ->GetRoutingID(),
      device_id, this);

  open_reply_context_ = context->MakeReplyMessageContext();

  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperCameraDeviceHost::OnClose(
    ppapi::host::HostMessageContext* context) {
  DetachPlatformCameraDevice();
  return PP_OK;
}

int32_t PepperCameraDeviceHost::OnGetSupportedVideoCaptureFormats(
    ppapi::host::HostMessageContext* context) {
  if (video_capture_formats_reply_context_.is_valid())
    return PP_ERROR_INPROGRESS;
  if (!platform_camera_device_)
    return PP_ERROR_FAILED;

  video_capture_formats_reply_context_ = context->MakeReplyMessageContext();
  platform_camera_device_->GetSupportedVideoCaptureFormats();

  return PP_OK_COMPLETIONPENDING;
}

void PepperCameraDeviceHost::DetachPlatformCameraDevice() {
  if (platform_camera_device_) {
    platform_camera_device_->DetachEventHandler();
    platform_camera_device_.reset();
  }
}

}  // namespace content
