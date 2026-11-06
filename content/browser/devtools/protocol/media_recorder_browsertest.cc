// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/media_recorder.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "build/build_config.h"
#include "content/browser/devtools/devtools_io_context.h"
#include "content/browser/devtools/protocol/display_capture_launcher.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/browser/media_stream_request.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/shell/browser/shell.h"
#include "media/base/video_frame.h"
#include "media/capture/mojom/video_capture_buffer.mojom.h"
#include "media/capture/video/video_frame_receiver_types.h"
#include "media/media_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-shared.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

// Tests MediaRecorder::StartDisplayCapture() and how it uses the screen
// sharing UI. The tests use a window ID that does not exist, so the device
// does not launch; the UI calls happen before the launch fails. These are
// browser tests, because the recording starts the media encoding service.

namespace content::protocol {

#if BUILDFLAG(ENABLE_LIBAOM)
namespace {

// What a FakeMediaStreamUI received. It stays valid after the UI is destroyed.
struct FakeMediaStreamUIState {
  bool started = false;
  bool destroyed = false;
  base::RepeatingClosure stop;
  MediaStreamUI::StateChangeCallback state_change;
  std::string label;
  std::vector<DesktopMediaID> screen_capture_ids;
  int device_stopped_count = 0;
  std::string device_stopped_label;
  DesktopMediaID device_stopped_media_id;
};

class FakeMediaStreamUI : public MediaStreamUI {
 public:
  explicit FakeMediaStreamUI(FakeMediaStreamUIState* state) : state_(state) {}
  ~FakeMediaStreamUI() override { state_->destroyed = true; }

  gfx::NativeViewId OnStarted(base::RepeatingClosure stop,
                              SourceCallback source,
                              const std::string& label,
                              std::vector<DesktopMediaID> screen_capture_ids,
                              StateChangeCallback state_change) override {
    state_->started = true;
    state_->stop = std::move(stop);
    state_->state_change = std::move(state_change);
    state_->label = label;
    state_->screen_capture_ids = std::move(screen_capture_ids);
    return 0;
  }
  void OnDeviceStoppedForSourceChange(
      const std::string& label,
      const DesktopMediaID& old_media_id,
      const DesktopMediaID& new_media_id,
      bool captured_surface_control_active) override {}
  void OnDeviceStopped(const std::string& label,
                       const DesktopMediaID& media_id) override {
    ++state_->device_stopped_count;
    state_->device_stopped_label = label;
    state_->device_stopped_media_id = media_id;
  }

 private:
  raw_ptr<FakeMediaStreamUIState> state_;
};

DesktopMediaID WindowId() {
  return DesktopMediaID(DesktopMediaID::TYPE_WINDOW, 42);
}

}  // namespace

class MediaRecorderBrowserTest : public ContentBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    recorder_ =
        std::make_unique<MediaRecorder>(&io_context_, base::DoNothing());
  }
  void TearDownOnMainThread() override {
    recorder_.reset();
    io_context_.DiscardAllStreams();
    // A launch that is pending at shutdown cannot complete, because the IO
    // thread stops before the device thread. Wait for it, so that the test
    // does not depend on the shutdown timing.
    EXPECT_TRUE(base::test::RunUntil([] {
      return DisplayCaptureLauncher::GetPendingLaunchCountForTesting() == 0;
    }));
  }

  MediaRecorder* recorder() { return recorder_.get(); }

  DevToolsIOContext io_context_;
  std::unique_ptr<MediaRecorder> recorder_;
};

