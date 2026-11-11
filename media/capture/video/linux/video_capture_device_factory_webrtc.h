// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_LINUX_VIDEO_CAPTURE_DEVICE_FACTORY_WEBRTC_H_
#define MEDIA_CAPTURE_VIDEO_LINUX_VIDEO_CAPTURE_DEVICE_FACTORY_WEBRTC_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "build/config/linux/dbus/buildflags.h"
#include "media/capture/video/video_capture_device_factory.h"
#include "media/capture/video_capture_types.h"
#include "third_party/webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "third_party/webrtc/modules/video_capture/video_capture_options.h"

#if !defined(WEBRTC_USE_PIPEWIRE)
#error "This file is only built when PipeWire is enabled (rtc_use_pipewire)."
#endif

#if BUILDFLAG(USE_DBUS)
#include "components/dbus/xdg/camera_portal.h"
#endif

namespace media {

// Extension of VideoCaptureDeviceFactory to create and manipulate WebRTC
// devices.
class CAPTURE_EXPORT VideoCaptureDeviceFactoryWebRtc
    : public VideoCaptureDeviceFactory,
      webrtc::VideoCaptureOptions::Callback {
 public:
  VideoCaptureDeviceFactoryWebRtc();

  VideoCaptureDeviceFactoryWebRtc(const VideoCaptureDeviceFactoryWebRtc&) =
      delete;
  VideoCaptureDeviceFactoryWebRtc& operator=(
      const VideoCaptureDeviceFactoryWebRtc&) = delete;
  VideoCaptureDeviceFactoryWebRtc& operator=(
      const VideoCaptureDeviceFactoryWebRtc&&) = delete;

  ~VideoCaptureDeviceFactoryWebRtc() override;

  VideoCaptureErrorOrDevice CreateDevice(
      const VideoCaptureDeviceDescriptor& device_descriptor) override;
  void GetDevicesInfo(GetDevicesInfoCallback callback) override;

  void OnInitialized(webrtc::VideoCaptureOptions::Status status) override;

  // Check if the necessary services are available on the system.
  // Specifically this means that PipeWire and xdg-desktop-portal are running
  // and that the Camera portal granted access. It is false if the portal is
  // missing, fails or denies access. The return will always be true at first
  // and may change during device enumeration. So it should be checked again in
  // the GetDevicesInfo() callback.
  bool IsAvailable();

  // True after the Camera portal client was denied access, until the next
  // GetDevicesInfo() call retries the portal. GNOME Shell refuses the request
  // while no window of the app is focused, e.g. at startup, and that looks
  // like a denial. A retry after an actual denial gets the stored answer
  // without a dialog. There is one retry per denial from the portal client.
  bool IsPortalRetryPending() const {
    return portal_retry_ == PortalRetry::kPending;
  }

  static webrtc::VideoType WebRtcVideoTypeFromChromiumPixelFormat(
      VideoPixelFormat pixel_format);
  static VideoPixelFormat WebRtcVideoTypeToChromiumPixelFormat(
      webrtc::VideoType video_type);

 protected:
  // Starts the asynchronous PipeWire and Camera portal initialization, which
  // finishes with OnInitialized(). Virtual for testing.
  virtual void InitializeOptions();

  // Initializes `options_` with the PipeWire remote `pipewire_fd`, or lets
  // WebRTC open the Camera portal itself if it is invalid. Takes ownership of
  // `pipewire_fd`. Virtual for testing.
  virtual void InitOptions(int pipewire_fd);

#if BUILDFLAG(USE_DBUS)
  // Opens the Camera portal on a dedicated D-Bus connection. Virtual for
  // testing.
  virtual void OpenCameraPortal(dbus_xdg::CameraPortalCallback callback);
#endif

 private:
#if BUILDFLAG(USE_DBUS)
  void OnCameraPortalResult(dbus_xdg::CameraPortalResult result);
#endif

  // Collect the camera devices and call pending GetDevicesInfo() callbacks.
  void FinishGetDevicesInfo();
  std::vector<GetDevicesInfoCallback> callbacks_;

  std::unique_ptr<webrtc::VideoCaptureOptions> options_;
  webrtc::VideoCaptureOptions::Status status_{
      webrtc::VideoCaptureOptions::Status::UNINITIALIZED};

  scoped_refptr<base::SingleThreadTaskRunner> origin_task_runner_;

#if BUILDFLAG(USE_DBUS)
  std::unique_ptr<dbus_xdg::CameraPortal> camera_portal_;
#endif

  // The single Camera portal retry after a denial (see
  // IsPortalRetryPending()).
  enum class PortalRetry {
    // No denial so far.
    kUnused,
    // The portal client was denied; the next GetDevicesInfo() retries.
    kPending,
    // The retry has started, so a further denial is final.
    kUsed,
  };
  PortalRetry portal_retry_ = PortalRetry::kUnused;

  base::WeakPtrFactory<VideoCaptureDeviceFactoryWebRtc> weak_ptr_factory_{this};
};

}  // namespace media
#endif  // MEDIA_CAPTURE_VIDEO_LINUX_VIDEO_CAPTURE_DEVICE_FACTORY_WEBRTC_H_
