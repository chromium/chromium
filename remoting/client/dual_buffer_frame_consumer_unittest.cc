// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "remoting/client/dual_buffer_frame_consumer.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/shared_desktop_frame.h"

namespace remoting {

namespace {

webrtc::DesktopFrame* GetUnderlyingFrame(
    const std::unique_ptr<webrtc::DesktopFrame>& frame) {
  return reinterpret_cast<webrtc::SharedDesktopFrame*>(frame.get())->
      GetUnderlyingFrame();
}

void FillRGBARect(uint8_t r,
                  uint8_t g,
                  uint8_t b,
                  uint8_t a,
                  const webrtc::DesktopRect& rect,
                  webrtc::DesktopFrame* frame) {
  for (int x = 0; x < rect.width(); x++) {
    for (int y = 0; y < rect.height(); y++) {
      uint8_t* data = frame->GetFrameDataAtPos(
          rect.top_left().add(webrtc::DesktopVector(x, y)));
      data[0] = r;
      data[1] = g;
      data[2] = b;
      data[3] = a;
    }
  }
  frame->mutable_updated_region()->SetRect(rect);
}

void CheckFrameColor(uint8_t r,
                     uint8_t g,
                     uint8_t b,
                     uint8_t a,
                     const webrtc::DesktopVector& pos,
                     const webrtc::DesktopFrame& frame) {
  uint8_t* data = frame.GetFrameDataAtPos(pos);
  EXPECT_EQ(r, data[0]);
  EXPECT_EQ(g, data[1]);
  EXPECT_EQ(b, data[2]);
  EXPECT_EQ(a, data[3]);
}

}  // namespace

class DualBufferFrameConsumerTest : public testing::Test {
 public:
  void SetUp() override;
 protected:
  std::unique_ptr<DualBufferFrameConsumer> consumer_;
  std::unique_ptr<webrtc::DesktopFrame> received_frame_;
  base::OnceClosure done_closure_;

