// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_FUCHSIA_VIDEO_CAPTURE_DEVICE_FUCHSIA_H_
#define MEDIA_CAPTURE_VIDEO_FUCHSIA_VIDEO_CAPTURE_DEVICE_FUCHSIA_H_

#include <fuchsia/camera3/cpp/fidl.h>

#include <memory>
#include <optional>

#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "media/capture/video/video_capture_device.h"
#include "media/fuchsia/common/sysmem_client.h"
#include "media/fuchsia/common/vmo_buffer.h"

namespace media {

class CAPTURE_EXPORT VideoCaptureDeviceFuchsia final
    : public VideoCaptureDevice {
 public:
  // Returns pixel format to which video frames in the specified sysmem pixel
  // |format| will be converted. PIXEL_FORMAT_UNKNOWN is returned for
  // unsupported formats.
  static VideoPixelFormat GetConvertedPixelFormat(
      fuchsia::images2::PixelFormat format);
  static VideoPixelFormat GetConvertedPixelFormat(
      fuchsia::sysmem::PixelFormatType format);

  static bool IsSupportedPixelFormat(fuchsia::images2::PixelFormat format);

  explicit VideoCaptureDeviceFuchsia(
      fidl::InterfaceHandle<fuchsia::camera3::Device> device);
  ~VideoCaptureDeviceFuchsia() override;

  VideoCaptureDeviceFuchsia(const VideoCaptureDeviceFuchsia&) = delete;
  VideoCaptureDeviceFuchsia& operator=(const VideoCaptureDeviceFuchsia&) =
      delete;

  // VideoCaptureDevice implementation.
  void AllocateAndStart(const VideoCaptureParams& params,
                        std::unique_ptr<Client> client) override;
  void StopAndDeAllocate() override;

 private:
  // Disconnects the |stream_| and resets related state.
  void DisconnectStream();

  // Reports the specified |error| to the client.
  void OnError(base::Location location,
               VideoCaptureError error,
               const std::string& reason);

  // Error handlers for the |device_| and |stream_|.
  void OnDeviceError(zx_status_t status);
  void OnStreamError(zx_status_t status);

  // Watches for resolution updates and updates |frame_size_| accordingly.
  void WatchResolution();

  // Callback for WatchResolution().
  void OnWatchResolutionResult(fuchsia::math::Size frame_size);

  // Watches for orientation updates and updates |orientation_| accordingly.
  void WatchOrientation();

  // Callback for WatchOrientation().
  void OnWatchOrientationResult(fuchsia::camera3::Orientation orientation);

  // Watches for sysmem buffer collection updates from the camera.
  void WatchBufferCollection();

  // Initializes buffer collection using the specified token. Initialization is
  // asynchronous. |buffer_reader_| will be set once the initialization is
  // complete. Old buffer collection are dropped synchronously (whether they
  // have finished initialization or not).
  void InitializeBufferCollection(
      fidl::InterfaceHandle<fuchsia::sysmem2::BufferCollectionToken>
          token_handle);

  // Callback for SysmemCollectionClient::AcquireBuffers().
  void OnBuffersAcquired(
      std::vector<VmoBuffer> buffers,
      const fuchsia::sysmem2::SingleBufferSettings& buffer_settings);

  // Calls Stream::GetNextFrame() in a loop to receive incoming frames.
  void ReceiveNextFrame();

  // Processes new frames received by ReceiveNextFrame() and passes it to the
  // |client_|.
  void ProcessNewFrame(fuchsia::camera3::FrameInfo frame_info);

  fuchsia::camera3::DevicePtr device_;
  fuchsia::camera3::StreamPtr stream_;

  std::unique_ptr<Client> client_;

  SysmemAllocatorClient sysmem_allocator_;
  std::unique_ptr<SysmemCollectionClient> buffer_collection_;
  std::vector<VmoBuffer> buffers_;
  fuchsia::sysmem2::ImageFormatConstraints buffers_format_;

  std::optional<gfx::Size> frame_size_;
  fuchsia::camera3::Orientation orientation_ =
      fuchsia::camera3::Orientation::UP;

  base::TimeTicks start_time_;

  bool started_ = false;

  size_t frames_received_ = 0;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_FUCHSIA_VIDEO_CAPTURE_DEVICE_FUCHSIA_H_