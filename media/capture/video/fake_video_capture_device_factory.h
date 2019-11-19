// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_FAKE_VIDEO_CAPTURE_DEVICE_FACTORY_H_
#define MEDIA_CAPTURE_VIDEO_FAKE_VIDEO_CAPTURE_DEVICE_FACTORY_H_

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "media/capture/video/fake_video_capture_device.h"
#include "media/capture/video/video_capture_device_factory.h"

namespace gpu {
class GpuMemoryBufferSupport;
}  // namespace gpu

namespace media {

struct CAPTURE_EXPORT FakeVideoCaptureDeviceSettings {
  FakeVideoCaptureDeviceSettings();
  ~FakeVideoCaptureDeviceSettings();
  FakeVideoCaptureDeviceSettings(const FakeVideoCaptureDeviceSettings& other);

  std::string device_id;
  FakeVideoCaptureDevice::DeliveryMode delivery_mode;
  VideoCaptureFormats supported_formats;
  FakePhotoDeviceConfig photo_device_config;
  FakeVideoCaptureDevice::DisplayMediaType display_media_type;
};

// Implementation of VideoCaptureDeviceFactory that creates fake devices
// that generate test output frames.
// By default, the factory has one device outputting I420 with
// USE_DEVICE_INTERNAL_BUFFERS. It supports a default set of resolutions.
// When a resolution is requested that is not part of the default set, it snaps
// to the resolution with the next larger width. It supports all frame rates
// within a default allowed range.
class CAPTURE_EXPORT FakeVideoCaptureDeviceFactory
    : public VideoCaptureDeviceFactory {
 public:
  static constexpr const char kDeviceConfigForGetPhotoStateFails[] =
      "GetPhotoStateFails";
  static constexpr const char kDeviceConfigForSetPhotoOptionsFails[] =
      "SetPhotoOptionsFails";
  static constexpr const char kDeviceConfigForTakePhotoFails[] =
      "TakePhotoFails";

  FakeVideoCaptureDeviceFactory();
  ~FakeVideoCaptureDeviceFactory() override;

  static std::unique_ptr<VideoCaptureDevice> CreateDeviceWithSettings(
      const FakeVideoCaptureDeviceSettings& settings,
      std::unique_ptr<gpu::GpuMemoryBufferSupport> gmb_support = nullptr);

  static std::unique_ptr<VideoCaptureDevice> CreateDeviceWithDefaultResolutions(
      VideoPixelFormat pixel_format,
      FakeVideoCaptureDevice::DeliveryMode delivery_mode,
      float frame_rate,
      std::unique_ptr<gpu::GpuMemoryBufferSupport> gmb_support = nullptr);

  // Creates a device that reports OnError() when AllocateAndStart() is called.
  static std::unique_ptr<VideoCaptureDevice> CreateErrorDevice();

  static void ParseFakeDevicesConfigFromOptionsString(
      const std::string options_string,
      std::vector<FakeVideoCaptureDeviceSettings>* config);

  // All devices use the default set of resolution, the default range for
  // allowed frame rates, as well as USE_DEVICE_INTERNAL_BUFFERS.
  // Device 0 outputs I420.
  // Device 1 outputs Y16.
  // Device 2 outputs MJPEG.
  // All additional devices output I420.
  void SetToDefaultDevicesConfig(int device_count);
  void SetToCustomDevicesConfig(
      const std::vector<FakeVideoCaptureDeviceSettings>& config);

  // VideoCaptureDeviceFactory implementation:
  std::unique_ptr<VideoCaptureDevice> CreateDevice(
      const VideoCaptureDeviceDescriptor& device_descriptor) override;
  void GetDeviceDescriptors(
      VideoCaptureDeviceDescriptors* device_descriptors) override;
  void GetSupportedFormats(
      const VideoCaptureDeviceDescriptor& device_descriptor,
      VideoCaptureFormats* supported_formats) override;
  void GetCameraLocationsAsync(
      std::unique_ptr<VideoCaptureDeviceDescriptors> device_descriptors,
      DeviceDescriptorsCallback result_callback) override;

  int number_of_devices() {
    DCHECK(thread_checker_.CalledOnValidThread());
    return static_cast<int>(devices_config_.size());
  }

 private:
  std::vector<FakeVideoCaptureDeviceSettings> devices_config_;
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_FAKE_VIDEO_CAPTURE_DEVICE_FACTORY_H_
