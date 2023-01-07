// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromeos/aura_desktop_capturer.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "remoting/host/chromeos/ash_proxy.h"
#include "remoting/host/chromeos/scoped_fake_ash_proxy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "ui/display/display.h"

using testing::_;
using testing::Eq;
using testing::IsNull;
using testing::NotNull;

namespace remoting {

namespace {

struct CaptureResult {
  webrtc::DesktopCapturer::Result result;
  std::unique_ptr<webrtc::DesktopFrame> frame;
};

// Test frame data.
const unsigned char expected_frame[] = {
    0x00, 0x00, 0x00, 0x9a, 0x65, 0x1e, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
    0x00, 0x90, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x60, 0x90,
    0x24, 0x71, 0xf8, 0xf2, 0xe5, 0xdf, 0x7f, 0x81, 0xc7, 0x49, 0xc4, 0xa3,
    0x58, 0x5c, 0xf6, 0xcc, 0x40, 0x14, 0x28, 0x0c, 0xa0, 0xfa, 0x03, 0x18,
    0x38, 0xd8, 0x7d, 0x77, 0x2b, 0x3a, 0x00, 0x00, 0x00, 0x20, 0x64, 0x46,
    0x47, 0x2f, 0xdf, 0x6e, 0xed, 0x7b, 0xf3, 0xc3, 0x37, 0x20, 0xf2, 0x36,
    0x67, 0x6c, 0x36, 0xe1, 0xb4, 0x5e, 0xbe, 0x04, 0x85, 0xdb, 0x89, 0xa3,
    0xcd, 0xfd, 0xd2, 0x4b, 0xd6, 0x9f, 0x00, 0x00, 0x00, 0x40, 0x38, 0x35,
    0x05, 0x75, 0x1d, 0x13, 0x6e, 0xb3, 0x6b, 0x1d, 0x29, 0xae, 0xd3, 0x43,
    0xe6, 0x84, 0x8f, 0xa3, 0x9d, 0x65, 0x4e, 0x2f, 0x57, 0xe3, 0xf6, 0xe6,
    0x20, 0x3c, 0x00, 0xc6, 0xe1, 0x73, 0x34, 0xe2, 0x23, 0x99, 0xc4, 0xfa,
    0x91, 0xc2, 0xd5, 0x97, 0xc1, 0x8b, 0xd0, 0x3c, 0x13, 0xba, 0xf0, 0xd7};
const char kUmaKeyForCapturerCreated[] =
    "Enterprise.DeviceRemoteCommand.Crd.Capturer.Aura.Created";
const char kUmaKeyForCapturerDestroyed[] =
    "Enterprise.DeviceRemoteCommand.Crd.Capturer.Aura.Destroyed";

SkBitmap TestBitmap() {
  SkBitmap bitmap;
  const SkImageInfo& info =
      SkImageInfo::Make(3, 4, kBGRA_8888_SkColorType, kPremul_SkAlphaType,
                        SkColorSpace::MakeSRGB());
  bitmap.installPixels(info, const_cast<unsigned char*>(expected_frame), 12);
  return bitmap;
}

SkBitmap CreateBitmap(int width, int height) {
  SkBitmap bitmap;
  const SkImageInfo& info =
      SkImageInfo::Make(width, height, kBGRA_8888_SkColorType,
                        kPremul_SkAlphaType, SkColorSpace::MakeSRGB());
  bitmap.setInfo(info);
  return bitmap;
}

ACTION_P(SaveUniquePtrArg, dest) {
  *dest = std::move(*arg1);
}

class DesktopCapturerCallback : public webrtc::DesktopCapturer::Callback {
 public:
  DesktopCapturerCallback() = default;
  DesktopCapturerCallback(const DesktopCapturerCallback&) = delete;
  DesktopCapturerCallback& operator=(const DesktopCapturerCallback&) = delete;
  ~DesktopCapturerCallback() override = default;

  CaptureResult WaitForResult() {
    EXPECT_TRUE(result_.Wait());
    return result_.Take();
  }

  // webrtc::DesktopCapturer::Callback implementation:
  void OnCaptureResult(webrtc::DesktopCapturer::Result result,
                       std::unique_ptr<webrtc::DesktopFrame> frame) override {
    result_.SetValue(CaptureResult{result, std::move(frame)});
  }

 private:
  base::test::TestFuture<CaptureResult> result_;
};

}  // namespace

class AuraDesktopCapturerTest : public testing::Test {
 public:
  AuraDesktopCapturerTest() = default;

  test::ScopedFakeAshProxy& ash_proxy() { return ash_proxy_; }

  DesktopCapturerCallback& desktop_capturer_callback() { return callback_; }