 private:
  void OnFrameReceived(std::unique_ptr<webrtc::DesktopFrame> frame,
                       base::OnceClosure done);
};

void DualBufferFrameConsumerTest::SetUp() {
  consumer_ = std::make_unique<DualBufferFrameConsumer>(
      base::BindRepeating(&DualBufferFrameConsumerTest::OnFrameReceived,
                          base::Unretained(this)),
      nullptr, protocol::FrameConsumer::FORMAT_RGBA);
}

void DualBufferFrameConsumerTest::OnFrameReceived(
    std::unique_ptr<webrtc::DesktopFrame> frame,
    base::OnceClosure done) {
  received_frame_ = std::move(frame);
  done_closure_ = std::move(done);
}

TEST_F(DualBufferFrameConsumerTest, AllocateOneFrame) {
  std::unique_ptr<webrtc::DesktopFrame> frame =
      consumer_->AllocateFrame(webrtc::DesktopSize(16, 16));
  ASSERT_TRUE(frame->size().equals(webrtc::DesktopSize(16, 16)));
  webrtc::DesktopFrame* raw_frame = frame.get();
  consumer_->DrawFrame(std::move(frame), base::NullCallback());
  EXPECT_EQ(raw_frame, received_frame_.get());
}

TEST_F(DualBufferFrameConsumerTest, BufferRotation) {
  webrtc::DesktopSize size16x16(16, 16);

  std::unique_ptr<webrtc::DesktopFrame> frame =
      consumer_->AllocateFrame(size16x16);
  webrtc::DesktopFrame* underlying_frame_1 = GetUnderlyingFrame(frame);
  consumer_->DrawFrame(std::move(frame), base::NullCallback());

  frame = consumer_->AllocateFrame(size16x16);
  webrtc::DesktopFrame* underlying_frame_2 = GetUnderlyingFrame(frame);
  EXPECT_NE(underlying_frame_1, underlying_frame_2);
  consumer_->DrawFrame(std::move(frame), base::NullCallback());

  frame = consumer_->AllocateFrame(size16x16);
  webrtc::DesktopFrame* underlying_frame_3 = GetUnderlyingFrame(frame);
  EXPECT_EQ(underlying_frame_1, underlying_frame_3);
  consumer_->DrawFrame(std::move(frame), base::NullCallback());

  frame = consumer_->AllocateFrame(size16x16);
  webrtc::DesktopFrame* underlying_frame_4 = GetUnderlyingFrame(frame);
  EXPECT_EQ(underlying_frame_2, underlying_frame_4);
  consumer_->DrawFrame(std::move(frame), base::NullCallback());
}

TEST_F(DualBufferFrameConsumerTest, DrawAndMergeFrames) {
  webrtc::DesktopSize size2x2(2, 2);

  // X means uninitialized color.

  // Frame 1:
  // RR
  // RR
  std::unique_ptr<webrtc::DesktopFrame> frame =
      consumer_->AllocateFrame(size2x2);
  FillRGBARect(0xff, 0, 0, 0xff, webrtc::DesktopRect::MakeXYWH(0, 0, 2, 2),
               frame.get());
  consumer_->DrawFrame(std::move(frame), base::NullCallback());

  // Frame 2:
  // GG
  // XX
  frame = consumer_->AllocateFrame(size2x2);
  FillRGBARect(0, 0xff, 0, 0xff, webrtc::DesktopRect::MakeXYWH(0, 0, 2, 1),
               frame.get());
  consumer_->DrawFrame(std::move(frame), base::NullCallback());

  // Merged Frame:
  // GG
  // RR
  consumer_->RequestFullDesktopFrame();
  ASSERT_TRUE(received_frame_->size().equals(size2x2));

  CheckFrameColor(0, 0xff, 0, 0xff, webrtc::DesktopVector(0, 0),
                  *received_frame_);
  CheckFrameColor(0xff, 0, 0, 0xff, webrtc::DesktopVector(0, 1),
                  *received_frame_);
  CheckFrameColor(0, 0xff, 0, 0xff, webrtc::DesktopVector(1, 0),
                  *received_frame_);
  CheckFrameColor(0xff, 0, 0, 0xff, webrtc::DesktopVector(1, 1),
                  *received_frame_);

  // Frame 3:
  // BX
  // BX
  frame = consumer_->AllocateFrame(size2x2);
  FillRGBARect(0, 0, 0xff, 0xff, webrtc::DesktopRect::MakeXYWH(0, 0, 1, 2),
               frame.get());
  consumer_->DrawFrame(std::move(frame), base::NullCallback());

  // Merged Frame:
  // BG
  // BR
  consumer_->RequestFullDesktopFrame();
  ASSERT_TRUE(received_frame_->size().equals(size2x2));

  CheckFrameColor(0, 0, 0xff, 0xff, webrtc::DesktopVector(0, 0),
                  *received_frame_);
  CheckFrameColor(0, 0, 0xff, 0xff, webrtc::DesktopVector(0, 1),
                  *received_frame_);
  CheckFrameColor(0, 0xff, 0, 0xff, webrtc::DesktopVector(1, 0),
                  *received_frame_);
  CheckFrameColor(0xff, 0, 0, 0xff, webrtc::DesktopVector(1, 1),
                  *received_frame_);
}

TEST_F(DualBufferFrameConsumerTest, ChangeScreenSizeAndReallocateBuffers) {
  webrtc::DesktopSize size16x16(16, 16);

  std::unique_ptr<webrtc::DesktopFrame> frame =
      consumer_->AllocateFrame(size16x16);
  webrtc::DesktopFrame* underlying_frame_1 = GetUnderlyingFrame(frame);
  consumer_->DrawFrame(std::move(frame), base::NullCallback());

  frame = consumer_->AllocateFrame(size16x16);
  webrtc::DesktopFrame* underlying_frame_2 = GetUnderlyingFrame(frame);
  EXPECT_NE(underlying_frame_1, underlying_frame_2);
  consumer_->DrawFrame(std::move(frame), base::NullCallback());

  webrtc::DesktopSize size32x32(32, 32);

  frame = consumer_->AllocateFrame(size32x32);
  webrtc::DesktopFrame* underlying_frame_3 = GetUnderlyingFrame(frame);
  EXPECT_NE(underlying_frame_1, underlying_frame_3);
  consumer_->DrawFrame(std::move(frame), base::NullCallback());

  frame = consumer_->AllocateFrame(size32x32);
  webrtc::DesktopFrame* underlying_frame_4 = GetUnderlyingFrame(frame);
  EXPECT_NE(underlying_frame_2, underlying_frame_4);
  consumer_->DrawFrame(std::move(frame), base::NullCallback());
}

}  // namespace remoting
