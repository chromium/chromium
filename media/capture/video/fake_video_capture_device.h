// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_FAKE_VIDEO_CAPTURE_DEVICE_H_
#define MEDIA_CAPTURE_VIDEO_FAKE_VIDEO_CAPTURE_DEVICE_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/threading/thread_checker.h"
#include "media/capture/video/video_capture_device.h"

namespace gpu {
class GpuMemoryBufferSupport;
}  // namespace gpu

namespace media {

struct FakeDeviceState;
class FakePhotoDevice;
class FrameDeliverer;
class FrameDelivererFactory;

// Paints a "pacman-like" animated circle including textual information such
// as a frame count and timer.
class PacmanFramePainter {
 public:
  enum class Format { I420, SK_N32, Y16, NV12 };

  PacmanFramePainter(Format pixel_format,
                     const FakeDeviceState* fake_device_state);

  void PaintFrame(base::TimeDelta elapsed_time, uint8_t* target_buffer);

 private:
  void DrawGradientSquares(base::TimeDelta elapsed_time,
                           uint8_t* target_buffer);

  void DrawPacman(base::TimeDelta elapsed_time, uint8_t* target_buffer);

  const Format pixel_format_;
  const FakeDeviceState* fake_device_state_ = nullptr;
};

// Implementation of VideoCaptureDevice that generates test frames. This is
// useful for testing the video capture components without having to use real
// devices. The implementation schedules delayed tasks to itself to generate and
// deliver frames at the requested rate.
class FakeVideoCaptureDevice : public VideoCaptureDevice {
 public:
  enum class DeliveryMode {
    USE_DEVICE_INTERNAL_BUFFERS,
    USE_CLIENT_PROVIDED_BUFFERS,
    USE_GPU_MEMORY_BUFFERS,
  };

  enum class DisplayMediaType { ANY, MONITOR, WINDOW, BROWSER };

  FakeVideoCaptureDevice(
      const VideoCaptureFormats& supported_formats,
      std::unique_ptr<FrameDelivererFactory> frame_deliverer_factory,
      std::unique_ptr<FakePhotoDevice> photo_device,
      std::unique_ptr<FakeDeviceState> device_state);
  ~FakeVideoCaptureDevice() override;

  static void GetSupportedSizes(std::vector<gfx::Size>* supported_sizes);

  // VideoCaptureDevice implementation.
  void AllocateAndStart(const VideoCaptureParams& params,
                        std::unique_ptr<Client> client) override;
  void StopAndDeAllocate() override;
  void GetPhotoState(GetPhotoStateCallback callback) override;
  void SetPhotoOptions(mojom::PhotoSettingsPtr settings,
                       SetPhotoOptionsCallback callback) override;
  void TakePhoto(TakePhotoCallback callback) override;

 private:
  void BeepAndScheduleNextCapture(base::TimeTicks expected_execution_time);
  void OnNextFrameDue(base::TimeTicks expected_execution_time, int session_id);

  const VideoCaptureFormats supported_formats_;
  const std::unique_ptr<FrameDelivererFactory> frame_deliverer_factory_;
  const std::unique_ptr<FakePhotoDevice> photo_device_;
  const std::unique_ptr<FakeDeviceState> device_state_;
  std::unique_ptr<FrameDeliverer> frame_deliverer_;
  int current_session_id_ = 0;

  // Time when the next beep occurs.
  base::TimeDelta beep_time_;
  // Time since the fake video started rendering frames.
  base::TimeDelta elapsed_time_;

  base::ThreadChecker thread_checker_;

  // FakeVideoCaptureDevice post tasks to itself for frame construction and
  // needs to deal with asynchronous StopAndDeallocate().
  base::WeakPtrFactory<FakeVideoCaptureDevice> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FakeVideoCaptureDevice);
};

// Represents the current state of a FakeVideoCaptureDevice.
// This is a separate struct because read-access to it is shared with several
// collaborating classes.
struct FakeDeviceState {
  FakeDeviceState(double pan,
                  double tilt,
                  double zoom,
                  double exposure_time,
                  double focus_distance,
                  float frame_rate,
                  VideoPixelFormat pixel_format)
      : pan(pan),
        tilt(tilt),
        zoom(zoom),
        exposure_time(exposure_time),
        focus_distance(focus_distance),
        format(gfx::Size(), frame_rate, pixel_format) {
    exposure_mode = (exposure_time >= 0.0f) ? mojom::MeteringMode::MANUAL
                                            : mojom::MeteringMode::CONTINUOUS;
    focus_mode = (focus_distance >= 0.0f) ? mojom::MeteringMode::MANUAL
                                          : mojom::MeteringMode::CONTINUOUS;
  }

  double pan;
  double tilt;
  double zoom;
  double exposure_time;
  mojom::MeteringMode exposure_mode;
  double focus_distance;
  mojom::MeteringMode focus_mode;
  VideoCaptureFormat format;
};

// A dependency needed by FakeVideoCaptureDevice.
class FrameDelivererFactory {
 public:
  FrameDelivererFactory(
      FakeVideoCaptureDevice::DeliveryMode delivery_mode,
      const FakeDeviceState* device_state,
      std::unique_ptr<gpu::GpuMemoryBufferSupport> gmb_support);
  ~FrameDelivererFactory();

  std::unique_ptr<FrameDeliverer> CreateFrameDeliverer(
      const VideoCaptureFormat& format,
      bool video_capture_use_gmb);

 private:
  const FakeVideoCaptureDevice::DeliveryMode delivery_mode_;
  const FakeDeviceState* device_state_ = nullptr;
  std::unique_ptr<gpu::GpuMemoryBufferSupport> gmb_support_;
};

struct FakePhotoDeviceConfig {
  FakePhotoDeviceConfig()
      : should_fail_get_photo_capabilities(false),
        should_fail_set_photo_options(false),
        should_fail_take_photo(false) {}

  bool should_fail_get_photo_capabilities;
  bool should_fail_set_photo_options;
  bool should_fail_take_photo;
};

// Implements the photo functionality of a FakeVideoCaptureDevice
class FakePhotoDevice {
 public:
  FakePhotoDevice(std::unique_ptr<PacmanFramePainter> sk_n32_painter,
                  const FakeDeviceState* fake_device_state,
                  const FakePhotoDeviceConfig& config);
  ~FakePhotoDevice();

  void GetPhotoState(VideoCaptureDevice::GetPhotoStateCallback callback);
  void SetPhotoOptions(mojom::PhotoSettingsPtr settings,
                       VideoCaptureDevice::SetPhotoOptionsCallback callback,
                       FakeDeviceState* device_state_write_access);
  void TakePhoto(VideoCaptureDevice::TakePhotoCallback callback,
                 base::TimeDelta elapsed_time);

 private:
  const std::unique_ptr<PacmanFramePainter> sk_n32_painter_;
  const FakeDeviceState* const fake_device_state_;
  const FakePhotoDeviceConfig config_;
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_FAKE_VIDEO_CAPTURE_DEVICE_H_
