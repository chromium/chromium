// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/rtc_encoded_audio_stream_transformer.h"

#include <stdint.h>

#include <memory>
#include <vector>

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
#include "third_party/webrtc/api/frame_transformer_interface.h"
#include "third_party/webrtc/rtc_base/ref_counted_object.h"

namespace blink {

namespace {

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
            new rtc::RefCountedObject<MockWebRtcTransformedFrameCallback>()),
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
  rtc::scoped_refptr<MockWebRtcTransformedFrameCallback> webrtc_callback_;
  MockTransformerCallbackHolder mock_transformer_callback_holder_;
  RTCEncodedAudioStreamTransformer encoded_audio_stream_transformer_;
};

TEST_F(RTCEncodedAudioStreamTransformerTest,
       TransformerForwardsFrameToTransformerCallback) {
  EXPECT_FALSE(encoded_audio_stream_transformer_.HasTransformerCallback());
  encoded_audio_stream_transformer_.SetTransformerCallback(
      WTF::CrossThreadBindRepeating(
          &MockTransformerCallbackHolder::OnEncodedFrame,
          WTF::CrossThreadUnretained(&mock_transformer_callback_holder_)));
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

  rtc::scoped_refptr<MockWebRtcTransformedFrameCallback> webrtc_callback_2(
      new rtc::RefCountedObject<MockWebRtcTransformedFrameCallback>());
  EXPECT_CALL(*webrtc_callback_2, StartShortCircuiting);
  encoded_audio_stream_transformer_.RegisterTransformedFrameCallback(
      webrtc_callback_2);
}

}  // namespace blink
