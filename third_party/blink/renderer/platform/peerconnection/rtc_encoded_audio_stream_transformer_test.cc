// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/rtc_encoded_audio_stream_transformer.h"

#include <stdint.h>

#include <memory>

#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/webrtc/api/frame_transformer_interface.h"
#include "third_party/webrtc/api/scoped_refptr.h"
#include "third_party/webrtc/rtc_base/ref_counted_object.h"

namespace blink {

namespace {

class MockWebRtcTransformedFrameCallback
    : public webrtc::TransformedFrameCallback {
 public:
  MOCK_METHOD1(OnTransformedFrame,
               void(std::unique_ptr<webrtc::TransformableFrameInterface>));
  MOCK_METHOD0(StartShortCircuiting, void());
  MOCK_METHOD0(StopShortCircuiting, void());
};

class MockTransformerCallbackHolder {
 public:
  MOCK_METHOD1(OnEncodedFrame,
               void(std::unique_ptr<webrtc::TransformableAudioFrameInterface>));
};

}  // namespace

class RTCEncodedAudioStreamTransformerTest : public ::testing::Test {
 public:
  RTCEncodedAudioStreamTransformerTest()
      : main_task_runner_(
            blink::scheduler::GetSingleThreadTaskRunnerForTesting()),
        webrtc_task_runner_(base::ThreadPool::CreateSingleThreadTaskRunner({})),
        webrtc_callback_(
            new webrtc::RefCountedObject<MockWebRtcTransformedFrameCallback>()),
        encoded_audio_stream_transformer_(main_task_runner_) {}

  void SetUp() override {
    EXPECT_FALSE(
        encoded_audio_stream_transformer_.HasTransformedFrameCallback());
    encoded_audio_stream_transformer_.RegisterTransformedFrameCallback(
        webrtc_callback_);
    EXPECT_TRUE(
        encoded_audio_stream_transformer_.HasTransformedFrameCallback());
  }

