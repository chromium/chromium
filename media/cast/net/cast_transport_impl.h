// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class maintains a transport for audio and video in a Cast Streaming
// session.
// Audio, video frames and RTCP messages are submitted to this object
// and then packetized and paced to the underlying UDP socket.
//
// The hierarchy of send transport in a Cast Streaming session:
//
// CastTransport              RTP                      RTCP
// ------------------------------------------------------------------
//                      TransportEncryptionHandler (A/V)
//                      RtpSender (A/V)                   Rtcp (A/V)
//                                      PacedSender (Shared)
//                                      UdpTransport (Shared)
//
// There are objects of TransportEncryptionHandler, RtpSender and Rtcp
// for each audio and video stream.
// PacedSender and UdpTransport are shared between all RTP and RTCP
// streams.

#ifndef MEDIA_CAST_NET_CAST_TRANSPORT_IMPL_H_
#define MEDIA_CAST_NET_CAST_TRANSPORT_IMPL_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <vector>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "media/cast/common/transport_encryption_handler.h"
#include "media/cast/logging/logging_defines.h"
#include "media/cast/net/cast_transport.h"
#include "media/cast/net/cast_transport_config.h"
#include "media/cast/net/pacing/paced_sender.h"
#include "media/cast/net/rtcp/rtcp_builder.h"
#include "media/cast/net/rtcp/sender_rtcp_session.h"
#include "media/cast/net/rtp/rtp_parser.h"
#include "media/cast/net/rtp/rtp_sender.h"
#include "net/base/network_interfaces.h"

namespace media {
namespace cast {

class CastTransportImpl final : public CastTransport {
 public:
  CastTransportImpl(
      const base::TickClock* clock,  // Owned by the caller.
      base::TimeDelta logging_flush_interval,
      std::unique_ptr<Client> client,
      std::unique_ptr<PacketTransport> transport,
      const scoped_refptr<base::SingleThreadTaskRunner>& transport_task_runner);

  ~CastTransportImpl() final;

  // CastTransport implementation for sending.
  void InitializeStream(const CastTransportRtpConfig& config,
                        std::unique_ptr<RtcpObserver> rtcp_observer) final;
  void InsertFrame(uint32_t ssrc, const EncodedFrame& frame) final;

  void SendSenderReport(uint32_t ssrc,
                        base::TimeTicks current_time,
                        RtpTimeTicks current_time_as_rtp_timestamp) final;

  void CancelSendingFrames(uint32_t ssrc,
                           const std::vector<FrameId>& frame_ids) final;

  void ResendFrameForKickstart(uint32_t ssrc, FrameId frame_id) final;

  PacketReceiverCallback PacketReceiverForTesting() final;

  // Possible keys of |options| handled here are:
  //   "pacer_target_burst_size": int
  //        - Specifies how many packets to send per 10 ms ideally.
  //   "pacer_max_burst_size": int
  //        - Specifies how many pakcets to send per 10 ms, maximum.
  //   "send_buffer_min_size": int
  //        - Specifies the minimum socket send buffer size.
  //   "disable_wifi_scan" (value ignored)
  //        - Disable wifi scans while streaming.
  //   "media_streaming_mode" (value ignored)
  //        - Turn media streaming mode on.
  // Note, these options may be ignored on some platforms.
  void SetOptions(const base::DictionaryValue& options) final;

  // CastTransport implementation for receiving.
  void AddValidRtpReceiver(uint32_t rtp_sender_ssrc,
                           uint32_t rtp_receiver_ssrc) final;

  // Building and sending RTCP packet from RTP receiver implementation.
  void InitializeRtpReceiverRtcpBuilder(uint32_t rtp_receiver_ssrc,
                                        const RtcpTimeData& time_data) final;
  void AddCastFeedback(const RtcpCastMessage& cast_message,
                       base::TimeDelta target_delay) final;
  void AddPli(const RtcpPliMessage& pli_message) final;
  void AddRtcpEvents(
      const ReceiverRtcpEventSubscriber::RtcpEvents& rtcp_events) final;
  void AddRtpReceiverReport(
      const RtcpReportBlock& rtp_receiver_report_block) final;
  void SendRtcpFromRtpReceiver() final;

