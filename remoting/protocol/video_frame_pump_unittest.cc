// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/video_frame_pump.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "remoting/base/auto_thread.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/codec/video_encoder.h"
#include "remoting/codec/video_encoder_verbatim.h"
#include "remoting/proto/control.pb.h"
#include "remoting/proto/video.pb.h"
#include "remoting/protocol/desktop_capturer.h"
#include "remoting/protocol/fake_desktop_capturer.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::DoAll;
using ::testing::Expectation;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;

namespace remoting::protocol {

namespace {

ACTION(FinishSend) {
  std::move(*arg1).Run();
}

std::unique_ptr<webrtc::DesktopFrame> CreateNullFrame(
    webrtc::SharedMemoryFactory* shared_memory_factory) {
  return nullptr;
}

std::unique_ptr<webrtc::DesktopFrame> CreateUnchangedFrame(
    webrtc::SharedMemoryFactory* shared_memory_factory) {
  const webrtc::DesktopSize kSize(800, 640);
  // updated_region() is already empty by default in new BasicDesktopFrames.
  return std::make_unique<webrtc::BasicDesktopFrame>(kSize);
}

class MockVideoEncoder : public VideoEncoder {
 public:
  MockVideoEncoder() = default;
  ~MockVideoEncoder() override = default;

  MOCK_METHOD1(EncodePtr, VideoPacket*(const webrtc::DesktopFrame&));

