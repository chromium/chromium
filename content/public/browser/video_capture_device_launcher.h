// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_VIDEO_CAPTURE_DEVICE_LAUNCHER_H_
#define CONTENT_PUBLIC_BROWSER_VIDEO_CAPTURE_DEVICE_LAUNCHER_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/task/single_thread_task_runner.h"
#include "base/token.h"
#include "content/common/content_export.h"
#include "media/capture/mojom/video_capture_types.mojom.h"
#include "media/capture/video/video_capture_device.h"
#include "media/capture/video/video_capture_device_info.h"
#include "media/capture/video/video_frame_receiver.h"
#include "media/capture/video_capture_types.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/video_effects/public/mojom/video_effects_processor.mojom-forward.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "ui/gfx/native_widget_types.h"

namespace content {

class LaunchedVideoCaptureDevice;

// Asynchronously launches video capture devices. After a call to
// LaunchDeviceAsync() it is illegal to call LaunchDeviceAsync() again until
// |callbacks| has been notified about the outcome of the asynchronous launch.
class CONTENT_EXPORT VideoCaptureDeviceLauncher {
 public:
  class CONTENT_EXPORT Callbacks {
   public:
    virtual ~Callbacks() {}
    virtual void OnDeviceLaunched(
        std::unique_ptr<LaunchedVideoCaptureDevice> device) = 0;
    virtual void OnDeviceLaunchFailed(media::VideoCaptureError error) = 0;
    virtual void OnDeviceLaunchAborted() = 0;
  };

  virtual ~VideoCaptureDeviceLauncher() {}

  // Creates an InProcessVideoCaptureDeviceLauncher.
  static std::unique_ptr<VideoCaptureDeviceLauncher>
  CreateInProcessVideoCaptureDeviceLauncher(
      scoped_refptr<base::SingleThreadTaskRunner> device_task_runner);

  // The passed-in `done_cb` must guarantee that the context relevant
  // during the asynchronous processing stays alive.
  //
  // The passed-in `video_effects_processor` remote is passed on to
  // `VideoCaptureDeviceClient`, allowing it to request post-processing of
  // video frames according to the effects configuration set on the
  // VideoEffectsProcessor by the Browser process. The remote won't
  // be bound to a receiver if the `VideoCaptureHost` couldn't get a valid
  // `content::BrowserContext`.
  virtual void LaunchDeviceAsync(
      const std::string& device_id,
      blink::mojom::MediaStreamType stream_type,
      const media::VideoCaptureParams& params,
      base::WeakPtr<media::VideoFrameReceiver> receiver,
      base::OnceClosure connection_lost_cb,
      Callbacks* callbacks,
      base::OnceClosure done_cb,
      mojo::PendingRemote<video_effects::mojom::VideoEffectsProcessor>
          video_effects_processor) = 0;

  virtual void AbortLaunch() = 0;
};

class CONTENT_EXPORT LaunchedVideoCaptureDevice
    : public media::VideoFrameConsumerFeedbackObserver {
 public:
  // Device operation methods.
  virtual void GetPhotoState(
      media::VideoCaptureDevice::GetPhotoStateCallback callback) = 0;
  virtual void SetPhotoOptions(
      media::mojom::PhotoSettingsPtr settings,
      media::VideoCaptureDevice::SetPhotoOptionsCallback callback) = 0;
  virtual void TakePhoto(
      media::VideoCaptureDevice::TakePhotoCallback callback) = 0;
  virtual void MaybeSuspendDevice() = 0;
  virtual void ResumeDevice() = 0;
  virtual void ApplySubCaptureTarget(
      media::mojom::SubCaptureTargetType type,
      const base::Token& target,
      uint32_t sub_capture_target_version,
      base::OnceCallback<void(media::mojom::ApplySubCaptureTargetResult)>
          callback) = 0;
  virtual void RequestRefreshFrame() = 0;

  // Methods for specific types of devices.
  virtual void SetDesktopCaptureWindowIdAsync(gfx::NativeViewId window_id,
                                              base::OnceClosure done_cb) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_VIDEO_CAPTURE_DEVICE_LAUNCHER_H_
