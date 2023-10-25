// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_VIDEO_CAPTURE_DEVICE_PROXY_LACROS_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_VIDEO_CAPTURE_DEVICE_PROXY_LACROS_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/crosapi/mojom/screen_manager.mojom.h"
#include "chromeos/crosapi/mojom/video_capture.mojom.h"
#include "content/browser/media/capture/receiver_media_to_crosapi_adapter.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/browser/device_service.h"
#include "media/capture/mojom/video_capture_types.mojom.h"
#include "media/capture/video/video_capture_device.h"
#include "media/capture/video/video_frame_receiver.h"
#include "media/capture/video_capture_types.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/wake_lock.mojom.h"
#include "services/video_capture/lacros/video_frame_handler_proxy_lacros.h"
#include "services/viz/public/cpp/compositing/video_capture_target_mojom_traits.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

// Given that lacros-chrome runs on ChromeOS, there are type optimizations that
// we can take advantage of above and beyond a typical webrtc::DesktopCapturer.
//
// This requires implementing the VideoCaptureDevice interface instead of the
// webrtc::DesktopCapturer interface. However, lacros-chrome, as a user-side
// process, doesn't actually have permissions to capture the desktop or windows
// itself. Thus, this class is responsible for acting as the lacros-chrome side
// capturer for the purposes of the video capture stack by proxying the calls
// across the crosapi boundary to the *actual* capturer running in the system-
// side ash-chrome process.
//
// Note that objects of this class are thread-affine (e.g. once created it must
// be used on the same thread).
class CONTENT_EXPORT VideoCaptureDeviceProxyLacros
    : public media::VideoCaptureDevice {
 public:
  explicit VideoCaptureDeviceProxyLacros(const DesktopMediaID& device_id);

  VideoCaptureDeviceProxyLacros(const VideoCaptureDeviceProxyLacros&) = delete;
  VideoCaptureDeviceProxyLacros& operator=(
      const VideoCaptureDeviceProxyLacros&) = delete;

  ~VideoCaptureDeviceProxyLacros() override;

  // Deviation from the VideoCaptureDevice interface: Since the memory pooling
  // provided by a VideoCaptureDevice::Client is not needed, we will provide
  // frames to a VideoFrameReceiver directly.
  void AllocateAndStartWithReceiver(
      const media::VideoCaptureParams& params,
      std::unique_ptr<media::VideoFrameReceiver> receiver);

  // Remaining VideoCaptureDevice implementation.
  void AllocateAndStart(const media::VideoCaptureParams& params,
                        std::unique_ptr<Client> client) final;
  void RequestRefreshFrame() final;
  void MaybeSuspend() final;
  void Resume() final;
  void Crop(const base::Token& crop_id,
            uint32_t sub_capture_target_version,
            base::OnceCallback<void(media::mojom::ApplySubCaptureTargetResult)>
                callback) override;
  void StopAndDeAllocate() final;
  void GetPhotoState(GetPhotoStateCallback callback) final;
  void SetPhotoOptions(media::mojom::PhotoSettingsPtr settings,
                       SetPhotoOptionsCallback callback) final;
  void TakePhoto(TakePhotoCallback callback) final;
  void OnUtilizationReport(media::VideoCaptureFeedback feedback) final;

 private:
  // Helper that logs the given error |message| to the |receiver_adapater_| and
  // then stops capture and this VideoCaptureDevice.
  void OnFatalError(std::string message);

  // Helper that requests wake lock to prevent the display from sleeping while
  // capturing is going on.
  void RequestWakeLock();

  const DesktopMediaID capture_id_;

  // Receives video frames from ash, for translation and propagation into the
  // video capture stack. This is set by AllocateAndStartWithReceiver(), and
  // cleared by StopAndDeAllocate().
  std::unique_ptr<crosapi::mojom::VideoFrameHandler> receiver_adapter_;

  // Set when OnFatalError() is called. This prevents any future
  // AllocateAndStartWithReceiver() calls from succeeding.
  absl::optional<std::string> fatal_error_message_;

  // Note that because we do not run on the main thread and because we want to
  // set a disconnect handler on the screen manager we must bind our own remote
  // rather than using |LacrosService::GetRemote|, which returns a shared remote
  // that is unsuitable for setting a disconnect handler on, and is also thread
  // affine to a different thread.
  mojo::Remote<crosapi::mojom::ScreenManager> screen_manager_;
  mojo::Remote<crosapi::mojom::VideoCaptureDevice> device_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Prevent display sleeping while content capture is in progress.
  mojo::Remote<device::mojom::WakeLock> wake_lock_;

  // Creates WeakPtrs for use on the device thread.
  base::WeakPtrFactory<VideoCaptureDeviceProxyLacros> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_VIDEO_CAPTURE_DEVICE_PROXY_LACROS_H_
