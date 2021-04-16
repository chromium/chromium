// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/linux/video_capture_device_chromeos.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/display/display.h"
#include "ui/display/display_observer.h"
#include "ui/display/screen.h"

namespace media {

VideoCaptureDeviceChromeOS::VideoCaptureDeviceChromeOS(
    const ChromeOSDeviceCameraConfig& camera_config,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    scoped_refptr<V4L2CaptureDevice> v4l2,
    const VideoCaptureDeviceDescriptor& device_descriptor)
    : VideoCaptureDeviceLinux(std::move(v4l2), device_descriptor),
      camera_config_(camera_config),
      screen_observer_delegate_(
          ScreenObserverDelegate::Create(this, ui_task_runner)) {}

VideoCaptureDeviceChromeOS::~VideoCaptureDeviceChromeOS() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  screen_observer_delegate_->RemoveObserver();
}

void VideoCaptureDeviceChromeOS::SetRotation(int rotation) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!camera_config_.rotates_with_device) {
    rotation = 0;
  } else if (camera_config_.lens_facing ==
             VideoFacingMode::MEDIA_VIDEO_FACING_ENVIRONMENT) {
    // Original frame when |rotation| = 0
    // -----------------------
    // |          *          |
    // |         * *         |
    // |        *   *        |
    // |       *******       |
    // |      *       *      |
    // |     *         *     |
    // -----------------------
    //
    // |rotation| = 90, this is what back camera sees
    // -----------------------
    // |    ********         |
    // |       *   ****      |
    // |       *      ***    |
    // |       *      ***    |
    // |       *   ****      |
    // |    ********         |
    // -----------------------
    //
    // |rotation| = 90, this is what front camera sees
    // -----------------------
    // |         ********    |
    // |      ****   *       |
    // |    ***      *       |
    // |    ***      *       |
    // |      ****   *       |
    // |         ********    |
    // -----------------------
    //
    // Therefore, for back camera, we need to rotate (360 - |rotation|).
    rotation = (360 - rotation) % 360;
  }
  // Take into account camera orientation w.r.t. the display. External cameras
  // would have camera_orientation_ as 0.
  rotation = (rotation + camera_config_.camera_orientation) % 360;
  VideoCaptureDeviceLinux::SetRotation(rotation);
}

void VideoCaptureDeviceChromeOS::SetDisplayRotation(
    const display::Display& display) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (display.IsInternal())
    SetRotation(display.rotation() * 90);
}

}  // namespace media
