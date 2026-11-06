// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/display_capture_launcher.h"

#include <atomic>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/video_capture_manager.h"
#include "content/browser/renderer_host/media/video_capture_provider.h"
#include "content/public/browser/video_capture_device_launcher.h"
#include "media/capture/video/video_frame_receiver_on_task_runner.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-shared.h"

namespace content::protocol {

namespace {

// See GetPendingLaunchCountForTesting(). The UI thread increments it before it
// posts the launch, and the IO thread decrements it when the launch completes
// or cannot start.
constinit std::atomic<int> g_pending_launch_count_for_testing{0};

// Forwards the frames from the IO thread to a receiver on another thread.
// The launcher needs a WeakPtr to the receiver on the IO thread.
class ForwardingReceiver : public media::VideoFrameReceiverOnTaskRunner {
 public:
  using media::VideoFrameReceiverOnTaskRunner::VideoFrameReceiverOnTaskRunner;

  base::WeakPtr<media::VideoFrameReceiver> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<ForwardingReceiver> weak_factory_{this};
};

}  // namespace

// Lives on the IO thread.
class DisplayCaptureLauncher::Core {
 public:
  explicit Core(base::WeakPtr<media::VideoFrameReceiver> receiver)
      : receiver_(receiver, GetUIThreadTaskRunner({})) {}
  Core(const Core&) = delete;
  Core& operator=(const Core&) = delete;
  ~Core() = default;

  void Launch(const std::string& device_id,
              const media::VideoCaptureParams& params) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    // The recording encodes the frames on the CPU, so it needs them in
    // shared memory.
    MediaStreamManager* const media_stream_manager =
        MediaStreamManager::GetInstance();
    std::unique_ptr<VideoCaptureDeviceLauncher> launcher;
    if (media_stream_manager && media_stream_manager->video_capture_manager()) {
      launcher = media_stream_manager->video_capture_manager()
                     ->video_capture_provider()
                     .CreateDeviceLauncherWithSharedMemoryBuffers();
    }
    if (!launcher) {
      DLOG(ERROR) << "Cannot create the display capture device launcher.";
      receiver_.OnError(
          media::VideoCaptureError::kVideoCaptureSystemDeviceIdNotFound);
      --g_pending_launch_count_for_testing;
      return;
    }
    VideoCaptureDeviceLauncher* const launcher_ptr = launcher.get();
    auto callbacks =
        std::make_unique<LauncherCallbacks>(weak_factory_.GetWeakPtr());
    LauncherCallbacks* const callbacks_ptr = callbacks.get();
    launcher_ptr->LaunchDeviceAsync(
        device_id, blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE, params,
        receiver_.GetWeakPtr(), base::DoNothing(), callbacks_ptr,
        // The launcher and the callbacks must stay alive until the launch
        // completes, even if this object is destroyed before.
        base::BindOnce(
            [](std::unique_ptr<VideoCaptureDeviceLauncher>,
               std::unique_ptr<LauncherCallbacks>) {
              // This works only if the launcher runs `done_cb` for every
              // launch, as VideoCaptureDeviceLauncher requires. If a launcher
              // drops it instead, tests that wait for the count time out.
              --g_pending_launch_count_for_testing;
            },
            std::move(launcher), std::move(callbacks)));
  }

  void RequestRefreshFrame() {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    if (device_) {
      device_->RequestRefreshFrame();
    }
  }

  void SetSuspended(bool suspended) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    suspended_ = suspended;
    if (!device_) {
      // OnDeviceLaunched() applies the state.
      return;
    }
    if (suspended_) {
      device_->MaybeSuspendDevice();
    } else {
      device_->ResumeDevice();
    }
  }

 private:
  class LauncherCallbacks : public VideoCaptureDeviceLauncher::Callbacks {
   public:
    explicit LauncherCallbacks(base::WeakPtr<Core> core)
        : core_(std::move(core)) {}

    // VideoCaptureDeviceLauncher::Callbacks:
    void OnDeviceLaunched(
        std::unique_ptr<LaunchedVideoCaptureDevice> device) override {
      DCHECK_CURRENTLY_ON(BrowserThread::IO);
      // If `core_` is gone, `device` is destroyed here, which stops it.
      if (core_) {
        core_->device_ = std::move(device);
        if (core_->suspended_) {
          core_->device_->MaybeSuspendDevice();
        } else {
          core_->device_->RequestRefreshFrame();
        }
      }
    }
    void OnDeviceLaunchFailed(media::VideoCaptureError error) override {
      DLOG(ERROR) << "Cannot launch the display capture device: "
                  << static_cast<int>(error);
      if (core_) {
        core_->receiver_.OnError(error);
      }
    }
    void OnDeviceLaunchAborted() override {
      if (core_) {
        core_->receiver_.OnError(
            media::VideoCaptureError::kVideoCaptureSystemDeviceIdNotFound);
      }
    }

   private:
    base::WeakPtr<Core> core_;
  };

  ForwardingReceiver receiver_;
  // Stops the device when it is destroyed.
  std::unique_ptr<LaunchedVideoCaptureDevice> device_;
  // Whether the device must be suspended, also if it launches later.
  bool suspended_ = false;
  base::WeakPtrFactory<Core> weak_factory_{this};
};

DisplayCaptureLauncher::DisplayCaptureLauncher(
    const std::string& device_id,
    const media::VideoCaptureParams& params,
    base::WeakPtr<media::VideoFrameReceiver> receiver)
    : core_(new Core(std::move(receiver))) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Count the launch before it is posted, so that a test that waits for the
  // count to be zero also waits for a launch that the IO thread has not
  // started yet.
  ++g_pending_launch_count_for_testing;
  // `core_` is deleted on the IO thread, after this task.
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&Core::Launch, base::Unretained(core_.get()),
                                device_id, params));
}

DisplayCaptureLauncher::~DisplayCaptureLauncher() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

// static
int DisplayCaptureLauncher::GetPendingLaunchCountForTesting() {
  return g_pending_launch_count_for_testing;
}

void DisplayCaptureLauncher::RequestRefreshFrame() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&Core::RequestRefreshFrame,
                                base::Unretained(core_.get())));
}

void DisplayCaptureLauncher::Suspend() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&Core::SetSuspended, base::Unretained(core_.get()), true));
}

void DisplayCaptureLauncher::Resume() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&Core::SetSuspended,
                                base::Unretained(core_.get()), false));
}

}  // namespace content::protocol
