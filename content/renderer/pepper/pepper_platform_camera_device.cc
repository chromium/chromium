// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_platform_camera_device.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "content/renderer/pepper/gfx_conversion.h"
#include "content/renderer/pepper/pepper_camera_device_host.h"
#include "content/renderer/pepper/pepper_media_device_manager.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "media/base/bind_to_current_loop.h"
#include "third_party/blink/public/platform/modules/video_capture/web_video_capture_impl_manager.h"

namespace content {

PepperPlatformCameraDevice::PepperPlatformCameraDevice(
    int render_frame_id,
    const std::string& device_id,
    PepperCameraDeviceHost* handler)
    : render_frame_id_(render_frame_id),
      device_id_(device_id),
      handler_(handler),
      pending_open_device_(false),
      pending_open_device_id_(-1) {
  // We need to open the device and obtain the label and session ID before
  // initializing.
  PepperMediaDeviceManager* const device_manager = GetMediaDeviceManager();
  if (device_manager) {
    pending_open_device_id_ = device_manager->OpenDevice(
        PP_DEVICETYPE_DEV_VIDEOCAPTURE, device_id, handler->pp_instance(),
        base::BindOnce(&PepperPlatformCameraDevice::OnDeviceOpened,
                       weak_factory_.GetWeakPtr()));
    pending_open_device_ = true;
  }
}

void PepperPlatformCameraDevice::GetSupportedVideoCaptureFormats() {
  DCHECK(thread_checker_.CalledOnValidThread());
  blink::WebVideoCaptureImplManager* manager =
      RenderThreadImpl::current()->video_capture_impl_manager();
  manager->GetDeviceSupportedFormats(
      session_id_,
      media::BindToCurrentLoop(base::BindOnce(
          &PepperPlatformCameraDevice::OnDeviceSupportedFormatsEnumerated,
          weak_factory_.GetWeakPtr())));
}

void PepperPlatformCameraDevice::DetachEventHandler() {
  DCHECK(thread_checker_.CalledOnValidThread());
  handler_ = nullptr;
  if (release_device_cb_) {
    std::move(release_device_cb_).Run();
  }
  if (!label_.empty()) {
    PepperMediaDeviceManager* const device_manager = GetMediaDeviceManager();
    if (device_manager)
      device_manager->CloseDevice(label_);
    label_.clear();
  }
  if (pending_open_device_) {
    PepperMediaDeviceManager* const device_manager = GetMediaDeviceManager();
    if (device_manager)
      device_manager->CancelOpenDevice(pending_open_device_id_);
    pending_open_device_ = false;
    pending_open_device_id_ = -1;
  }
}

PepperPlatformCameraDevice::~PepperPlatformCameraDevice() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!release_device_cb_);
  DCHECK(label_.empty());
  DCHECK(!pending_open_device_);
}

void PepperPlatformCameraDevice::OnDeviceOpened(int request_id,
                                                bool succeeded,
                                                const std::string& label) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(handler_);

  pending_open_device_ = false;
  pending_open_device_id_ = -1;

  PepperMediaDeviceManager* const device_manager = GetMediaDeviceManager();
  succeeded = succeeded && device_manager;
  if (succeeded) {
    label_ = label;
    session_id_ =
        device_manager->GetSessionID(PP_DEVICETYPE_DEV_VIDEOCAPTURE, label);
    blink::WebVideoCaptureImplManager* manager =
        RenderThreadImpl::current()->video_capture_impl_manager();
    release_device_cb_ = manager->UseDevice(session_id_);
  }

  handler_->OnInitialized(succeeded);
}

void PepperPlatformCameraDevice::OnDeviceSupportedFormatsEnumerated(
    const media::VideoCaptureFormats& formats) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(handler_);

  std::vector<PP_VideoCaptureFormat> output_formats;
  for (const auto& format : formats) {
    PP_VideoCaptureFormat output_format;
    output_format.frame_size = PP_FromGfxSize(format.frame_size);
    output_format.frame_rate = format.frame_rate;
    output_formats.push_back(output_format);
  }
  handler_->OnVideoCaptureFormatsEnumerated(output_formats);
}

PepperMediaDeviceManager* PepperPlatformCameraDevice::GetMediaDeviceManager() {
  RenderFrameImpl* const render_frame =
      RenderFrameImpl::FromRoutingID(render_frame_id_);
  return render_frame
             ? PepperMediaDeviceManager::GetForRenderFrame(render_frame).get()
             : nullptr;
}

}  // namespace content
