// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_transport_processor.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/webrtc/api/transport/network_control.h"

namespace blink {

const uint32_t kMaxCount = 10;

class RTCRtpTransportProcessorTest : public ::testing::Test {
 public:
  void SetUp() override {}

 protected:
  test::TaskEnvironment task_environment_;
};

webrtc::TransportPacketsFeedback CreateFeedback(size_t packet_count,
                                                uint32_t feedback_time_ms) {
  webrtc::TransportPacketsFeedback feedback;
  feedback.feedback_time = webrtc::Timestamp::Millis(feedback_time_ms);
  for (size_t i = 0; i < packet_count; i++) {
    webrtc::PacketResult packet_result;
    packet_result.receive_time =
        webrtc::Timestamp::Millis(feedback_time_ms - packet_count + i);
    feedback.packet_feedbacks.push_back(packet_result);
  }
  return feedback;
}

TEST_F(RTCRtpTransportProcessorTest, EmptyReadReceivedAcks) {
  V8TestingScope scope_;
  RTCRtpTransportProcessor* processor =
      MakeGarbageCollected<RTCRtpTransportProcessor>(
          scope_.GetExecutionContext());
  EXPECT_EQ(processor->readReceivedAcks(kMaxCount).size(), 0ul);
}

TEST_F(RTCRtpTransportProcessorTest, ReadReceivedAcksReturnsFewerThanMax) {
  V8TestingScope scope_;
  RTCRtpTransportProcessor* processor =
      MakeGarbageCollected<RTCRtpTransportProcessor>(
          scope_.GetExecutionContext());

  processor->OnFeedback(CreateFeedback(0, 0));
  processor->OnFeedback(CreateFeedback(10, 1000));
  processor->OnFeedback(CreateFeedback(20, 2000));
  processor->OnFeedback(CreateFeedback(30, 3000));

  HeapVector<Member<RTCRtpAcks>> acks_vector =
      processor->readReceivedAcks(kMaxCount);
  EXPECT_EQ(acks_vector.size(), 4u);
  for (size_t i = 0; i < 4; i++) {
    EXPECT_EQ(acks_vector[i]->remoteSendTimestamp(), i * 1000);
    EXPECT_EQ(acks_vector[i]->acks().size(), i * 10);
  }
}

TEST_F(RTCRtpTransportProcessorTest, ReadReceivedAcksTruncatesToFirstMax) {
  V8TestingScope scope_;
  RTCRtpTransportProcessor* processor =
      MakeGarbageCollected<RTCRtpTransportProcessor>(
          scope_.GetExecutionContext());
  // Receive kMaxCount*2 feedbacks, with increasing packet counts and feedback
  // timestamps;
  for (size_t i = 0; i < kMaxCount * 2; i++) {
    processor->OnFeedback(CreateFeedback(i * 10, i * 1000));
  }

  // Reading kMaxCount should return the first kMaxCount acks objects.
  HeapVector<Member<RTCRtpAcks>> acks_vector =
      processor->readReceivedAcks(kMaxCount);
  EXPECT_EQ(acks_vector.size(), kMaxCount);
  for (size_t i = 0; i < kMaxCount; i++) {
    EXPECT_EQ(acks_vector[i]->remoteSendTimestamp(), i * 1000);
    EXPECT_EQ(acks_vector[i]->acks().size(), i * 10);
  }

  // Reading again kMaxCount should return the remaining kMaxCount acks objects.
  acks_vector = processor->readReceivedAcks(kMaxCount);
  EXPECT_EQ(acks_vector.size(), kMaxCount);
  for (size_t i = 0; i < kMaxCount; i++) {
    EXPECT_EQ(acks_vector[i]->remoteSendTimestamp(), (i + kMaxCount) * 1000);
    EXPECT_EQ(acks_vector[i]->acks().size(), (i + kMaxCount) * 10);
  }
}

}  // namespace blink
