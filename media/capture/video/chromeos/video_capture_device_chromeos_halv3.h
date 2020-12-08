// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_CHROMEOS_VIDEO_CAPTURE_DEVICE_CHROMEOS_HALV3_H_
#define MEDIA_CAPTURE_VIDEO_CHROMEOS_VIDEO_CAPTURE_DEVICE_CHROMEOS_HALV3_H_

#include <memory>

#include "media/capture/video/video_capture_device.h"

namespace media {

class VideoCaptureDeviceChromeOSDelegate;

// Implementation of VideoCaptureDevice for ChromeOS with CrOS camera HALv3.
class CAPTURE_EXPORT VideoCaptureDeviceChromeOSHalv3 final
    : public VideoCaptureDevice {
 public:
  explicit VideoCaptureDeviceChromeOSHalv3(
      std::unique_ptr<VideoCaptureDeviceChromeOSDelegate> delegate);

  ~VideoCaptureDeviceChromeOSHalv3() final;

  // VideoCaptureDevice implementation.
  void AllocateAndStart(const VideoCaptureParams& params,
                        std::unique_ptr<Client> client) final;
  void StopAndDeAllocate() final;
  void TakePhoto(TakePhotoCallback callback) final;
  void GetPhotoState(GetPhotoStateCallback callback) final;
  void SetPhotoOptions(mojom::PhotoSettingsPtr settings,
                       SetPhotoOptionsCallback callback) final;

 private:
  std::unique_ptr<VideoCaptureDeviceChromeOSDelegate> vcd_delegate_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(VideoCaptureDeviceChromeOSHalv3);
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_CHROMEOS_VIDEO_CAPTURE_DEVICE_CHROMEOS_HALV3_H_