 private:
  // Handle received RTCP messages on RTP sender.
  class RtcpClient;

  struct RtpStreamSession;

  FRIEND_TEST_ALL_PREFIXES(CastTransportImplTest, NacksCancelRetransmits);
  FRIEND_TEST_ALL_PREFIXES(CastTransportImplTest, CancelRetransmits);
  FRIEND_TEST_ALL_PREFIXES(CastTransportImplTest, Kickstart);
  FRIEND_TEST_ALL_PREFIXES(CastTransportImplTest, DedupRetransmissionWithAudio);

  // Resend packets for the stream identified by |ssrc|.
  // If |cancel_rtx_if_not_in_list| is true then transmission of packets for the
  // frames but not in the list will be dropped.
  // See PacedSender::ResendPackets() to see how |dedup_info| works.
  void ResendPackets(uint32_t ssrc,
                     const MissingFramesAndPacketsMap& missing_packets,
                     bool cancel_rtx_if_not_in_list,
                     const DedupInfo& dedup_info);

  // If |logging_flush_interval| is set, this is called at approximate periodic
  // intervals.
  void SendRawEvents();

  // Called when a packet is received.
  bool OnReceivedPacket(std::unique_ptr<Packet> packet);

  // Called when a log message is received.
  void OnReceivedLogMessage(EventMediaType media_type,
                            const RtcpReceiverLogMessage& log);

  // Called when a RTCP Cast message is received.
  void OnReceivedCastMessage(uint32_t ssrc,
                             const RtcpCastMessage& cast_message);

  const base::TickClock* const clock_;  // Not owned by this class.
  const base::TimeDelta logging_flush_interval_;
  const std::unique_ptr<Client> transport_client_;
  const std::unique_ptr<PacketTransport> transport_;
  const scoped_refptr<base::SingleThreadTaskRunner> transport_task_runner_;

  // FrameEvents and PacketEvents pending delivery via raw events callback.
  // Do not add elements to these when |logging_flush_interval| is
  // |base::TimeDelta()|.
  std::vector<FrameEvent> recent_frame_events_;
  std::vector<PacketEvent> recent_packet_events_;

  // Packet sender that performs pacing.
  PacedSender pacer_;

  // Right after a frame is sent we record the number of bytes sent to the
  // socket. We record the corresponding bytes sent for the most recent ACKed
  // audio packet.
  int64_t last_byte_acked_for_audio_;

  // Packets that don't match these sender ssrcs are ignored.
  std::set<uint32_t> valid_sender_ssrcs_;

  // While non-null, global WiFi behavior modifications are in effect. This is
  // used, for example, to turn off WiFi scanning that tends to interfere with
  // the reliability of UDP packet transmission.
  std::unique_ptr<net::ScopedWifiOptions> wifi_options_autoreset_;

  // Do not initialize the |rtcp_builder_at_rtp_receiver_| if the RTP receiver
  // SSRC does not match these ssrcs. Only RTP receiver needs to register its
  // SSRC in this set.
  std::set<uint32_t> valid_rtp_receiver_ssrcs_;

  std::unique_ptr<RtcpBuilder> rtcp_builder_at_rtp_receiver_;

  // Records the initialized stream sessions on RTP sender. The sender SSRC is
  // used as key since it is unique for each RTP stream.
  using SessionMap = std::map<uint32_t, std::unique_ptr<RtpStreamSession>>;
  SessionMap sessions_;

  base::WeakPtrFactory<CastTransportImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CastTransportImpl);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_NET_CAST_TRANSPORT_IMPL_H_