 protected:
  base::test::SingleThreadTaskEnvironment environment_;
  test::ScopedFakeAshProxy ash_proxy_;
  DesktopCapturerCallback callback_;
  AuraDesktopCapturer capturer_{ash_proxy_};
};

TEST_F(AuraDesktopCapturerTest, ShouldSendScreenshotRequestForPrimaryDisplay) {
  ash_proxy().AddPrimaryDisplay(111);

  capturer_.Start(&desktop_capturer_callback());
  capturer_.CaptureFrame();

  test::ScreenshotRequest request = ash_proxy().WaitForScreenshotRequest();

  EXPECT_THAT(request.display, Eq(111));
}

TEST_F(AuraDesktopCapturerTest, ShouldSendScreenshotToCapturer) {
  ash_proxy().AddPrimaryDisplay();

  capturer_.Start(&desktop_capturer_callback());
  capturer_.CaptureFrame();

  const SkBitmap expected_bitmap = TestBitmap();
  ash_proxy().ReplyWithScreenshot(expected_bitmap);

  CaptureResult result = desktop_capturer_callback().WaitForResult();
  EXPECT_THAT(result.result, Eq(webrtc::DesktopCapturer::Result::SUCCESS));
  ASSERT_THAT(result.frame, NotNull());
  EXPECT_EQ(0, memcmp(expected_bitmap.getPixels(), result.frame->data(),
                      expected_bitmap.computeByteSize()));
}

TEST_F(AuraDesktopCapturerTest, ShouldSetUpdatedRegion) {
  ash_proxy().AddPrimaryDisplay();

  capturer_.Start(&desktop_capturer_callback());
  capturer_.CaptureFrame();

  ash_proxy().ReplyWithScreenshot(TestBitmap());

  CaptureResult result = desktop_capturer_callback().WaitForResult();
  ASSERT_THAT(result.frame, NotNull());
  EXPECT_FALSE(result.frame->updated_region().is_empty());
}

TEST_F(AuraDesktopCapturerTest, ShouldSetDpi) {
  const float scale_factor = 2.5;
  // scale_factor = dpi / default_dpi (and default_dpi is 96).
  const int dpi = static_cast<int>(scale_factor * 96);

  ash_proxy().AddPrimaryDisplay().set_device_scale_factor(scale_factor);

  capturer_.Start(&desktop_capturer_callback());
  capturer_.CaptureFrame();

  ash_proxy().ReplyWithScreenshot(TestBitmap());

  CaptureResult result = desktop_capturer_callback().WaitForResult();
  ASSERT_THAT(result.frame, NotNull());
  EXPECT_THAT(result.frame->dpi().x(), Eq(dpi));
  EXPECT_THAT(result.frame->dpi().y(), Eq(dpi));
}

TEST_F(AuraDesktopCapturerTest, ShouldSetDisplayBounds) {
  const int left = 100;
  const int top = 200;
  const int width = 300;
  const int height = 400;

  ash_proxy().AddPrimaryDisplay().set_bounds(
      gfx::Rect(left, top, width, height));

  capturer_.Start(&desktop_capturer_callback());
  capturer_.CaptureFrame();

  ash_proxy().ReplyWithScreenshot(CreateBitmap(width, height));

  CaptureResult result = desktop_capturer_callback().WaitForResult();
  ASSERT_THAT(result.frame, NotNull());
  EXPECT_THAT(result.frame->rect().left(), Eq(left));
  EXPECT_THAT(result.frame->rect().top(), Eq(top));
  EXPECT_THAT(result.frame->rect().width(), Eq(width));
  EXPECT_THAT(result.frame->rect().height(), Eq(height));
}

TEST_F(AuraDesktopCapturerTest, ShouldNotCrashIfDisplayIsUnavailable) {
  capturer_.Start(&desktop_capturer_callback());

  ash_proxy().AddDisplayWithId(111);

  capturer_.SelectSource(111);

  ash_proxy().RemoveDisplay(111);

  capturer_.CaptureFrame();

  CaptureResult result = desktop_capturer_callback().WaitForResult();
  ASSERT_THAT(result.result,
              Eq(webrtc::DesktopCapturer::Result::ERROR_TEMPORARY));
  ASSERT_THAT(result.frame, IsNull());
}

TEST_F(AuraDesktopCapturerTest, ShouldReturnTemporaryErrorIfScreenshotFails) {
  ash_proxy().AddPrimaryDisplay();

  capturer_.Start(&desktop_capturer_callback());
  capturer_.CaptureFrame();

  ash_proxy().ReplyWithScreenshot(absl::nullopt);

  CaptureResult result = desktop_capturer_callback().WaitForResult();
  EXPECT_THAT(result.result,
              Eq(webrtc::DesktopCapturer::Result::ERROR_TEMPORARY));
  EXPECT_THAT(result.frame, IsNull());
}

TEST_F(AuraDesktopCapturerTest, ShouldAllowSwitchingToSecondaryMonitor) {
  // We're using a value bigger than 32 bit to ensure nothing gets truncated.
  constexpr int64_t display_id = 123456789123456789;

  ash_proxy().AddPrimaryDisplay();
  ash_proxy().AddDisplayWithId(display_id);

  capturer_.Start(&desktop_capturer_callback());
  capturer_.SelectSource(display_id);

  capturer_.CaptureFrame();

  test::ScreenshotRequest request = ash_proxy().WaitForScreenshotRequest();
  EXPECT_THAT(request.display, Eq(display_id));
}

TEST_F(AuraDesktopCapturerTest, ShouldFailSwitchingToNonExistingMonitor) {
  capturer_.Start(&desktop_capturer_callback());
  EXPECT_FALSE(capturer_.SelectSource(222));
}

TEST_F(AuraDesktopCapturerTest, ShouldUseCorrectDisplayAfterSwitching) {
  ash_proxy().AddPrimaryDisplay();
  ash_proxy().AddDisplayWithId(222);
  ash_proxy().AddDisplayWithId(333);

  capturer_.Start(&desktop_capturer_callback());
  capturer_.SelectSource(333);

  capturer_.CaptureFrame();

  test::ScreenshotRequest request = ash_proxy().WaitForScreenshotRequest();
  EXPECT_THAT(request.display, Eq(333));
}

TEST_F(AuraDesktopCapturerTest,
       ShouldSendUmaLogsOnCapturerConstructionAndDestruction) {
  base::HistogramTester histogram_tester;

  auto my_capturer = std::make_unique<AuraDesktopCapturer>();
  histogram_tester.ExpectUniqueSample(kUmaKeyForCapturerCreated, true, 1);

  my_capturer = nullptr;
  histogram_tester.ExpectUniqueSample(kUmaKeyForCapturerDestroyed, true, 1);
}

}  // namespace remoting
