// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/software_video_renderer.h"

#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "remoting/client/client_context.h"
#include "remoting/codec/video_encoder_verbatim.h"
#include "remoting/proto/video.pb.h"
#include "remoting/protocol/frame_consumer.h"
#include "remoting/protocol/session_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

using webrtc::DesktopFrame;

namespace remoting {

namespace {

const int kFrameWidth = 200;
const int kFrameHeight = 200;

class TestFrameConsumer : public protocol::FrameConsumer {
 public:
  TestFrameConsumer() = default;
  ~TestFrameConsumer() override = default;

  std::unique_ptr<DesktopFrame> WaitForNextFrame(
      base::Closure* out_done_callback) {
    EXPECT_TRUE(thread_checker_.CalledOnValidThread());
    frame_run_loop_.reset(new base::RunLoop());
    frame_run_loop_->Run();
    frame_run_loop_.reset();
    *out_done_callback = last_frame_done_callback_;
    last_frame_done_callback_.Reset();
    return std::move(last_frame_);
  }

  // FrameConsumer interface.
  std::unique_ptr<DesktopFrame> AllocateFrame(
      const webrtc::DesktopSize& size) override {
    EXPECT_TRUE(thread_checker_.CalledOnValidThread());
    return std::make_unique<webrtc::BasicDesktopFrame>(size);
  }

  void DrawFrame(std::unique_ptr<DesktopFrame> frame,
                 const base::Closure& done) override {
    EXPECT_TRUE(thread_checker_.CalledOnValidThread());
    last_frame_ = std::move(frame);
    last_frame_done_callback_ = done;
    frame_run_loop_->Quit();
  }

  PixelFormat GetPixelFormat() override {
    EXPECT_TRUE(thread_checker_.CalledOnValidThread());
    return FORMAT_BGRA;
  }

 private:
  base::ThreadChecker thread_checker_;

  std::unique_ptr<base::RunLoop> frame_run_loop_;

  std::unique_ptr<DesktopFrame> last_frame_;
  base::Closure last_frame_done_callback_;
};

std::unique_ptr<DesktopFrame> CreateTestFrame(int index) {
  std::unique_ptr<DesktopFrame> frame(new webrtc::BasicDesktopFrame(
      webrtc::DesktopSize(kFrameWidth, kFrameHeight)));

  for (int y = 0; y < kFrameHeight; y++) {
    for (int x = 0; x < kFrameWidth; x++) {
      uint8_t* out = frame->data() + x * DesktopFrame::kBytesPerPixel +
                     y * frame->stride();
        out[0] = index + x + y * kFrameWidth;
        out[1] = index + x + y * kFrameWidth + 1;
        out[2] = index + x + y * kFrameWidth + 2;
        out[3] = 0;
    }
  }

  if (index == 0) {
    frame->mutable_updated_region()->SetRect(
        webrtc::DesktopRect::MakeWH(kFrameWidth, kFrameHeight));
  } else {
    frame->mutable_updated_region()->SetRect(
        webrtc::DesktopRect::MakeWH(index, index));
  }

  return frame;
}

// Returns true when frames a and b are equivalent.
bool CompareFrames(const DesktopFrame& a, const DesktopFrame& b) {
  if (!a.size().equals(b.size()) ||
      !a.updated_region().Equals(b.updated_region())) {
    return false;
  }

  for (webrtc::DesktopRegion::Iterator i(a.updated_region()); !i.IsAtEnd();
       i.Advance()) {
    for (int row = i.rect().top(); row < i.rect().bottom(); ++row) {
      if (memcmp(a.data() + a.stride() * row +
                     i.rect().left() * DesktopFrame::kBytesPerPixel,
                 b.data() + b.stride() * row +
                     i.rect().left() * DesktopFrame::kBytesPerPixel,
                 i.rect().width() * DesktopFrame::kBytesPerPixel) != 0) {
        return false;
      }
    }
  }

  return true;
}

// Helper to set value at |out| to 1.
void SetTrue(int* out) {
  *out = 1;
}

}  // namespace

class SoftwareVideoRendererTest : public ::testing::Test {
 public:
  SoftwareVideoRendererTest() : context_(nullptr) {
    context_.Start();
    renderer_.reset(new SoftwareVideoRenderer(&frame_consumer_));
    renderer_->Initialize(context_, nullptr);
    renderer_->OnSessionConfig(
        *protocol::SessionConfig::ForTestWithVerbatimVideo());
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  ClientContext context_;

  TestFrameConsumer frame_consumer_;
  std::unique_ptr<SoftwareVideoRenderer> renderer_;

  VideoEncoderVerbatim encoder_;
};

TEST_F(SoftwareVideoRendererTest, DecodeFrame) {
  const int kFrameCount = 5;

  std::vector<std::unique_ptr<DesktopFrame>> test_frames;

  // std::vector<bool> doesn't allow to get pointer to individual values, so
  // int needs to be used instead.
  std::vector<int> callback_called(kFrameCount);

  for (int frame_index = 0; frame_index < kFrameCount; frame_index++) {
    test_frames.push_back(CreateTestFrame(frame_index));
    callback_called[frame_index] = 0;

    renderer_->ProcessVideoPacket(
        encoder_.Encode(*test_frames[frame_index]),
        base::Bind(&SetTrue, &(callback_called[frame_index])));
  }

  for (int frame_index = 0; frame_index < kFrameCount; frame_index++) {
    base::Closure done_callback;
    std::unique_ptr<DesktopFrame> decoded_frame =
        frame_consumer_.WaitForNextFrame(&done_callback);

    EXPECT_FALSE(callback_called[frame_index]);
    done_callback.Run();
    EXPECT_TRUE(callback_called[frame_index]);

    EXPECT_TRUE(CompareFrames(*test_frames[frame_index], *decoded_frame));
  }
}

}  // namespace remoting
