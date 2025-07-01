// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/camera_device_resource.h"

#include "base/functional/bind.h"
#include "ppapi/proxy/camera_capabilities_resource.h"
#include "ppapi/proxy/plugin_resource_tracker.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/var.h"

namespace ppapi {
namespace proxy {

CameraDeviceResource::CameraDeviceResource(Connection connection,
                                           PP_Instance instance)
    : PluginResource(connection, instance),
      open_state_(OpenState::BEFORE_OPEN) {
  SendCreate(RENDERER, PpapiHostMsg_CameraDevice_Create());
}

CameraDeviceResource::~CameraDeviceResource() {
}

int32_t CameraDeviceResource::Open(
    PP_Var device_id,
    const scoped_refptr<TrackedCallback>& callback) {
  if (open_state_ != OpenState::BEFORE_OPEN)
    return PP_ERROR_FAILED;

  if (TrackedCallback::IsPending(open_callback_))
    return PP_ERROR_INPROGRESS;

  scoped_refptr<StringVar> source_string_var(StringVar::FromPPVar(device_id));
  if (!source_string_var || source_string_var->value().empty())
    return PP_ERROR_BADARGUMENT;

  open_callback_ = callback;

  Call<PpapiPluginMsg_CameraDevice_OpenReply>(
      RENDERER, PpapiHostMsg_CameraDevice_Open(source_string_var->value()),
      base::BindOnce(&CameraDeviceResource::OnPluginMsgOpenReply,
                     base::Unretained(this)));
  return PP_OK_COMPLETIONPENDING;
}

void CameraDeviceResource::Close() {
  if (open_state_ == OpenState::CLOSED)
    return;

  if (TrackedCallback::IsPending(open_callback_)) {
    open_callback_->PostAbort();
    open_callback_ = nullptr;
  }

  if (TrackedCallback::IsPending(get_capabilities_callback_)) {
    get_capabilities_callback_->PostAbort();
    get_capabilities_callback_ = nullptr;
  }

  Post(RENDERER, PpapiHostMsg_CameraDevice_Close());

  open_state_ = OpenState::CLOSED;
}

int32_t CameraDeviceResource::GetCameraCapabilities(
    PP_Resource* capabilities,
    const scoped_refptr<TrackedCallback>& callback) {
  if (!is_opened())
    return PP_ERROR_FAILED;

  if (TrackedCallback::IsPending(get_capabilities_callback_))
    return PP_ERROR_INPROGRESS;

  if (camera_capabilities_.get()) {
    *capabilities = camera_capabilities_->GetReference();
    return PP_OK;
  }

  get_capabilities_callback_ = callback;
  Call<PpapiPluginMsg_CameraDevice_GetSupportedVideoCaptureFormatsReply>(
      RENDERER, PpapiHostMsg_CameraDevice_GetSupportedVideoCaptureFormats(),
      base::BindOnce(
          &CameraDeviceResource::OnPluginMsgGetVideoCaptureFormatsReply,
          base::Unretained(this), capabilities));

  return PP_OK_COMPLETIONPENDING;
}

void CameraDeviceResource::OnPluginMsgOpenReply(
    const ResourceMessageReplyParams& params) {
  // The callback may have been aborted by Close().
  if (TrackedCallback::IsPending(open_callback_)) {
    if (open_state_ == OpenState::BEFORE_OPEN && params.result() == PP_OK)
      open_state_ = OpenState::OPENED;

    open_callback_->Run(params.result());
  }
}

void CameraDeviceResource::OnPluginMsgGetVideoCaptureFormatsReply(
    PP_Resource* capabilities_output,
    const ResourceMessageReplyParams& params,
    const std::vector<PP_VideoCaptureFormat>& formats) {
  if (!TrackedCallback::IsPending(get_capabilities_callback_))
    return;

  // Return camera capabilities.
  int32_t result = params.result();
  scoped_refptr<TrackedCallback> callback;
  callback.swap(get_capabilities_callback_);
  if (result == PP_OK) {
    camera_capabilities_ =
        new CameraCapabilitiesResource(pp_instance(), formats);
    *capabilities_output = camera_capabilities_->GetReference();
  }
  callback->Run(result == PP_OK ? PP_OK : PP_ERROR_FAILED);
}

}  // namespace proxy
}  // namespace ppapi
