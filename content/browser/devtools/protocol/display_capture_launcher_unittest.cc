// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/display_capture_launcher.h"

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "media/capture/video/video_frame_receiver.h"
#include "media/capture/video_capture_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content::protocol {

namespace {

// A receiver that records the error and the thread on which it arrives.
class FakeVideoFrameReceiver : public media::VideoFrameReceiver {
 public:
  explicit FakeVideoFrameReceiver(base::OnceClosure on_error = {})
      : on_error_(std::move(on_error)) {}

  base::WeakPtr<media::VideoFrameReceiver> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  const std::optional<media::VideoCaptureError>& error() const {
    return error_;
  }
  bool error_on_ui_thread() const { return error_on_ui_thread_; }

  // media::VideoFrameReceiver:
  void OnNewBuffer(int buffer_id,
                   media::mojom::VideoBufferHandlePtr buffer_handle) override {}
  void OnFrameReadyInBuffer(media::ReadyFrameInBuffer frame) override {}
  void OnBufferRetired(int buffer_id) override {}
  void OnCaptureConfigurationChanged() override {}
  void OnError(media::VideoCaptureError error) override {
    error_ = error;
    error_on_ui_thread_ = BrowserThread::CurrentlyOn(BrowserThread::UI);
    if (on_error_) {
      std::move(on_error_).Run();
    }
  }
  void OnFrameDropped(media::VideoCaptureFrameDropReason reason) override {}
  void OnNewCaptureVersion(media::CaptureVersion capture_version) override {}
  void OnFrameWithEmptyRegionCapture() override {}
  void OnLog(const std::string& message) override {}
  void OnStarted() override {}
  void OnStartedUsingGpuDecode() override {}
  void OnStopped() override {}

 private:
  base::OnceClosure on_error_;
  std::optional<media::VideoCaptureError> error_;
  bool error_on_ui_thread_ = false;
  base::WeakPtrFactory<FakeVideoFrameReceiver> weak_factory_{this};
};

media::VideoCaptureParams MakeParams() {
  media::VideoCaptureParams params;
  params.requested_format = media::VideoCaptureFormat(gfx::Size(800, 600), 30,
                                                      media::PIXEL_FORMAT_ARGB);
  params.resolution_change_policy =
      media::ResolutionChangePolicy::ANY_WITHIN_LIMIT;
  return params;
}

class DisplayCaptureLauncherTest : public testing::Test {
 protected:
  BrowserTaskEnvironment task_environment_{
      BrowserTaskEnvironment::REAL_IO_THREAD};
};

}  // namespace

// Checks that the receiver gets an error on the UI thread when the device
// cannot be launched. Unit tests have no MediaStreamManager, so the launcher
// cannot be created.
TEST_F(DisplayCaptureLauncherTest, ReportsErrorWhenLaunchFails) {
  base::RunLoop run_loop;
  FakeVideoFrameReceiver receiver(run_loop.QuitClosure());
  DisplayCaptureLauncher device("window:1:0", MakeParams(),
                                receiver.GetWeakPtr());
  run_loop.Run();
  EXPECT_TRUE(receiver.error().has_value());
  EXPECT_TRUE(receiver.error_on_ui_thread());
}

// Checks that the device and the receiver can be destroyed before the launch
// completes on the IO thread.
TEST_F(DisplayCaptureLauncherTest, DestroyBeforeLaunchCompletes) {
  auto receiver = std::make_unique<FakeVideoFrameReceiver>();
  auto device = std::make_unique<DisplayCaptureLauncher>(
      "window:1:0", MakeParams(), receiver->GetWeakPtr());
  device->RequestRefreshFrame();
  device.reset();
  receiver.reset();
  // Wait until the IO thread has run the launch and deleted the core, and
  // the UI thread has run the tasks that they posted.
  base::RunLoop run_loop;
  GetIOThreadTaskRunner({})->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                              run_loop.QuitClosure());
  run_loop.Run();
}

}  // namespace content::protocol
