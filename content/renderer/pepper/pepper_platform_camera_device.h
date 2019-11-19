// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_PLATFORM_CAMERA_DEVICE_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_PLATFORM_CAMERA_DEVICE_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/unguessable_token.h"
#include "media/capture/video_capture_types.h"
#include "third_party/blink/public/common/media/video_capture.h"

namespace content {
class PepperMediaDeviceManager;
class PepperCameraDeviceHost;

// This object must only be used on the thread it's constructed on.
class PepperPlatformCameraDevice {
 public:
  PepperPlatformCameraDevice(int render_frame_id,
                             const std::string& device_id,
                             PepperCameraDeviceHost* handler);
  ~PepperPlatformCameraDevice();

  // Detaches the event handler and stops sending notifications to it.
  void DetachEventHandler();

  void GetSupportedVideoCaptureFormats();

 private:
  void OnDeviceOpened(int request_id, bool succeeded, const std::string& label);

  // Called by blink::WebVideoCaptureImplManager.
  void OnDeviceSupportedFormatsEnumerated(
      const media::VideoCaptureFormats& formats);

  // Can return NULL if the RenderFrame referenced by |render_frame_id_| has
  // gone away.
  PepperMediaDeviceManager* GetMediaDeviceManager();

  const int render_frame_id_;
  const std::string device_id_;

  std::string label_;
  base::UnguessableToken session_id_;
  base::OnceClosure release_device_cb_;

  PepperCameraDeviceHost* handler_;

  // Whether we have a pending request to open a device. We have to make sure
  // there isn't any pending request before this object goes away.
  bool pending_open_device_;
  int pending_open_device_id_;

  base::ThreadChecker thread_checker_;

  base::WeakPtrFactory<PepperPlatformCameraDevice> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PepperPlatformCameraDevice);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_PLATFORM_CAMERA_DEVICE_H_