#if BUILDFLAG(IS_ANDROID)
// Checks that Android rejects a display capture instead of launching the
// device, which needs a MediaProjection grant from the user, and that it does
// not show the UI.
IN_PROC_BROWSER_TEST_F(MediaRecorderBrowserTest, DisplayCaptureNotSupported) {
  FakeMediaStreamUIState state;
  bool answered = false;
  bool started = false;
  recorder()->StartDisplayCapture(
      WindowId(), shell()->web_contents(),
      std::make_unique<FakeMediaStreamUI>(&state), /*audio=*/false,
      /*max_width=*/800, /*max_height=*/600, /*frame_rate=*/30,
      base::BindLambdaForTesting([&](Response response) {
        answered = true;
        started = response.IsSuccess();
      }));
  EXPECT_TRUE(answered);
  EXPECT_FALSE(started);
  EXPECT_TRUE(recorder()->GetStream().empty());
  EXPECT_FALSE(state.started);
  EXPECT_TRUE(state.destroyed);
}
#else
// Checks that a display capture creates the stream, that the start fails
// when the device fails to launch before the first frame, and that Stop()
// still returns the stream.
IN_PROC_BROWSER_TEST_F(MediaRecorderBrowserTest, DisplayCaptureStartAndStop) {
  bool answered = false;
  bool started = false;
  recorder()->StartDisplayCapture(
      WindowId(), shell()->web_contents(), /*ui=*/nullptr, /*audio=*/false,
      /*max_width=*/800, /*max_height=*/600, /*frame_rate=*/30,
      base::BindLambdaForTesting([&](Response response) {
        answered = true;
        started = response.IsSuccess();
      }));
  EXPECT_TRUE(base::test::RunUntil([&] { return answered; }));
  EXPECT_FALSE(started);
  const std::string stream = recorder()->GetStream();
  EXPECT_FALSE(stream.empty());

  std::string stopped_stream;
  base::RunLoop run_loop;
  recorder()->Stop(base::BindLambdaForTesting([&](std::string handle) {
    stopped_stream = handle;
    run_loop.Quit();
  }));
  run_loop.Run();
  EXPECT_EQ(stream, stopped_stream);
}

// Checks that a device error removes the UI, as Stop in the UI does. The
// window ID does not exist, so the launch fails.
IN_PROC_BROWSER_TEST_F(MediaRecorderBrowserTest, DisplayCaptureErrorRemovesUI) {
  FakeMediaStreamUIState state;
  recorder()->StartDisplayCapture(WindowId(), shell()->web_contents(),
                                  std::make_unique<FakeMediaStreamUI>(&state),
                                  /*audio=*/false,
                                  /*max_width=*/800, /*max_height=*/600,
                                  /*frame_rate=*/30, base::DoNothing());
  ASSERT_TRUE(state.started);
  EXPECT_TRUE(base::test::RunUntil([&] { return state.destroyed; }));
  EXPECT_EQ(1, state.device_stopped_count);
  EXPECT_EQ(state.label, state.device_stopped_label);
}

// Checks that the UI gets a unique label, the captured window, and the
// callbacks that DLP needs.
IN_PROC_BROWSER_TEST_F(MediaRecorderBrowserTest, DisplayCaptureStartsUI) {
  FakeMediaStreamUIState state;
  recorder()->StartDisplayCapture(WindowId(), shell()->web_contents(),
                                  std::make_unique<FakeMediaStreamUI>(&state),
                                  /*audio=*/false,
                                  /*max_width=*/800, /*max_height=*/600,
                                  /*frame_rate=*/30, base::DoNothing());
  ASSERT_TRUE(state.started);
  EXPECT_TRUE(state.stop);
  EXPECT_TRUE(state.state_change);
  EXPECT_FALSE(state.label.empty());
  EXPECT_EQ(std::vector<DesktopMediaID>{WindowId()}, state.screen_capture_ids);

  // Pausing and resuming must not crash, also before the device launched.
  state.state_change.Run(WindowId(),
                         blink::mojom::MediaStreamStateChange::PAUSE);
  state.state_change.Run(WindowId(),
                         blink::mojom::MediaStreamStateChange::PLAY);
  // Run the posted state changes.
  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();

  FakeMediaStreamUIState other_state;
  auto other_recorder =
      std::make_unique<MediaRecorder>(&io_context_, base::DoNothing());
  other_recorder->StartDisplayCapture(
      WindowId(), shell()->web_contents(),
      std::make_unique<FakeMediaStreamUI>(&other_state), /*audio=*/false,
      /*max_width=*/800, /*max_height=*/600, /*frame_rate=*/30,
      base::DoNothing());
  ASSERT_TRUE(other_state.started);
  EXPECT_NE(state.label, other_state.label);

  // Destroy the UIs while their states still exist.
  other_recorder.reset();
  recorder_.reset();
}

