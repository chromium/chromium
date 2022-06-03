// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/codec/webrtc_video_encoder_selector.h"

#include <string>

#include "base/bind.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

namespace remoting {

namespace {

class FakeWebrtcVideoEncoder : public WebrtcVideoEncoder {
 public:
  explicit FakeWebrtcVideoEncoder(int index) : index_(index) {}
  ~FakeWebrtcVideoEncoder() override = default;

  int index() const { return index_; }

  void Encode(std::unique_ptr<webrtc::DesktopFrame> frame,
              const FrameParams& param,
              EncodeCallback done) override {}

 private:
  const int index_;
};

}  // namespace

class WebrtcVideoEncoderSelectorTest : public ::testing::Test {
 public:
  WebrtcVideoEncoderSelectorTest() {
    // Register three encoders.
    for (int i = 0; i < kEncoderCount; i++) {
      int j = selector_.RegisterEncoder(
          base::BindRepeating(
              &WebrtcVideoEncoderSelectorTest::IsProfileSupported,
              base::Unretained(this), i),
          base::BindRepeating(&WebrtcVideoEncoderSelectorTest::CreateEncoder,
                              base::Unretained(this), i));
      EXPECT_EQ(i, j);
    }
    // Set the default DesktopFrame.
    SetDesktopFrame();
  }

 protected:
  void SetDesktopFrame() {
    selector_.SetDesktopFrame(webrtc::BasicDesktopFrame(
        webrtc::DesktopSize(frame_width_, frame_height_)));
  }

  static constexpr int kEncoderCount = 3;

  WebrtcVideoEncoderSelector selector_;

  // IsProfileSupported() increments |is_supported_called_times_| each time it's
  // called.
  int is_supported_called_times_ = 0;

  bool is_supported_[kEncoderCount] = { false };

  // Some arbitrary integers to test
  // WebrtcVideoEncoderSelector::SetDesktopFrame() function.
  int frame_width_ = 1001;
  int frame_height_ = 999;

 private:
  bool IsProfileSupported(int index,
                          const WebrtcVideoEncoderSelector::Profile& profile) {
    EXPECT_EQ(profile.resolution.width(), frame_width_);
    EXPECT_EQ(profile.resolution.height(), frame_height_);
    is_supported_called_times_++;
    return is_supported_[index];
  }

