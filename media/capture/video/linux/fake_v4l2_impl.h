// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_LINUX_FAKE_V4L2_IMPL_H_
#define MEDIA_CAPTURE_VIDEO_LINUX_FAKE_V4L2_IMPL_H_

#include <map>
#include <string>

#include <linux/videodev2.h>

#include "base/synchronization/lock.h"
#include "media/capture/capture_export.h"
#include "media/capture/video/linux/v4l2_capture_device.h"
#include "media/capture/video/video_capture_device_descriptor.h"

namespace media {

struct FakeV4L2DeviceConfig {
  explicit FakeV4L2DeviceConfig(const VideoCaptureDeviceDescriptor& descriptor,
                                uint32_t fmt = V4L2_PIX_FMT_YUV420)
      : descriptor(descriptor), v4l2_pixel_format(fmt) {}

  const VideoCaptureDeviceDescriptor descriptor;
  uint32_t v4l2_pixel_format;
};

// Implementation of V4L2CaptureDevice interface that allows configuring fake
// devices useful for testing.
class CAPTURE_EXPORT FakeV4L2Impl : public V4L2CaptureDevice {
 public:
  FakeV4L2Impl();

  void AddDevice(const std::string& device_name,
                 const FakeV4L2DeviceConfig& config);

  // Implementation of V4L2CaptureDevice interface:
  int open(const char* device_name, int flags) override;
  int close(int fd) override;
  int ioctl(int fd, int request, void* argp) override;
  void* mmap(void* start,
             size_t length,
             int prot,
             int flags,
             int fd,
             off_t offset) override;

  int munmap(void* start, size_t length) override;
  int poll(struct pollfd* ufds, unsigned int nfds, int timeout) override;

 protected:
  ~FakeV4L2Impl() override;

 private:
  class OpenedDevice;

  base::Lock lock_;

  int next_id_to_return_from_open_ GUARDED_BY(lock_);
  std::map<std::string, FakeV4L2DeviceConfig> device_configs_ GUARDED_BY(lock_);
  std::map<std::string, int> device_name_to_open_id_map_ GUARDED_BY(lock_);
  std::map<int /*value returned by open()*/, std::unique_ptr<OpenedDevice>>
      opened_devices_ GUARDED_BY(lock_);
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_LINUX_FAKE_V4L2_IMPL_H_