  void TearDown() override {
    encoded_audio_stream_transformer_.UnregisterTransformedFrameCallback();
    EXPECT_FALSE(
        encoded_audio_stream_transformer_.HasTransformedFrameCallback());
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> webrtc_task_runner_;
  webrtc::scoped_refptr<MockWebRtcTransformedFrameCallback> webrtc_callback_;
  MockTransformerCallbackHolder mock_transformer_callback_holder_;
  RTCEncodedAudioStreamTransformer encoded_audio_stream_transformer_;
};

TEST_F(RTCEncodedAudioStreamTransformerTest,
       TransformerForwardsFrameToTransformerCallback) {
  EXPECT_FALSE(encoded_audio_stream_transformer_.HasTransformerCallback());
  encoded_audio_stream_transformer_.SetTransformerCallback(
      CrossThreadBindRepeating(
          &MockTransformerCallbackHolder::OnEncodedFrame,
          CrossThreadUnretained(&mock_transformer_callback_holder_)));
  EXPECT_TRUE(encoded_audio_stream_transformer_.HasTransformerCallback());

  EXPECT_CALL(mock_transformer_callback_holder_, OnEncodedFrame);
  // Frames are pushed to the RTCEncodedAudioStreamTransformer via its delegate,
  // which  would normally be registered with a WebRTC sender or receiver.
  // In this test, manually send the frame to the transformer on the simulated
  // WebRTC thread.
  PostCrossThreadTask(
      *webrtc_task_runner_, FROM_HERE,
      CrossThreadBindOnce(&webrtc::FrameTransformerInterface::Transform,
                          encoded_audio_stream_transformer_.Delegate(),
                          nullptr));
  task_environment_.RunUntilIdle();
}

TEST_F(RTCEncodedAudioStreamTransformerTest, TransformerForwardsFrameToWebRTC) {
  EXPECT_CALL(*webrtc_callback_, OnTransformedFrame);
  encoded_audio_stream_transformer_.SendFrameToSink(nullptr);
  task_environment_.RunUntilIdle();
}

TEST_F(RTCEncodedAudioStreamTransformerTest, ShortCircuitingPropagated) {
  EXPECT_CALL(*webrtc_callback_, StartShortCircuiting);
  encoded_audio_stream_transformer_.StartShortCircuiting();
  task_environment_.RunUntilIdle();
}

TEST_F(RTCEncodedAudioStreamTransformerTest,
       ShortCircuitingSetOnLateRegisteredCallback) {
  EXPECT_CALL(*webrtc_callback_, StartShortCircuiting);
  encoded_audio_stream_transformer_.StartShortCircuiting();

  webrtc::scoped_refptr<MockWebRtcTransformedFrameCallback> webrtc_callback_2(
      new webrtc::RefCountedObject<MockWebRtcTransformedFrameCallback>());
  EXPECT_CALL(*webrtc_callback_2, StartShortCircuiting);
  encoded_audio_stream_transformer_.RegisterTransformedFrameCallback(
      webrtc_callback_2);
}

TEST_F(RTCEncodedAudioStreamTransformerTest, StopShortCircuitingPropagated) {
  EXPECT_CALL(*webrtc_callback_, StartShortCircuiting);
  encoded_audio_stream_transformer_.StartShortCircuiting();

  EXPECT_CALL(*webrtc_callback_, StopShortCircuiting);
  encoded_audio_stream_transformer_.StopShortCircuiting();
}

TEST_F(RTCEncodedAudioStreamTransformerTest,
       ShortCircuitingNotSetOnLateRegisteredCallbackAfterStop) {
  EXPECT_CALL(*webrtc_callback_, StartShortCircuiting);
  EXPECT_CALL(*webrtc_callback_, StopShortCircuiting);
  encoded_audio_stream_transformer_.StartShortCircuiting();
  encoded_audio_stream_transformer_.StopShortCircuiting();

  webrtc::scoped_refptr<MockWebRtcTransformedFrameCallback> webrtc_callback_2(
      new webrtc::RefCountedObject<MockWebRtcTransformedFrameCallback>());
  EXPECT_CALL(*webrtc_callback_2, StartShortCircuiting).Times(0);
  encoded_audio_stream_transformer_.RegisterTransformedFrameCallback(
      webrtc_callback_2);
}

TEST_F(RTCEncodedAudioStreamTransformerTest,
       ReentrantSendFrameToSinkDuringTransformFrame) {
  scoped_refptr<RTCEncodedAudioStreamTransformer::Broker> broker =
      encoded_audio_stream_transformer_.GetBroker();
  broker->SetTransformerCallback(CrossThreadBindRepeating(
      [](scoped_refptr<RTCEncodedAudioStreamTransformer::Broker> broker,
         std::unique_ptr<webrtc::TransformableAudioFrameInterface> frame) {
        broker->SendFrameToSink(std::move(frame));
      },
      broker));

  EXPECT_CALL(*webrtc_callback_, OnTransformedFrame);
  broker->TransformFrameOnSourceTaskRunner(nullptr);
}

TEST_F(RTCEncodedAudioStreamTransformerTest,
       TransformerDestroyedDuringTransformFrame) {
  auto transformer =
      std::make_unique<RTCEncodedAudioStreamTransformer>(main_task_runner_);
  scoped_refptr<RTCEncodedAudioStreamTransformer::Broker> broker =
      transformer->GetBroker();
  bool callback_invoked = false;
  broker->SetTransformerCallback(CrossThreadBindRepeating(
      [](std::unique_ptr<RTCEncodedAudioStreamTransformer>* transformer,
         scoped_refptr<RTCEncodedAudioStreamTransformer::Broker> broker,
         bool* callback_invoked,
         std::unique_ptr<webrtc::TransformableAudioFrameInterface> frame) {
        *callback_invoked = true;
        transformer->reset();
        broker->SendFrameToSink(std::move(frame));
      },
      CrossThreadUnretained(&transformer), broker,
      CrossThreadUnretained(&callback_invoked)));

  broker->TransformFrameOnSourceTaskRunner(nullptr);
  EXPECT_TRUE(callback_invoked);
  EXPECT_EQ(transformer, nullptr);
}

TEST_F(RTCEncodedAudioStreamTransformerTest,
       ReentrantResetTransformerCallbackDuringTransformFrame) {
  scoped_refptr<RTCEncodedAudioStreamTransformer::Broker> broker =
      encoded_audio_stream_transformer_.GetBroker();
  int callback_count = 0;
  broker->SetTransformerCallback(CrossThreadBindRepeating(
      [](scoped_refptr<RTCEncodedAudioStreamTransformer::Broker> broker,
         int* callback_count,
         std::unique_ptr<webrtc::TransformableAudioFrameInterface> frame) {
        ++(*callback_count);
        broker->ResetTransformerCallback();
      },
      broker, CrossThreadUnretained(&callback_count)));

  broker->TransformFrameOnSourceTaskRunner(nullptr);
  EXPECT_EQ(callback_count, 1);
  EXPECT_FALSE(encoded_audio_stream_transformer_.HasTransformerCallback());

  // Subsequent frames must not invoke the reset callback.
  broker->TransformFrameOnSourceTaskRunner(nullptr);
  EXPECT_EQ(callback_count, 1);
}

TEST_F(RTCEncodedAudioStreamTransformerTest,
       ReentrantSetTransformerCallbackDuringTransformFrame) {
  scoped_refptr<RTCEncodedAudioStreamTransformer::Broker> broker =
      encoded_audio_stream_transformer_.GetBroker();
  int first_callback_count = 0;
  int second_callback_count = 0;
  broker->SetTransformerCallback(CrossThreadBindRepeating(
      [](scoped_refptr<RTCEncodedAudioStreamTransformer::Broker> broker,
         int* first_callback_count, int* second_callback_count,
         std::unique_ptr<webrtc::TransformableAudioFrameInterface> frame) {
        ++(*first_callback_count);
        broker->SetTransformerCallback(CrossThreadBindRepeating(
            [](int* second_callback_count,
               std::unique_ptr<webrtc::TransformableAudioFrameInterface>
                   frame) { ++(*second_callback_count); },
            CrossThreadUnretained(second_callback_count)));
      },
      broker, CrossThreadUnretained(&first_callback_count),
      CrossThreadUnretained(&second_callback_count)));

  broker->TransformFrameOnSourceTaskRunner(nullptr);
  EXPECT_EQ(first_callback_count, 1);
  EXPECT_EQ(second_callback_count, 0);

  broker->TransformFrameOnSourceTaskRunner(nullptr);
  EXPECT_EQ(first_callback_count, 1);
  EXPECT_EQ(second_callback_count, 1);
}

}  // namespace blink