  // Returns a FakeWebrtcVideoEncoder with |index| as its index.
  std::unique_ptr<WebrtcVideoEncoder> CreateEncoder(int index) {
    return std::make_unique<FakeWebrtcVideoEncoder>(index);
  }
};

TEST_F(WebrtcVideoEncoderSelectorTest, ReturnsNullIfCodecIsNotSupported) {
  ASSERT_EQ(nullptr, selector_.CreateEncoder());
  ASSERT_EQ(3, is_supported_called_times_);
}

TEST_F(WebrtcVideoEncoderSelectorTest, ReturnsPreferredCodecFirst) {
  memset(is_supported_, -1, sizeof(is_supported_));
  selector_.SetPreferredCodec(1);
  std::unique_ptr<WebrtcVideoEncoder> encoder = selector_.CreateEncoder();
  ASSERT_NE(nullptr, encoder);
  ASSERT_EQ(1, static_cast<FakeWebrtcVideoEncoder*>(encoder.get())->index());
  ASSERT_EQ(1, is_supported_called_times_);

  encoder = selector_.CreateEncoder();
  ASSERT_NE(nullptr, encoder);
  ASSERT_EQ(0, static_cast<FakeWebrtcVideoEncoder*>(encoder.get())->index());
  ASSERT_EQ(2, is_supported_called_times_);

  encoder = selector_.CreateEncoder();
  ASSERT_NE(nullptr, encoder);
  ASSERT_EQ(2, static_cast<FakeWebrtcVideoEncoder*>(encoder.get())->index());
  ASSERT_EQ(3, is_supported_called_times_);

  encoder = selector_.CreateEncoder();
  ASSERT_EQ(nullptr, encoder);
  ASSERT_EQ(3, is_supported_called_times_);
}

TEST_F(WebrtcVideoEncoderSelectorTest, PreferredCodecIsNotQualified) {
  is_supported_[1] = true;
  is_supported_[2] = true;
  selector_.SetPreferredCodec(0);
  std::unique_ptr<WebrtcVideoEncoder> encoder = selector_.CreateEncoder();
  ASSERT_NE(nullptr, encoder);
  ASSERT_EQ(1, static_cast<FakeWebrtcVideoEncoder*>(encoder.get())->index());
  ASSERT_EQ(2, is_supported_called_times_);

  encoder = selector_.CreateEncoder();
  ASSERT_NE(nullptr, encoder);
  ASSERT_EQ(2, static_cast<FakeWebrtcVideoEncoder*>(encoder.get())->index());
  ASSERT_EQ(3, is_supported_called_times_);

  encoder = selector_.CreateEncoder();
  ASSERT_EQ(nullptr, encoder);
  ASSERT_EQ(3, is_supported_called_times_);
}

TEST_F(WebrtcVideoEncoderSelectorTest, OtherCodecsAreNotQualified) {
  is_supported_[2] = true;
  selector_.SetPreferredCodec(2);
  std::unique_ptr<WebrtcVideoEncoder> encoder = selector_.CreateEncoder();
  ASSERT_NE(nullptr, encoder);
  ASSERT_EQ(2, static_cast<FakeWebrtcVideoEncoder*>(encoder.get())->index());
  ASSERT_EQ(1, is_supported_called_times_);

  encoder = selector_.CreateEncoder();
  ASSERT_EQ(nullptr, encoder);
  ASSERT_EQ(3, is_supported_called_times_);
}

TEST_F(WebrtcVideoEncoderSelectorTest, SetNewDesktopFrame) {
  memset(is_supported_, -1, sizeof(is_supported_));
  selector_.SetPreferredCodec(1);
  std::unique_ptr<WebrtcVideoEncoder> encoder = selector_.CreateEncoder();
  ASSERT_NE(nullptr, encoder);
  ASSERT_EQ(1, static_cast<FakeWebrtcVideoEncoder*>(encoder.get())->index());
  ASSERT_EQ(1, is_supported_called_times_);

  encoder = selector_.CreateEncoder();
  ASSERT_NE(nullptr, encoder);
  ASSERT_EQ(0, static_cast<FakeWebrtcVideoEncoder*>(encoder.get())->index());
  ASSERT_EQ(2, is_supported_called_times_);

  encoder = selector_.CreateEncoder();
  ASSERT_NE(nullptr, encoder);
  ASSERT_EQ(2, static_cast<FakeWebrtcVideoEncoder*>(encoder.get())->index());
  ASSERT_EQ(3, is_supported_called_times_);

  encoder = selector_.CreateEncoder();
  ASSERT_EQ(nullptr, encoder);
  ASSERT_EQ(3, is_supported_called_times_);

  // Setting an unchanged DesktopFrame should not reset the internal state of
  // WebrtcVideoEncoderSelector.
  SetDesktopFrame();
  encoder = selector_.CreateEncoder();
  ASSERT_EQ(nullptr, encoder);
  ASSERT_EQ(3, is_supported_called_times_);

  // Setting a new DesktopFrame should reset the internal state of
  // WebrtcVideoEncoderSelector: it should start from the preferred codec.
  frame_width_++;
  SetDesktopFrame();

  selector_.SetPreferredCodec(2);
  encoder = selector_.CreateEncoder();
  ASSERT_NE(nullptr, encoder);
  ASSERT_EQ(2, static_cast<FakeWebrtcVideoEncoder*>(encoder.get())->index());
  ASSERT_EQ(4, is_supported_called_times_);

  encoder = selector_.CreateEncoder();
  ASSERT_NE(nullptr, encoder);
  ASSERT_EQ(0, static_cast<FakeWebrtcVideoEncoder*>(encoder.get())->index());
  ASSERT_EQ(5, is_supported_called_times_);

  encoder = selector_.CreateEncoder();
  ASSERT_NE(nullptr, encoder);
  ASSERT_EQ(1, static_cast<FakeWebrtcVideoEncoder*>(encoder.get())->index());
  ASSERT_EQ(6, is_supported_called_times_);

  encoder = selector_.CreateEncoder();
  ASSERT_EQ(nullptr, encoder);
  ASSERT_EQ(6, is_supported_called_times_);
}

}  // namespace remoting
