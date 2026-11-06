// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_IN_PROCESS_VIDEO_CAPTURE_DEVICE_LAUNCHER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_IN_PROCESS_VIDEO_CAPTURE_DEVICE_LAUNCHER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "content/browser/media/capture/pip_screen_capture_coordinator_proxy.h"
#include "content/browser/renderer_host/media/video_capture_controller.h"
#include "content/browser/renderer_host/media/video_capture_provider.h"
#include "content/public/browser/video_capture_device_launcher.h"
#include "media/capture/video/video_capture_device.h"
#include "media/capture/video/video_capture_device_client.h"
#include "media/capture/video/video_capture_device_descriptor.h"
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

  void LaunchDeviceAsync(const std::string& device_id,
                         blink::mojom::MediaStreamType stream_type,
                         const media::VideoCaptureParams& params,
                         base::WeakPtr<media::VideoFrameReceiver> receiver,
                         base::OnceClosure connection_lost_cb,
                         Callbacks* callbacks,
                         base::OnceClosure done_cb) override;

  void AbortLaunch() override;

  // Asks the launched screen, window, or tab capture device to deliver its
  // frames in shared memory instead of GPU buffers. Must be called before
  // LaunchDeviceAsync().
  void SetPreferSharedMemoryBuffers();

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
                       std::unique_ptr<media::VideoCaptureDevice> device);

  static void DoStartTabCaptureOnDeviceThread(
      const std::string& device_id,
      const media::VideoCaptureParams& params,
      bool prefer_shared_memory_buffers,
      std::unique_ptr<media::VideoFrameReceiver> receiver,
      ReceiveDeviceCallback result_callback);

#if defined(USE_AURA) || BUILDFLAG(IS_MAC)
  static void DoStartVizFrameSinkWindowCaptureOnDeviceThread(
      const DesktopMediaID& device_id,
      const media::VideoCaptureParams& params,
      bool prefer_shared_memory_buffers,
      std::unique_ptr<media::VideoFrameReceiver> receiver,
      ReceiveDeviceCallback result_callback);
#endif  // defined(USE_AURA) || BUILDFLAG(IS_MAC)

  static void DoStartDesktopCaptureOnDeviceThread(
      NativeScreenCapturePicker* picker,
      const DesktopMediaID& desktop_id,
      const media::VideoCaptureParams& params,
      std::unique_ptr<media::VideoCaptureDeviceClient> client,
      ReceiveDeviceCallback result_callback);

  static void OnPipScreenCaptureCoordinatorProxyCreated(
      NativeScreenCapturePicker* picker,
      const DesktopMediaID& desktop_id,
      const media::VideoCaptureParams& params,
      std::unique_ptr<media::VideoCaptureDeviceClient> device_client,
      ReceiveDeviceCallback result_callback,
      std::unique_ptr<PipScreenCaptureCoordinatorProxy>
          pip_screen_capture_coordinator_proxy);

  static void DoStartFakeDisplayCaptureOnDeviceThread(
      const DesktopMediaID& desktop_id,
      const media::VideoCaptureParams& params,
      std::unique_ptr<media::VideoCaptureDeviceClient> client,
      ReceiveDeviceCallback result_callback);

  static void OnFakeDevicesEnumerated(
      std::unique_ptr<media::FakeVideoCaptureDeviceFactory> fake_device_factory,
      const media::VideoCaptureParams& params,
      std::unique_ptr<media::VideoCaptureDeviceClient> device_client,
      ReceiveDeviceCallback result_callback,
      std::vector<media::VideoCaptureDeviceInfo> devices_info);

  const scoped_refptr<base::SingleThreadTaskRunner> device_task_runner_;
  State state_;
  // The `done_cb` of LaunchDeviceAsync(). It keeps the caller's context alive
  // until the launch completes, and it can own `this`. It stays on the IO
  // thread: if the result callback from the device thread owned it and the IO
  // thread had stopped, that callback would destroy it on the device thread.
  //
  // If `done_cb_` owns `this`, it makes a reference cycle that only
  // OnDeviceStarted() breaks. So the device thread must run the result
  // callback while the IO thread runs. The shutdown order makes sure of this:
  // the IO thread stops before the device thread (on Mac, the device thread
  // is the UI thread). If the result callback cannot reach the IO thread
  // because the IO thread has stopped, `done_cb_` and the objects that it owns
  // are leaked, instead of destroyed on the wrong thread.
  base::OnceClosure done_cb_;
  // See SetPreferSharedMemoryBuffers().
  bool prefer_shared_memory_buffers_ = false;
  raw_ptr<NativeScreenCapturePicker> native_screen_capture_picker_;
  base::WeakPtrFactory<InProcessVideoCaptureDeviceLauncher> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_IN_PROCESS_VIDEO_CAPTURE_DEVICE_LAUNCHER_H_
