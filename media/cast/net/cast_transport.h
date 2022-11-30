// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is the main interface for the cast transport sender.  It accepts encoded
// frames (both audio and video), encrypts their encoded data, packetizes them
// and feeds them into a transport (e.g., UDP).

// Construction of the Cast Sender and the Cast Transport should be done
// in the following order:
// 1. Create CastTransport.
// 2. Create CastSender (accepts CastTransport as an input).

// Destruction: The CastTransport is assumed to be valid as long as the
// CastSender is alive. Therefore the CastSender should be destructed before the
// CastTransport.

#ifndef MEDIA_CAST_NET_CAST_TRANSPORT_H_
#define MEDIA_CAST_NET_CAST_TRANSPORT_H_

#include <stdint.h>

#include <memory>

#include "base/callback.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/tick_clock.h"
#include "base/values.h"
#include "media/cast/logging/logging_defines.h"
#include "media/cast/net/cast_transport_config.h"
#include "media/cast/net/cast_transport_defines.h"
#include "media/cast/net/rtcp/receiver_rtcp_event_subscriber.h"
#include "media/cast/net/rtcp/rtcp_defines.h"
#include "net/base/ip_endpoint.h"

namespace media {
namespace cast {

struct EncodedFrame;
struct RtcpTimeData;

// Following the initialization of either audio or video an initialization
// status will be sent via this callback.
using CastTransportStatusCallback =
    base::RepeatingCallback<void(CastTransportStatus status)>;

// Interface to handle received RTCP messages on RTP sender.
class RtcpObserver {
 public:
  virtual ~RtcpObserver() {}

  // Called on receiving cast message from RTP receiver.
  virtual void OnReceivedCastMessage(const RtcpCastMessage& cast_message) = 0;

  // Called on receiving Rtt message from RTP receiver.
  virtual void OnReceivedRtt(base::TimeDelta round_trip_time) = 0;

  // Called on receiving PLI from RTP receiver.
  virtual void OnReceivedPli() = 0;

  // Called on receiving RTP receiver logs.
  virtual void OnReceivedReceiverLog(const RtcpReceiverLogMessage& log) {}
};

// The application should only trigger this class from the transport thread.
class CastTransport {
 public:
  // Interface used for receiving status updates, raw events, and RTP packets
  // from CastTransport.
  class Client {
   public:
    virtual ~Client() {}

    // Audio and Video transport status change is reported on this callback.
    virtual void OnStatusChanged(CastTransportStatus status) = 0;

    // Raw events will be invoked on this callback periodically, according to
    // the configured logging flush interval passed to
    // CastTransport::Create().
    virtual void OnLoggingEventsReceived(
        std::unique_ptr<std::vector<FrameEvent>> frame_events,
        std::unique_ptr<std::vector<PacketEvent>> packet_events) = 0;

    // Called to pass RTP packets to the Client.
    virtual void ProcessRtpPacket(std::unique_ptr<Packet> packet) = 0;
  };

  static std::unique_ptr<CastTransport> Create(
      const base::TickClock* clock,  // Owned by the caller.
      base::TimeDelta logging_flush_interval,
      std::unique_ptr<Client> client,
      std::unique_ptr<PacketTransport> transport,
      const scoped_refptr<base::SingleThreadTaskRunner>& transport_task_runner);

  virtual ~CastTransport() {}

  // Audio/Video initialization.
  // Encoded frames cannot be transmitted until the relevant initialize method
  // is called.
  virtual void InitializeStream(const CastTransportRtpConfig& config,
                                std::unique_ptr<RtcpObserver> rtcp_observer) {}

  // Encrypt, packetize and transmit |frame|. |ssrc| must refer to a
  // a channel already established with InitializeStream.
  virtual void InsertFrame(uint32_t ssrc, const EncodedFrame& frame) = 0;

  // Sends a RTCP sender report to the receiver.
  // |ssrc| is the SSRC for this report.
  // |current_time| is the current time reported by a tick clock.
  // |current_time_as_rtp_timestamp| is the corresponding RTP timestamp.
  virtual void SendSenderReport(uint32_t ssrc,
                                base::TimeTicks current_time,
                                RtpTimeTicks current_time_as_rtp_timestamp) = 0;

  // Cancels sending packets for the frames in the set.
  // |ssrc| is the SSRC for the stream.
  // |frame_ids| contains the IDs of the frames that will be cancelled.
  virtual void CancelSendingFrames(uint32_t ssrc,
                                   const std::vector<FrameId>& frame_ids) = 0;

  // Resends a frame or part of a frame to kickstart. This is used when the
  // stream appears to be stalled.
  virtual void ResendFrameForKickstart(uint32_t ssrc, FrameId frame_id) = 0;

  // Returns a callback for receiving packets for testing purposes.
  virtual PacketReceiverCallback PacketReceiverForTesting();

  // The following functions are needed for receving.

  // The RTP sender SSRC is used to verify that incoming packets come from the
  // right sender. Without valid SSRCs, the return address cannot be
  // automatically established. The RTP receiver SSRC is used to verify that the
  // request to build the RTCP packet is from the right RTP receiver.
  virtual void AddValidRtpReceiver(uint32_t rtp_sender_ssrc,
                                   uint32_t rtp_receiver_ssrc) = 0;

  // The following function are used to build and send a RTCP packet from
  // RTP receiver to RTP sender.

  // Initialize the RTCP builder on RTP receiver. This has to be called before
  // adding other optional RTCP messages to the packet.
  virtual void InitializeRtpReceiverRtcpBuilder(
      uint32_t rtp_receiver_ssrc,
      const RtcpTimeData& time_data) = 0;

  virtual void AddCastFeedback(const RtcpCastMessage& cast_message,
                               base::TimeDelta target_delay) = 0;
  virtual void AddPli(const RtcpPliMessage& pli_message) = 0;
  virtual void AddRtcpEvents(
      const ReceiverRtcpEventSubscriber::RtcpEvents& rtcp_events) = 0;
  virtual void AddRtpReceiverReport(
      const RtcpReportBlock& rtp_report_block) = 0;

  // Finalize the building of the RTCP packet and send out the built packet.
  virtual void SendRtcpFromRtpReceiver() = 0;

  // Set options for the PacedSender and Wifi.
  virtual void SetOptions(const base::Value::Dict& options) = 0;
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_NET_CAST_TRANSPORT_H_
