// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_IN_PROCESS_VIDEO_CAPTURE_DEVICE_LAUNCHER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_IN_PROCESS_VIDEO_CAPTURE_DEVICE_LAUNCHER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "content/browser/renderer_host/media/video_capture_controller.h"
#include "content/browser/renderer_host/media/video_capture_provider.h"
#include "content/public/browser/video_capture_device_launcher.h"
#include "media/capture/video/video_capture_device.h"
#include "media/capture/video/video_capture_device_client.h"
#include "media/capture/video/video_capture_device_descriptor.h"
#include "services/video_effects/public/mojom/video_effects_processor.mojom-forward.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"

namespace media {
class FakeVideoCaptureDeviceFactory;
}  // namespace media

namespace content {

struct DesktopMediaID;
class NativeScreenCapturePicker;

// Implementation of BuildableVideoCaptureDevice that creates capture devices
// in the same process as it is being operated on, which must be the Browser
// process. The devices are operated on the given |device_task_runner|.
// Instances of this class must be operated from the Browser process IO thread.
class InProcessVideoCaptureDeviceLauncher : public VideoCaptureDeviceLauncher {
 public:
  InProcessVideoCaptureDeviceLauncher(
      scoped_refptr<base::SingleThreadTaskRunner> device_task_runner,
      NativeScreenCapturePicker* picker);
  ~InProcessVideoCaptureDeviceLauncher() override;

  void LaunchDeviceAsync(
      const std::string& device_id,
      blink::mojom::MediaStreamType stream_type,
      const media::VideoCaptureParams& params,
      base::WeakPtr<media::VideoFrameReceiver> receiver,
      base::OnceClosure connection_lost_cb,
      Callbacks* callbacks,
      base::OnceClosure done_cb,
      mojo::PendingRemote<video_effects::mojom::VideoEffectsProcessor>
          video_effects_processor) override;

  void AbortLaunch() override;

 private:
  using ReceiveDeviceCallback = base::OnceCallback<void(
      std::unique_ptr<media::VideoCaptureDevice> device)>;

  enum class State {
    READY_TO_LAUNCH,
    DEVICE_START_IN_PROGRESS,
    DEVICE_START_ABORTING
  };

  std::unique_ptr<media::VideoCaptureDeviceClient> CreateDeviceClient(
      media::VideoCaptureBufferType requested_buffer_type,
      int buffer_pool_max_buffer_count,
      std::unique_ptr<media::VideoFrameReceiver> receiver,
      base::WeakPtr<media::VideoFrameReceiver> receiver_on_io_thread);

  void OnDeviceStarted(Callbacks* callbacks,
                       base::OnceClosure done_cb,
                       std::unique_ptr<media::VideoCaptureDevice> device);

  void DoStartTabCaptureOnDeviceThread(
      const std::string& device_id,
      const media::VideoCaptureParams& params,
      std::unique_ptr<media::VideoFrameReceiver> receiver,
      ReceiveDeviceCallback result_callback);

  void DoStartVizFrameSinkWindowCaptureOnDeviceThread(
      const DesktopMediaID& device_id,
      const media::VideoCaptureParams& params,
      std::unique_ptr<media::VideoFrameReceiver> receiver,
      ReceiveDeviceCallback result_callback);

  void DoStartDesktopCaptureOnDeviceThread(
      const DesktopMediaID& desktop_id,
      const media::VideoCaptureParams& params,
      std::unique_ptr<media::VideoCaptureDeviceClient> client,
      ReceiveDeviceCallback result_callback);

  void DoStartDesktopCaptureWithReceiverOnDeviceThread(
      const DesktopMediaID& desktop_id,
      const media::VideoCaptureParams& params,
      std::unique_ptr<media::VideoFrameReceiver> receiver,
      ReceiveDeviceCallback result_callback);

  void DoStartFakeDisplayCaptureOnDeviceThread(
      const DesktopMediaID& desktop_id,
      const media::VideoCaptureParams& params,
      std::unique_ptr<media::VideoCaptureDeviceClient> client,
      ReceiveDeviceCallback result_callback);

  void OnFakeDevicesEnumerated(
      const media::VideoCaptureParams& params,
      std::unique_ptr<media::VideoCaptureDeviceClient> device_client,
      ReceiveDeviceCallback result_callback,
      std::vector<media::VideoCaptureDeviceInfo> devices_info);

  const scoped_refptr<base::SingleThreadTaskRunner> device_task_runner_;
  State state_;
  std::unique_ptr<media::FakeVideoCaptureDeviceFactory> fake_device_factory_;
  raw_ptr<NativeScreenCapturePicker> native_screen_capture_picker_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_IN_PROCESS_VIDEO_CAPTURE_DEVICE_LAUNCHER_H_
