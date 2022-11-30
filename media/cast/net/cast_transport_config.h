// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_NET_CAST_TRANSPORT_CONFIG_H_
#define MEDIA_CAST_NET_CAST_TRANSPORT_CONFIG_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/time/time.h"
#include "media/cast/cast_config.h"
#include "media/cast/common/rtp_time.h"
#include "media/cast/net/cast_transport_defines.h"

namespace media {
namespace cast {

struct CastTransportRtpConfig {
  CastTransportRtpConfig();
  ~CastTransportRtpConfig();

  // Identifier for the RTP stream.
  int32_t rtp_stream_id;

  // Identifier refering to this sender.
  uint32_t ssrc;

  // Identifier for incoming RTCP traffic.
  uint32_t feedback_ssrc;

  // RTP payload type enum: Specifies the type/encoding of frame data.
  RtpPayloadType rtp_payload_type;

  // The AES crypto key and initialization vector.  Each of these strings
  // contains the data in binary form, of size kAesKeySize.  If they are empty
  // strings, crypto is not being used.
  std::string aes_key;
  std::string aes_iv_mask;
};

using PacketReceiverCallback =
    base::RepeatingCallback<void(std::unique_ptr<Packet> packet)>;
using PacketReceiverCallbackWithStatus =
    base::RepeatingCallback<bool(std::unique_ptr<Packet> packet)>;

class PacketTransport {
 public:
  // Send a packet to the network. Returns false if the network is blocked
  // and we should wait for |cb| to be called. It is not allowed to called
  // SendPacket again until |cb| has been called. Any other errors that
  // occur will be reported through side channels, in such cases, this function
  // will return true indicating that the channel is not blocked.
  virtual bool SendPacket(PacketRef packet, base::OnceClosure cb) = 0;

  // Returns the number of bytes ever sent.
  virtual int64_t GetBytesSent() = 0;

  // Start receiving packets. Pakets are submitted to |packet_receiver|.
  virtual void StartReceiving(
      PacketReceiverCallbackWithStatus packet_receiver) = 0;

  // Stop receiving packets.
  virtual void StopReceiving() = 0;

  virtual ~PacketTransport() {}
};

struct RtcpSenderInfo {
  RtcpSenderInfo();
  ~RtcpSenderInfo();
  // First three members are used for lipsync.
  // Last two members are used for rtt.
  uint32_t ntp_seconds;
  uint32_t ntp_fraction;
  RtpTimeTicks rtp_timestamp;
  uint32_t send_packet_count;
  size_t send_octet_count;
};

struct RtcpReportBlock {
  RtcpReportBlock();
  ~RtcpReportBlock();
  uint32_t remote_ssrc;  // SSRC of sender of this report.
  uint32_t media_ssrc;   // SSRC of the RTP packet sender.
  uint8_t fraction_lost;
  uint32_t cumulative_lost;  // 24 bits valid.
  uint32_t extended_high_sequence_number;
  uint32_t jitter;
  uint32_t last_sr;
  uint32_t delay_since_last_sr;
};

struct RtcpDlrrReportBlock {
  RtcpDlrrReportBlock();
  ~RtcpDlrrReportBlock();
  uint32_t last_rr;
  uint32_t delay_since_last_rr;
};

inline bool operator==(RtcpSenderInfo lhs, RtcpSenderInfo rhs) {
  return lhs.ntp_seconds == rhs.ntp_seconds &&
         lhs.ntp_fraction == rhs.ntp_fraction &&
         lhs.rtp_timestamp == rhs.rtp_timestamp &&
         lhs.send_packet_count == rhs.send_packet_count &&
         lhs.send_octet_count == rhs.send_octet_count;
}

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_NET_CAST_TRANSPORT_CONFIG_H_
