// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_NET_RTCP_RTCP_BUILDER_H_
#define MEDIA_CAST_NET_RTCP_RTCP_BUILDER_H_

#include <stddef.h>
#include <stdint.h>

#include "base/big_endian.h"
#include "base/memory/raw_ptr.h"
#include "media/cast/net/cast_transport_config.h"
#include "media/cast/net/cast_transport_defines.h"
#include "media/cast/net/rtcp/receiver_rtcp_event_subscriber.h"
#include "media/cast/net/rtcp/rtcp_defines.h"

namespace media {
namespace cast {

class RtcpBuilder {
 public:
  explicit RtcpBuilder(uint32_t sending_ssrc);

  RtcpBuilder(const RtcpBuilder&) = delete;
  RtcpBuilder& operator=(const RtcpBuilder&) = delete;

  ~RtcpBuilder();

  PacketRef BuildRtcpFromSender(const RtcpSenderInfo& sender_info);

  uint32_t local_ssrc() const { return local_ssrc_; }
  void AddRR(const RtcpReportBlock* report_block);
  void AddRrtr(const RtcpReceiverReferenceTimeReport& rrtr);
  void AddCast(const RtcpCastMessage& cast_message,
               base::TimeDelta target_delay);
  void AddPli(const RtcpPliMessage& pli_message);
  void AddReceiverLog(
      const ReceiverRtcpEventSubscriber::RtcpEvents& rtcp_events);
  void Start();
  PacketRef Finish();

 private:
  void AddRtcpHeader(RtcpPacketFields payload, int format_or_count);
  void PatchLengthField();
  void AddSR(const RtcpSenderInfo& sender_info);
  void AddDlrrRb(const RtcpDlrrReportBlock& dlrr);
  void AddReportBlocks(const RtcpReportBlock& report_block);

  bool GetRtcpReceiverLogMessage(
      const ReceiverRtcpEventSubscriber::RtcpEvents& rtcp_events,
      RtcpReceiverLogMessage* receiver_log_message,
      size_t* total_number_of_messages_to_send);

  const uint32_t local_ssrc_;
  raw_ptr<char, AllowPtrArithmetic> ptr_of_length_;
  PacketRef packet_;
  base::BigEndianWriter writer_;
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_NET_RTCP_RTCP_BUILDER_H_
