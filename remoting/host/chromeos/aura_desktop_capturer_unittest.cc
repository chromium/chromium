// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromeos/aura_desktop_capturer.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

using testing::_;

namespace remoting {

namespace {

// Test frame data.
const unsigned char frame_data[] = {
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
    0x91, 0xc2, 0xd5, 0x97, 0xc1, 0x8b, 0xd0, 0x3c, 0x13, 0xba, 0xf0, 0xd7
  };

ACTION_P(SaveUniquePtrArg, dest) {
  *dest = std::move(*arg1);
}

}  // namespace

class AuraDesktopCapturerTest : public testing::Test,
                                public webrtc::DesktopCapturer::Callback {
 public:
  AuraDesktopCapturerTest() = default;

  void SetUp() override;

  MOCK_METHOD2(OnCaptureResultPtr,
               void(webrtc::DesktopCapturer::Result result,
                    std::unique_ptr<webrtc::DesktopFrame>* frame));
  void OnCaptureResult(webrtc::DesktopCapturer::Result result,
                       std::unique_ptr<webrtc::DesktopFrame> frame) override {
    OnCaptureResultPtr(result, &frame);
  }

 protected:
  void SimulateFrameCapture() {
    SkBitmap bitmap;
    const SkImageInfo& info =
        SkImageInfo::Make(3, 4, kBGRA_8888_SkColorType, kPremul_SkAlphaType,
                          SkColorSpace::MakeSRGB());
    bitmap.installPixels(info, const_cast<unsigned char*>(frame_data), 12);

    capturer_->OnFrameCaptured(std::make_unique<viz::CopyOutputSkBitmapResult>(
        gfx::Rect(0, 0, bitmap.width(), bitmap.height()), std::move(bitmap)));
  }

  std::unique_ptr<AuraDesktopCapturer> capturer_;
};

void AuraDesktopCapturerTest::SetUp() {
  capturer_ = std::make_unique<AuraDesktopCapturer>();
}

TEST_F(AuraDesktopCapturerTest, ConvertSkBitmapToDesktopFrame) {
  std::unique_ptr<webrtc::DesktopFrame> captured_frame;

  EXPECT_CALL(*this,
              OnCaptureResultPtr(webrtc::DesktopCapturer::Result::SUCCESS, _))
      .Times(1)
      .WillOnce(SaveUniquePtrArg(&captured_frame));
  capturer_->Start(this);

  SimulateFrameCapture();

  ASSERT_TRUE(captured_frame);
  EXPECT_EQ(0, memcmp(frame_data, captured_frame->data(), sizeof(frame_data)));
}

}  // namespace remoting
