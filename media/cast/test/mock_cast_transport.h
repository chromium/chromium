// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_TEST_MOCK_CAST_TRANSPORT_H_
#define MEDIA_CAST_TEST_MOCK_CAST_TRANSPORT_H_

#include <stdint.h>

#include "media/cast/net/cast_transport.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {
namespace cast {

class MockCastTransport : public CastTransport {
 public:
  MockCastTransport();
  ~MockCastTransport() override;

  MOCK_METHOD2(InsertFrame, void(uint32_t ssrc, const EncodedFrame& frame));
  MOCK_METHOD3(SendSenderReport,
               void(uint32_t ssrc,
                    base::TimeTicks current_time,
                    RtpTimeTicks current_time_as_rtp_timestamp));
  MOCK_METHOD2(CancelSendingFrames,
               void(uint32_t ssrc, const std::vector<FrameId>& frame_ids));
  MOCK_METHOD2(ResendFrameForKickstart, void(uint32_t ssrc, FrameId frame_id));
  MOCK_METHOD0(PacketReceiverForTesting, PacketReceiverCallback());
  MOCK_METHOD2(AddValidRtpReceiver,
               void(uint32_t rtp_sender_ssrc, uint32_t rtp_receiver_ssrc));
  MOCK_METHOD2(InitializeRtpReceiverRtcpBuilder,
               void(uint32_t rtp_receiver_ssrc, const RtcpTimeData& time_data));
  MOCK_METHOD2(AddCastFeedback,
               void(const RtcpCastMessage& cast_message,
                    base::TimeDelta target_delay));
  MOCK_METHOD1(AddPli, void(const RtcpPliMessage& pli_message));
  MOCK_METHOD1(
      AddRtcpEvents,
      void(const ReceiverRtcpEventSubscriber::RtcpEvents& rtcp_events));
  MOCK_METHOD1(AddRtpReceiverReport,
               void(const RtcpReportBlock& rtp_report_block));
  MOCK_METHOD0(SendRtcpFromRtpReceiver, void());
  MOCK_METHOD1(SetOptions, void(const base::Value::Dict& options));
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_TEST_MOCK_CAST_TRANSPORT_H_
