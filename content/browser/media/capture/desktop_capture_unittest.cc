// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/desktop_capture.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "content/public/test/desktop_capture_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libyuv/include/libyuv.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

#if BUILDFLAG(IS_MAC)
#include "base/task/bind_post_task.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#endif

namespace content::desktop_capture {

class FakeDesktopCapturer : public webrtc::DesktopCapturer {
 public:
  FakeDesktopCapturer() = default;
  ~FakeDesktopCapturer() override = default;

  void Start(Callback* callback) override {
    callback_ = callback;
    if (!callback && on_stopped_closure_) {
      std::move(on_stopped_closure_).Run();
    }
  }

  void set_on_stopped_closure(base::OnceClosure on_stopped_closure) {
    on_stopped_closure_ = std::move(on_stopped_closure);
  }

  void CaptureFrame() override {
    capture_frame_called_cnt_++;
    if (callback_ && should_succeed_) {
      auto frame = std::make_unique<webrtc::BasicDesktopFrame>(
          webrtc::DesktopSize(100, 100));
      // ARGBRect fills the frame with a solid ARGB color
      // (0xffff0000 = solid red).
      libyuv::ARGBRect(frame->data(), frame->stride(), 0, 0, 100, 100,
                       0xffff0000);
      callback_->OnCaptureResult(webrtc::DesktopCapturer::Result::SUCCESS,
                                 std::move(frame));
    } else if (callback_) {
      callback_->OnCaptureResult(
          webrtc::DesktopCapturer::Result::ERROR_TEMPORARY, nullptr);
    }
  }

  bool SelectSource(SourceId id) override {
    selected_id_ = id;
    return should_select_source_;
  }

  raw_ptr<Callback> callback_ = nullptr;
  SourceId selected_id_ = 0;
  bool should_select_source_ = true;
  bool should_succeed_ = true;
  int capture_frame_called_cnt_ = 0;
  base::OnceClosure on_stopped_closure_;
};

class DesktopCaptureTest : public testing::Test {
 protected:
  DesktopCaptureTest() = default;
  ~DesktopCaptureTest() override = default;

  base::test::TaskEnvironment task_environment_;
};

TEST_F(DesktopCaptureTest, CaptureSuccess) {
  auto fake_capturer = std::make_unique<FakeDesktopCapturer>();
  FakeDesktopCapturer* capturer_ptr = fake_capturer.get();
  ScopedDesktopCapturerForTesting scoped_capturer(std::move(fake_capturer));

  base::RunLoop run_loop;
  SkBitmap captured_bitmap;

  auto request =
      CaptureScreenshot(DesktopMediaID(DesktopMediaID::TYPE_SCREEN, 123),
                        base::BindOnce(
                            [](base::OnceClosure quit_closure,
                               SkBitmap* out_bitmap, const SkBitmap& bitmap) {
                              *out_bitmap = bitmap;
                              std::move(quit_closure).Run();
                            },
                            run_loop.QuitClosure(), &captured_bitmap));

  ASSERT_TRUE(request);
  run_loop.Run();

  EXPECT_EQ(capturer_ptr->selected_id_, 123);
  EXPECT_EQ(capturer_ptr->capture_frame_called_cnt_, 1);
  EXPECT_FALSE(captured_bitmap.empty());
  EXPECT_EQ(captured_bitmap.width(), 100);
  EXPECT_EQ(captured_bitmap.height(), 100);
  EXPECT_EQ(captured_bitmap.getColor(0, 0), SK_ColorRED);
}

TEST_F(DesktopCaptureTest, CaptureFailure) {
  auto fake_capturer = std::make_unique<FakeDesktopCapturer>();
  fake_capturer->should_succeed_ = false;
  ScopedDesktopCapturerForTesting scoped_capturer(std::move(fake_capturer));

  base::RunLoop run_loop;
  SkBitmap captured_bitmap;

  auto request =
      CaptureScreenshot(DesktopMediaID(DesktopMediaID::TYPE_SCREEN, 123),
                        base::BindOnce(
                            [](base::OnceClosure quit_closure,
                               SkBitmap* out_bitmap, const SkBitmap& bitmap) {
                              *out_bitmap = bitmap;
                              std::move(quit_closure).Run();
                            },
                            run_loop.QuitClosure(), &captured_bitmap));

  run_loop.Run();

  EXPECT_TRUE(captured_bitmap.empty());
}

TEST_F(DesktopCaptureTest, CancelBeforeCapture) {
  base::RunLoop run_loop;
  auto fake_capturer = std::make_unique<FakeDesktopCapturer>();
  fake_capturer->set_on_stopped_closure(run_loop.QuitClosure());
  ScopedDesktopCapturerForTesting scoped_capturer(std::move(fake_capturer));

  bool callback_run = false;
  auto request = CaptureScreenshot(
      DesktopMediaID(DesktopMediaID::TYPE_SCREEN, 123),
      base::BindOnce([](bool* callback_run,
                        const SkBitmap& bitmap) { *callback_run = true; },
                     &callback_run));

  // Destroy the request handle immediately.
  request.reset();

  run_loop.Run();

  // The callback should NOT have run because the request was cancelled.
  EXPECT_FALSE(callback_run);
}

#if BUILDFLAG(IS_MAC)
class DesktopCaptureMacTest : public testing::Test {
 protected:
  DesktopCaptureMacTest() = default;
  ~DesktopCaptureMacTest() override = default;

  content::BrowserTaskEnvironment task_environment_;
};

// TODO(crbug.com/547843376): Re-enable this test once the bug is fixed.
#if BUILDFLAG(IS_MAC)
#define MAYBE_CancelMacNativePickerBeforeCapture \
  DISABLED_CancelMacNativePickerBeforeCapture
#else
#define MAYBE_CancelMacNativePickerBeforeCapture \
  CancelMacNativePickerBeforeCapture
#endif
TEST_F(DesktopCaptureMacTest, MAYBE_CancelMacNativePickerBeforeCapture) {
  bool callback_run = false;
  DesktopMediaID mac_source(DesktopMediaID::TYPE_SCREEN, 123);
  mac_source.id_type = DesktopMediaID::IdType::kNativePickerSession;

  auto request = CaptureScreenshot(
      mac_source,
      base::BindOnce([](bool* callback_run,
                        const SkBitmap& bitmap) { *callback_run = true; },
                     &callback_run));

  // Destroy the request handle immediately to cancel the capture.
  request.reset();

  // The Mac backend posts a task to the UI thread, which then posts back
  // to the current thread. To ensure we wait for that full chain to execute
  // and be discarded, we queue a QuitClosure through the exact same sequence.
  base::RunLoop run_loop;
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindPostTask(base::SingleThreadTaskRunner::GetCurrentDefault(),
                         run_loop.QuitClosure()));
  run_loop.Run();

  // The callback should NOT have run because the request was cancelled.
  EXPECT_FALSE(callback_run);
}
#endif  // BUILDFLAG(IS_MAC)

}  // namespace content::desktop_capture
