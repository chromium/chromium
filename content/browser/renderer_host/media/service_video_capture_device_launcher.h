// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_SERVICE_VIDEO_CAPTURE_DEVICE_LAUNCHER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_SERVICE_VIDEO_CAPTURE_DEVICE_LAUNCHER_H_

#include "content/browser/renderer_host/media/ref_counted_video_source_provider.h"
#include "content/browser/renderer_host/media/video_capture_provider.h"
#include "content/public/browser/video_capture_device_launcher.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/video_capture/public/mojom/device_factory.mojom.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"

namespace content {

// Implementation of VideoCaptureDeviceLauncher that uses uses
// video_capture::mojom::VideoCaptureService.
class CONTENT_EXPORT ServiceVideoCaptureDeviceLauncher
    : public VideoCaptureDeviceLauncher {
 public:
  // Receives an instance via output parameter |out_provider|.
  using ConnectToDeviceFactoryCB = base::RepeatingCallback<void(
      scoped_refptr<RefCountedVideoSourceProvider>* out_provider)>;

  explicit ServiceVideoCaptureDeviceLauncher(
      ConnectToDeviceFactoryCB connect_to_source_provider_cb);
  ~ServiceVideoCaptureDeviceLauncher() override;

  // VideoCaptureDeviceLauncher implementation.
  void LaunchDeviceAsync(const std::string& device_id,
                         blink::mojom::MediaStreamType stream_type,
                         const media::VideoCaptureParams& params,
                         base::WeakPtr<media::VideoFrameReceiver> receiver,
                         base::OnceClosure connection_lost_cb,
                         Callbacks* callbacks,
                         base::OnceClosure done_cb) override;
  void AbortLaunch() override;

  void OnUtilizationReport(int frame_feedback_id, double utilization);

 private:
  enum class State {
    READY_TO_LAUNCH,
    DEVICE_START_IN_PROGRESS,
    DEVICE_START_ABORTING
  };

  void OnCreatePushSubscriptionCallback(
      mojo::Remote<video_capture::mojom::VideoSource> source,
      mojo::Remote<video_capture::mojom::PushVideoStreamSubscription>
          subscription,
      base::OnceClosure connection_lost_cb,
      video_capture::mojom::CreatePushSubscriptionResultCode result_code,
      const media::VideoCaptureParams& params);

  void OnConnectionLostWhileWaitingForCallback();

  ConnectToDeviceFactoryCB connect_to_source_provider_cb_;
  scoped_refptr<RefCountedVideoSourceProvider> service_connection_;
  State state_;
  base::SequenceChecker sequence_checker_;
  base::OnceClosure done_cb_;
  Callbacks* callbacks_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_SERVICE_VIDEO_CAPTURE_DEVICE_LAUNCHER_H_
