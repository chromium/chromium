// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_LINUX_VIDEO_CAPTURE_DEVICE_CHROMEOS_H_
#define MEDIA_CAPTURE_VIDEO_LINUX_VIDEO_CAPTURE_DEVICE_CHROMEOS_H_

#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "media/capture/video/chromeos/display_rotation_observer.h"
#include "media/capture/video/linux/video_capture_device_linux.h"

namespace display {
class Display;
}  // namespace display

namespace media {

struct ChromeOSDeviceCameraConfig {
  ChromeOSDeviceCameraConfig(VideoFacingMode lens_facing,
                             int camera_orientation)
      : lens_facing(lens_facing),
        camera_orientation(camera_orientation),
        // External cameras have lens_facing as MEDIA_VIDEO_FACING_NONE.
        // We don't want to rotate the frame even if the device rotates.
        rotates_with_device(lens_facing !=
                            VideoFacingMode::MEDIA_VIDEO_FACING_NONE) {}

  const VideoFacingMode lens_facing;
  const int camera_orientation;
  // Whether the incoming frames should rotate when the device rotates.
  const bool rotates_with_device;
};

// This class is functionally the same as VideoCaptureDeviceLinux, with the
// exception that it is aware of the orientation of the internal Display.  When
// the internal Display is rotated, the frames captured are rotated to match.
class VideoCaptureDeviceChromeOS : public VideoCaptureDeviceLinux,
                                   public DisplayRotationObserver {
 public:
  VideoCaptureDeviceChromeOS(
      const ChromeOSDeviceCameraConfig& camera_config,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      scoped_refptr<V4L2CaptureDevice> v4l2,
      const VideoCaptureDeviceDescriptor& device_descriptor);
  ~VideoCaptureDeviceChromeOS() override;

 protected:
  void SetRotation(int rotation) override;

 private:
  // DisplayRotationObserver implementation.
  void SetDisplayRotation(const display::Display& display) override;

  const ChromeOSDeviceCameraConfig camera_config_;
  scoped_refptr<ScreenObserverDelegate> screen_observer_delegate_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_IMPLICIT_CONSTRUCTORS(VideoCaptureDeviceChromeOS);
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_LINUX_VIDEO_CAPTURE_DEVICE_CHROMEOS_H_
