// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Linux specific implementation of VideoCaptureDevice.
// V4L2 is used for capturing. V4L2 does not provide its own thread for
// capturing so this implementation uses a Chromium thread for fetching frames
// from V4L2.

#ifndef MEDIA_CAPTURE_VIDEO_LINUX_VIDEO_CAPTURE_DEVICE_LINUX_H_
#define MEDIA_CAPTURE_VIDEO_LINUX_VIDEO_CAPTURE_DEVICE_LINUX_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "base/threading/thread.h"
#include "media/capture/video/linux/v4l2_capture_device_impl.h"
#include "media/capture/video/video_capture_device.h"
#include "media/capture/video_capture_types.h"

namespace media {

class V4L2CaptureDelegate;

// Linux V4L2 implementation of VideoCaptureDevice.
class VideoCaptureDeviceLinux : public VideoCaptureDevice {
 public:
  static VideoPixelFormat V4l2FourCcToChromiumPixelFormat(uint32_t v4l2_fourcc);
  static std::vector<uint32_t> GetListOfUsableFourCCs(bool favour_mjpeg);

  explicit VideoCaptureDeviceLinux(
      scoped_refptr<V4L2CaptureDevice> v4l2,
      const VideoCaptureDeviceDescriptor& device_descriptor);
  ~VideoCaptureDeviceLinux() override;

  // VideoCaptureDevice implementation.
  void AllocateAndStart(const VideoCaptureParams& params,
                        std::unique_ptr<Client> client) override;
  void StopAndDeAllocate() override;
  void TakePhoto(TakePhotoCallback callback) override;
  void GetPhotoState(GetPhotoStateCallback callback) override;
  void SetPhotoOptions(mojom::PhotoSettingsPtr settings,
                       SetPhotoOptionsCallback callback) override;

 protected:
  virtual void SetRotation(int rotation);

  const VideoCaptureDeviceDescriptor device_descriptor_;

 private:
  const scoped_refptr<V4L2CaptureDevice> v4l2_;

  // Internal delegate doing the actual capture setting, buffer allocation and
  // circulation with the V4L2 API. Created in the thread where
  // VideoCaptureDeviceLinux lives but otherwise operating and deleted on
  // |v4l2_thread_|.
  std::unique_ptr<V4L2CaptureDelegate> capture_impl_;

  // Photo-related requests waiting for |v4l2_thread_| to be active.
  std::vector<base::OnceClosure> photo_requests_queue_;

  base::Thread v4l2_thread_;  // Thread used for reading data from the device.

  // SetRotation() may get called even when the device is not started. When that
  // is the case we remember the value here and use it as soon as the device
  // gets started.
  int rotation_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_IMPLICIT_CONSTRUCTORS(VideoCaptureDeviceLinux);
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_LINUX_VIDEO_CAPTURE_DEVICE_LINUX_H_
