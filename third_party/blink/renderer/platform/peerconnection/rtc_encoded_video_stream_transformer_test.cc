// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/rtc_encoded_video_stream_transformer.h"

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_scoped_refptr_cross_thread_copier.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_std.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/webrtc/api/array_view.h"
#include "third_party/webrtc/api/frame_transformer_interface.h"
#include "third_party/webrtc/api/test/mock_transformable_video_frame.h"
#include "third_party/webrtc/api/units/time_delta.h"
#include "third_party/webrtc/rtc_base/ref_counted_object.h"

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SaveArg;

namespace blink {

namespace {

const uint32_t kSSRC = 1;
const uint32_t kNonexistentSSRC = 0;

class MockWebRtcTransformedFrameCallback
    : public webrtc::TransformedFrameCallback {
 public:
  MOCK_METHOD1(OnTransformedFrame,
               void(std::unique_ptr<webrtc::TransformableFrameInterface>));
  MOCK_METHOD0(StartShortCircuiting, void());
};

class MockTransformerCallbackHolder {
 public:
  MOCK_METHOD1(OnEncodedFrame,
               void(std::unique_ptr<webrtc::TransformableVideoFrameInterface>));
};

class MockMetronome : public webrtc::Metronome {
 public:
  MOCK_METHOD(void,
              RequestCallOnNextTick,
              (absl::AnyInvocable<void() &&> callback),
              (override));
  MOCK_METHOD(webrtc::TimeDelta, TickPeriod, (), (const, override));
};

std::unique_ptr<webrtc::MockTransformableVideoFrame> CreateMockFrame() {
  auto mock_frame =
      std::make_unique<NiceMock<webrtc::MockTransformableVideoFrame>>();
  ON_CALL(*mock_frame.get(), GetSsrc).WillByDefault(Return(kSSRC));
  return mock_frame;
}

}  // namespace

// Parameterized by bool whether to suply a metronome or not.
class RTCEncodedVideoStreamTransformerTest
    : public testing::TestWithParam<bool> {
 public:
  RTCEncodedVideoStreamTransformerTest()
      : main_task_runner_(
            blink::scheduler::GetSingleThreadTaskRunnerForTesting()),
        webrtc_task_runner_(base::ThreadPool::CreateSingleThreadTaskRunner({})),
        webrtc_callback_(
            new rtc::RefCountedObject<MockWebRtcTransformedFrameCallback>()),
        metronome_(GetParam() ? new NiceMock<MockMetronome>() : nullptr),
        encoded_video_stream_transformer_(main_task_runner_,
                                          absl::WrapUnique(metronome_.get())) {}

  void SetUp() override {
    EXPECT_FALSE(
        encoded_video_stream_transformer_.HasTransformedFrameSinkCallback(
            kSSRC));
    encoded_video_stream_transformer_.RegisterTransformedFrameSinkCallback(
        webrtc_callback_, kSSRC);
    EXPECT_TRUE(
        encoded_video_stream_transformer_.HasTransformedFrameSinkCallback(
            kSSRC));
    EXPECT_FALSE(
        encoded_video_stream_transformer_.HasTransformedFrameSinkCallback(
            kNonexistentSSRC));
    if (GetParam()) {
      ON_CALL(*metronome_, RequestCallOnNextTick(_))
          .WillByDefault([](absl::AnyInvocable<void()&&> callback) {
            std::move(callback)();
          });
    }
  }

  void TearDown() override {
    metronome_ = nullptr;
    encoded_video_stream_transformer_.UnregisterTransformedFrameSinkCallback(
        kSSRC);
    EXPECT_FALSE(
        encoded_video_stream_transformer_.HasTransformedFrameSinkCallback(
            kSSRC));
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> webrtc_task_runner_;
  rtc::scoped_refptr<MockWebRtcTransformedFrameCallback> webrtc_callback_;
  MockTransformerCallbackHolder mock_transformer_callback_holder_;
  raw_ptr<MockMetronome> metronome_;
  RTCEncodedVideoStreamTransformer encoded_video_stream_transformer_;
};

INSTANTIATE_TEST_SUITE_P(MetronomeAlignment,
                         RTCEncodedVideoStreamTransformerTest,
                         testing::Values(true, false));

TEST_P(RTCEncodedVideoStreamTransformerTest,
       TransformerForwardsFrameToTransformerCallback) {
  EXPECT_FALSE(encoded_video_stream_transformer_.HasTransformerCallback());
  encoded_video_stream_transformer_.SetTransformerCallback(
      WTF::CrossThreadBindRepeating(
          &MockTransformerCallbackHolder::OnEncodedFrame,
          WTF::CrossThreadUnretained(&mock_transformer_callback_holder_)));
  EXPECT_TRUE(encoded_video_stream_transformer_.HasTransformerCallback());

  EXPECT_CALL(mock_transformer_callback_holder_, OnEncodedFrame);
  // Frames are pushed to the RTCEncodedVideoStreamTransformer via its delegate,
  // which  would normally be registered with a WebRTC sender or receiver.
  // In this test, manually send the frame to the transformer on the simulated
  // WebRTC thread.
  PostCrossThreadTask(
      *webrtc_task_runner_, FROM_HERE,
      CrossThreadBindOnce(&webrtc::FrameTransformerInterface::Transform,
                          encoded_video_stream_transformer_.Delegate(),
                          CreateMockFrame()));
  task_environment_.RunUntilIdle();
}

TEST_P(RTCEncodedVideoStreamTransformerTest, TransformerForwardsFrameToWebRTC) {
  EXPECT_CALL(*webrtc_callback_, OnTransformedFrame);
  encoded_video_stream_transformer_.SendFrameToSink(CreateMockFrame());
  task_environment_.RunUntilIdle();
}

TEST_P(RTCEncodedVideoStreamTransformerTest, IgnoresSsrcForSinglecast) {
  EXPECT_CALL(*webrtc_callback_, OnTransformedFrame);
  std::unique_ptr<webrtc::MockTransformableVideoFrame> mock_frame =
      CreateMockFrame();
  EXPECT_CALL(*mock_frame.get(), GetSsrc)
      .WillRepeatedly(Return(kNonexistentSSRC));
  encoded_video_stream_transformer_.SendFrameToSink(std::move(mock_frame));
  task_environment_.RunUntilIdle();
}

TEST_P(RTCEncodedVideoStreamTransformerTest, ShortCircuitingPropagated) {
  EXPECT_CALL(*webrtc_callback_, StartShortCircuiting);
  encoded_video_stream_transformer_.StartShortCircuiting();
  task_environment_.RunUntilIdle();
}

TEST_P(RTCEncodedVideoStreamTransformerTest,
       ShortCircuitingSetOnLateRegisteredCallback) {
  EXPECT_CALL(*webrtc_callback_, StartShortCircuiting);
  encoded_video_stream_transformer_.StartShortCircuiting();

  rtc::scoped_refptr<MockWebRtcTransformedFrameCallback> webrtc_callback_2(
      new rtc::RefCountedObject<MockWebRtcTransformedFrameCallback>());
  EXPECT_CALL(*webrtc_callback_2, StartShortCircuiting);
  encoded_video_stream_transformer_.RegisterTransformedFrameSinkCallback(
      webrtc_callback_2, kSSRC + 1);
}

TEST_P(RTCEncodedVideoStreamTransformerTest, WaitsForMetronomeTick) {
  if (!GetParam()) {
    return;
  }
  encoded_video_stream_transformer_.SetTransformerCallback(
      WTF::CrossThreadBindRepeating(
          &MockTransformerCallbackHolder::OnEncodedFrame,
          WTF::CrossThreadUnretained(&mock_transformer_callback_holder_)));
  ASSERT_TRUE(encoded_video_stream_transformer_.HasTransformerCallback());

  // There should be no transform call initially.
  EXPECT_CALL(mock_transformer_callback_holder_, OnEncodedFrame).Times(0);
  absl::AnyInvocable<void() &&> callback;
  EXPECT_CALL(*metronome_, RequestCallOnNextTick)
      .WillOnce(
          [&](absl::AnyInvocable<void()&&> c) { callback = std::move(c); });
  const size_t transform_count = 5;
  for (size_t i = 0; i < transform_count; i++) {
    PostCrossThreadTask(
        *webrtc_task_runner_, FROM_HERE,
        CrossThreadBindOnce(&webrtc::FrameTransformerInterface::Transform,
                            encoded_video_stream_transformer_.Delegate(),
                            CreateMockFrame()));
  }
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(callback);

  // But when the metronome ticks, all calls arrive.
  EXPECT_CALL(mock_transformer_callback_holder_, OnEncodedFrame)
      .Times(transform_count);
  // Must be done on the same sequence as the transform calls.
  PostCrossThreadTask(*webrtc_task_runner_, FROM_HERE,
                      CrossThreadBindOnce(
                          [](absl::AnyInvocable<void()&&>* callback) {
                            std::move (*callback)();
                          },
                          CrossThreadUnretained(&callback)));

  task_environment_.RunUntilIdle();
}

TEST_P(RTCEncodedVideoStreamTransformerTest,
       FramesBufferedBeforeShortcircuiting) {
  // Send some frames to be transformed before shortcircuiting.
  const size_t transform_count = 5;
  for (size_t i = 0; i < transform_count; i++) {
    PostCrossThreadTask(
        *webrtc_task_runner_, FROM_HERE,
        CrossThreadBindOnce(&webrtc::FrameTransformerInterface::Transform,
                            encoded_video_stream_transformer_.Delegate(),
                            CreateMockFrame()));
  }

  task_environment_.RunUntilIdle();

  // All frames should be passed back once short circuiting starts.
  EXPECT_CALL(*webrtc_callback_, OnTransformedFrame).Times(transform_count);
  EXPECT_CALL(*webrtc_callback_, StartShortCircuiting);
  encoded_video_stream_transformer_.StartShortCircuiting();

  task_environment_.RunUntilIdle();
}

TEST_P(RTCEncodedVideoStreamTransformerTest,
       FrameArrivingAfterShortcircuitingIsPassedBack) {
  EXPECT_CALL(*webrtc_callback_, StartShortCircuiting);
  encoded_video_stream_transformer_.StartShortCircuiting();

  // Frames passed to Transform after shortcircuting should be passed straight
  // back.
  PostCrossThreadTask(
      *webrtc_task_runner_, FROM_HERE,
      CrossThreadBindOnce(&webrtc::FrameTransformerInterface::Transform,
                          encoded_video_stream_transformer_.Delegate(),
                          CreateMockFrame()));

  EXPECT_CALL(*webrtc_callback_, OnTransformedFrame);
  task_environment_.RunUntilIdle();
}

TEST_P(RTCEncodedVideoStreamTransformerTest,
       FramesBufferedBeforeSettingTransform) {
  // Send some frames to be transformed before a transform is set.
  const size_t transform_count = 5;
  for (size_t i = 0; i < transform_count; i++) {
    PostCrossThreadTask(
        *webrtc_task_runner_, FROM_HERE,
        CrossThreadBindOnce(&webrtc::FrameTransformerInterface::Transform,
                            encoded_video_stream_transformer_.Delegate(),
                            CreateMockFrame()));
  }

  task_environment_.RunUntilIdle();

  // All frames should be passed as soon as a transform callback is provided
  EXPECT_CALL(mock_transformer_callback_holder_, OnEncodedFrame)
      .Times(transform_count);
  encoded_video_stream_transformer_.SetTransformerCallback(
      WTF::CrossThreadBindRepeating(
          &MockTransformerCallbackHolder::OnEncodedFrame,
          WTF::CrossThreadUnretained(&mock_transformer_callback_holder_)));
}

}  // namespace blink
