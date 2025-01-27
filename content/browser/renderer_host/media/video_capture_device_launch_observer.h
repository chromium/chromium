// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_DEVICE_LAUNCH_OBSERVER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_DEVICE_LAUNCH_OBSERVER_H_

#include "media/capture/video_capture_types.h"

namespace content {

class VideoCaptureController;

class VideoCaptureDeviceLaunchObserver {
 public:
  virtual ~VideoCaptureDeviceLaunchObserver() = default;
  virtual void OnDeviceLaunched(
      scoped_refptr<VideoCaptureController> controller) = 0;
  virtual void OnDeviceLaunchFailed(
      scoped_refptr<VideoCaptureController> controller,
      media::VideoCaptureError error) = 0;
  virtual void OnDeviceLaunchAborted() = 0;
  virtual void OnDeviceConnectionLost(
      scoped_refptr<VideoCaptureController> controller) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_DEVICE_LAUNCH_OBSERVER_H_