  std::unique_ptr<VideoPacket> Encode(
      const webrtc::DesktopFrame& frame) override {
    return base::WrapUnique(EncodePtr(frame));
  }
};

}  // namespace

static const int kWidth = 640;
static const int kHeight = 480;

class ThreadCheckVideoEncoder : public VideoEncoderVerbatim {
 public:
  ThreadCheckVideoEncoder(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : task_runner_(task_runner) {}

  ThreadCheckVideoEncoder(const ThreadCheckVideoEncoder&) = delete;
  ThreadCheckVideoEncoder& operator=(const ThreadCheckVideoEncoder&) = delete;

  ~ThreadCheckVideoEncoder() override {
    EXPECT_TRUE(task_runner_->BelongsToCurrentThread());
  }

  std::unique_ptr<VideoPacket> Encode(
      const webrtc::DesktopFrame& frame) override {
    return std::make_unique<VideoPacket>();
  }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

class ThreadCheckDesktopCapturer : public DesktopCapturer {
 public:
  ThreadCheckDesktopCapturer(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : task_runner_(task_runner), callback_(nullptr) {}

  ThreadCheckDesktopCapturer(const ThreadCheckDesktopCapturer&) = delete;
  ThreadCheckDesktopCapturer& operator=(const ThreadCheckDesktopCapturer&) =
      delete;

  ~ThreadCheckDesktopCapturer() override {
    EXPECT_TRUE(task_runner_->BelongsToCurrentThread());
  }

  void Start(Callback* callback) override {
    EXPECT_TRUE(task_runner_->BelongsToCurrentThread());
    EXPECT_FALSE(callback_);
    EXPECT_TRUE(callback);

    callback_ = callback;
  }

  void CaptureFrame() override {
    EXPECT_TRUE(task_runner_->BelongsToCurrentThread());

    std::unique_ptr<webrtc::DesktopFrame> frame(
        new webrtc::BasicDesktopFrame(webrtc::DesktopSize(kWidth, kHeight)));
    frame->mutable_updated_region()->SetRect(
        webrtc::DesktopRect::MakeXYWH(0, 0, 10, 10));
    callback_->OnCaptureResult(webrtc::DesktopCapturer::Result::SUCCESS,
                               std::move(frame));
  }

  bool GetSourceList(SourceList* sources) override {
    EXPECT_TRUE(task_runner_->BelongsToCurrentThread());
    return false;
  }

  bool SelectSource(SourceId id) override {
    EXPECT_TRUE(task_runner_->BelongsToCurrentThread());
    return true;
  }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  raw_ptr<webrtc::DesktopCapturer::Callback> callback_;
};

class VideoFramePumpTest : public testing::Test {
 public:
  void SetUp() override;
  void TearDown() override;

  void StartVideoFramePump(std::unique_ptr<webrtc::DesktopCapturer> capturer,
                           std::unique_ptr<VideoEncoder> encoder);

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::RunLoop run_loop_;
  scoped_refptr<AutoThreadTaskRunner> encode_task_runner_;
  scoped_refptr<AutoThreadTaskRunner> main_task_runner_;
  std::unique_ptr<VideoFramePump> pump_;

  MockVideoStub video_stub_;
};

void VideoFramePumpTest::SetUp() {
  main_task_runner_ = new AutoThreadTaskRunner(
      task_environment_.GetMainThreadTaskRunner(), run_loop_.QuitClosure());
  encode_task_runner_ = AutoThread::Create("encode", main_task_runner_);
}

void VideoFramePumpTest::TearDown() {
  pump_.reset();

  // Release the task runners, so that the test can quit.
  encode_task_runner_ = nullptr;
  main_task_runner_ = nullptr;

  // Run the MessageLoop until everything has torn down.
  run_loop_.Run();
}

// This test mocks capturer, encoder and network layer to simulate one capture
// cycle.
TEST_F(VideoFramePumpTest, StartAndStop) {
  std::unique_ptr<ThreadCheckDesktopCapturer> capturer(
      new ThreadCheckDesktopCapturer(main_task_runner_));
  std::unique_ptr<ThreadCheckVideoEncoder> encoder(
      new ThreadCheckVideoEncoder(encode_task_runner_));

  base::RunLoop run_loop;

  // When the first ProcessVideoPacket is received we stop the VideoFramePump.
  EXPECT_CALL(video_stub_, ProcessVideoPacketPtr(_, _))
      .WillOnce(DoAll(FinishSend(),
                      InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit)))
      .RetiresOnSaturation();

  // Start video frame capture.
  pump_ =
      std::make_unique<VideoFramePump>(encode_task_runner_, std::move(capturer),
                                       std::move(encoder), &video_stub_);

  // Run MessageLoop until the first frame is received.
  run_loop.Run();
}

// Tests that the pump handles null frames returned by the capturer.
TEST_F(VideoFramePumpTest, NullFrame) {
  std::unique_ptr<FakeDesktopCapturer> capturer(new FakeDesktopCapturer);
  std::unique_ptr<MockVideoEncoder> encoder(new MockVideoEncoder);

  base::RunLoop run_loop;

  // Set up the capturer to return null frames.
  capturer->set_frame_generator(base::BindRepeating(&CreateNullFrame));

  // Expect that the VideoEncoder::Encode() method is never called.
  EXPECT_CALL(*encoder, EncodePtr(_)).Times(0);

  // When the first ProcessVideoPacket is received we stop the VideoFramePump.
  EXPECT_CALL(video_stub_, ProcessVideoPacketPtr(_, _))
      .WillOnce(DoAll(FinishSend(),
                      InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit)))
      .RetiresOnSaturation();

  // Start video frame capture.
  pump_ =
      std::make_unique<VideoFramePump>(encode_task_runner_, std::move(capturer),
                                       std::move(encoder), &video_stub_);

  // Run MessageLoop until the first frame is received..
  run_loop.Run();
}

// Tests how the pump handles unchanged frames returned by the capturer.
TEST_F(VideoFramePumpTest, UnchangedFrame) {
  std::unique_ptr<FakeDesktopCapturer> capturer(new FakeDesktopCapturer);
  std::unique_ptr<MockVideoEncoder> encoder(new MockVideoEncoder);

  base::RunLoop run_loop;

  // Set up the capturer to return unchanged frames.
  capturer->set_frame_generator(base::BindRepeating(&CreateUnchangedFrame));

  // Expect that the VideoEncoder::Encode() method is called.
  EXPECT_CALL(*encoder, EncodePtr(_)).WillRepeatedly(Return(nullptr));

  // When the first ProcessVideoPacket is received we stop the VideoFramePump.
  // TODO(wez): Verify that the generated packet has no content here.
  EXPECT_CALL(video_stub_, ProcessVideoPacketPtr(_, _))
      .WillOnce(DoAll(FinishSend(),
                      InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit)))
      .RetiresOnSaturation();

  // Start video frame capture.
  pump_ =
      std::make_unique<VideoFramePump>(encode_task_runner_, std::move(capturer),
                                       std::move(encoder), &video_stub_);

  // Run MessageLoop until the first frame is received.
  run_loop.Run();
}

}  // namespace remoting::protocol