// Checks that Stop in the UI does not destroy the UI while the UI runs the
// callback, and that the UI learns that the device stopped.
IN_PROC_BROWSER_TEST_F(MediaRecorderBrowserTest, DisplayCaptureStopFromUI) {
  FakeMediaStreamUIState state;
  recorder()->StartDisplayCapture(WindowId(), shell()->web_contents(),
                                  std::make_unique<FakeMediaStreamUI>(&state),
                                  /*audio=*/false,
                                  /*max_width=*/800, /*max_height=*/600,
                                  /*frame_rate=*/30, base::DoNothing());
  ASSERT_TRUE(state.started);

  state.stop.Run();
  EXPECT_FALSE(state.destroyed);
  EXPECT_EQ(0, state.device_stopped_count);

  EXPECT_TRUE(base::test::RunUntil([&] { return state.destroyed; }));
  EXPECT_EQ(1, state.device_stopped_count);
  EXPECT_EQ(state.label, state.device_stopped_label);
  EXPECT_EQ(WindowId(), state.device_stopped_media_id);
}

// Checks that Page.stopScreenRecording tells the UI that the device stopped
// before it destroys the UI.
IN_PROC_BROWSER_TEST_F(MediaRecorderBrowserTest, DisplayCaptureStopTellsUI) {
  FakeMediaStreamUIState state;
  recorder()->StartDisplayCapture(WindowId(), shell()->web_contents(),
                                  std::make_unique<FakeMediaStreamUI>(&state),
                                  /*audio=*/false,
                                  /*max_width=*/800, /*max_height=*/600,
                                  /*frame_rate=*/30, base::DoNothing());
  ASSERT_TRUE(state.started);

  recorder()->Stop(base::DoNothing());
  EXPECT_TRUE(state.destroyed);
  EXPECT_EQ(1, state.device_stopped_count);
  EXPECT_EQ(state.label, state.device_stopped_label);
}

// Checks that a capture error after the first frame removes the UI, as Stop in
// the UI does. The window ID does not exist, so the launch fails after the
// test delivers a frame.
IN_PROC_BROWSER_TEST_F(MediaRecorderBrowserTest,
                       DisplayCaptureErrorAfterFirstFrameRemovesUI) {
  FakeMediaStreamUIState state;
  bool started = false;
  recorder()->StartDisplayCapture(
      WindowId(), shell()->web_contents(),
      std::make_unique<FakeMediaStreamUI>(&state), /*audio=*/false,
      /*max_width=*/800, /*max_height=*/600, /*frame_rate=*/30,
      base::BindLambdaForTesting(
          [&](Response response) { started = response.IsSuccess(); }));
  ASSERT_TRUE(state.started);

  // Deliver a frame before the launch fails on the IO thread.
  const int kBufferId = 1;
  const gfx::Size kSize(320, 240);
  auto region = base::ReadOnlySharedMemoryRegion::Create(
      media::VideoFrame::AllocationSize(media::PIXEL_FORMAT_I420, kSize));
  ASSERT_TRUE(region.IsValid());
  recorder()->OnNewBuffer(
      kBufferId, media::mojom::VideoBufferHandle::NewReadOnlyShmemRegion(
                     std::move(region.region)));
  recorder()->OnFrameReadyInBuffer(media::ReadyFrameInBuffer(
      kBufferId, 0, nullptr,
      media::mojom::VideoFrameInfo::New(
          base::Milliseconds(10), media::VideoFrameMetadata(),
          media::PIXEL_FORMAT_I420, kSize, gfx::Rect(kSize),
          /*natural_size=*/kSize, /*is_premapped=*/false,
          gfx::ColorSpace::CreateREC709(), nullptr)));
  // The start answer is posted.
  EXPECT_TRUE(base::test::RunUntil([&] { return started; }));

  EXPECT_TRUE(base::test::RunUntil([&] { return state.destroyed; }));
  EXPECT_EQ(1, state.device_stopped_count);
  EXPECT_EQ(state.label, state.device_stopped_label);
}
#endif  // BUILDFLAG(IS_ANDROID)
#endif  // BUILDFLAG(ENABLE_LIBAOM)

}  // namespace content::protocol
