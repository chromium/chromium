// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_RECEIVER_FRAME_RECEIVER_H_
#define MEDIA_CAST_RECEIVER_FRAME_RECEIVER_H_

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "media/cast/cast_receiver.h"
#include "media/cast/common/clock_drift_smoother.h"
#include "media/cast/common/rtp_time.h"
#include "media/cast/common/transport_encryption_handler.h"
#include "media/cast/logging/logging_defines.h"
#include "media/cast/net/rtcp/receiver_rtcp_event_subscriber.h"
#include "media/cast/net/rtcp/receiver_rtcp_session.h"
#include "media/cast/net/rtp/framer.h"
#include "media/cast/net/rtp/receiver_stats.h"
#include "media/cast/net/rtp/rtp_defines.h"
#include "media/cast/net/rtp/rtp_parser.h"

namespace media {
namespace cast {

class CastEnvironment;

// FrameReceiver receives packets out-of-order while clients make requests for
// complete frames in-order.  (A frame consists of one or more packets.)
//
// FrameReceiver also includes logic for computing the playout time for each
// frame, accounting for a constant targeted playout delay.  The purpose of the
// playout delay is to provide a fixed window of time between the capture event
// on the sender and the playout on the receiver.  This is important because
// each step of the pipeline (i.e., encode frame, then transmit/retransmit from
// the sender, then receive and re-order packets on the receiver, then decode
// frame) can vary in duration and is typically very hard to predict.
//
// Each request for a frame includes a callback which FrameReceiver guarantees
// will be called at some point in the future unless the FrameReceiver is
// destroyed.  Clients should generally limit the number of outstanding requests
// (perhaps to just one or two).
//
// This class is not thread safe.  Should only be called from the Main cast
// thread.
class FrameReceiver : public RtpPayloadFeedback {
 public:
  FrameReceiver(const scoped_refptr<CastEnvironment>& cast_environment,
                const FrameReceiverConfig& config,
                EventMediaType event_media_type,
                CastTransport* const transport);

  ~FrameReceiver() final;

  // Request an encoded frame.
  //
  // The given |callback| is guaranteed to be run at some point in the future,
  // except for those requests still enqueued at destruction time.
  void RequestEncodedFrame(const ReceiveEncodedFrameCallback& callback);

  // Called to deliver another packet, possibly a duplicate, and possibly
  // out-of-order.  Returns true if the parsing of the packet succeeded.
  bool ProcessPacket(std::unique_ptr<Packet> packet);

  base::WeakPtr<FrameReceiver> AsWeakPtr();

 protected:
  friend class FrameReceiverTest;  // Invokes ProcessParsedPacket().

  void ProcessParsedPacket(const RtpCastHeader& rtp_header,
                           const uint8_t* payload_data,
                           size_t payload_size);

  // RtpPayloadFeedback implementation.
  void CastFeedback(const RtcpCastMessage& cast_message) final;

 private:
  // Processes ready-to-consume packets from |framer_|, decrypting each packet's
  // payload data, and then running the enqueued callbacks in order (one for
  // each packet).  This method may post a delayed task to re-invoke itself in
  // the future to wait for missing/incomplete frames.
  void EmitAvailableEncodedFrames();

  // Clears the |is_waiting_for_consecutive_frame_| flag and invokes
  // EmitAvailableEncodedFrames().
  void EmitAvailableEncodedFramesAfterWaiting();

  // Helper that runs |callback|, passing ownership of |encoded_frame| to it.
  // This method is used by EmitAvailableEncodedFrames() to return to the event
  // loop, but make sure that FrameReceiver is still alive before the callback
  // is run.
  void EmitOneFrame(const ReceiveEncodedFrameCallback& callback,
                    std::unique_ptr<EncodedFrame> encoded_frame) const;

  // Computes the playout time for a frame with the given |rtp_timestamp|.
  // Because lip-sync info is refreshed regularly, calling this method with the
  // same argument may return different results.
  base::TimeTicks GetPlayoutTime(const EncodedFrame& frame) const;

  // Schedule timing for the next cast message.
  void ScheduleNextCastMessage();

  // Schedule timing for the next RTCP report.
  void ScheduleNextRtcpReport();

  // Actually send the next cast message.
  void SendNextCastMessage();

  // Actually send the next RTCP report.
  void SendNextRtcpReport();

  // Interface to send RTCP reports.
  // |cast_message|, |rtcp_events| and |rtp_receiver_statistics| are optional;
  // if |cast_message| is provided the RTCP receiver report will contain a Cast
  // ACK/NACK feedback message; |target_delay| is sent together with
  // |cast_message|. If |rtcp_events| is provided the RTCP receiver report will
  // include event log messages
  void SendRtcpReport(
      uint32_t rtp_receiver_ssrc,
      uint32_t rtp_sender_ssrc,
      const RtcpTimeData& time_data,
      const RtcpCastMessage* cast_message,
      const RtcpPliMessage* pli_message,
      base::TimeDelta target_delay,
      const ReceiverRtcpEventSubscriber::RtcpEvents* rtcp_events,
      const RtpReceiverStatistics* rtp_receiver_statistics);

  const scoped_refptr<CastEnvironment> cast_environment_;

  // Transport used to send data back.
  CastTransport* const transport_;

  // Deserializes a packet into a RtpHeader + payload bytes.
  RtpParser packet_parser_;

  // Accumulates packet statistics, including packet loss, counts, and jitter.
  ReceiverStats stats_;

  // Partitions logged events by the type of media passing through.
  EventMediaType event_media_type_;

  // Subscribes to raw events.
  // Processes raw events to be sent over to the cast sender via RTCP.
  ReceiverRtcpEventSubscriber event_subscriber_;

  // RTP timebase: The number of RTP units advanced per one second.
  const int rtp_timebase_;

  // The total amount of time between a frame's capture/recording on the sender
  // and its playback on the receiver (i.e., shown to a user).  This is fixed as
  // a value large enough to give the system sufficient time to encode,
  // transmit/retransmit, receive, decode, and render; given its run-time
  // environment (sender/receiver hardware performance, network conditions,
  // etc.).
  base::TimeDelta target_playout_delay_;

  // Hack: This is used in logic that determines whether to skip frames.
  // TODO(miu): Revisit this.  Logic needs to also account for expected decode
  // time.
  const base::TimeDelta expected_frame_duration_;

  // Set to false initially, then set to true after scheduling the periodic
  // sending of reports back to the sender.  Reports are first scheduled just
  // after receiving a first packet (since the first packet identifies the
  // sender for the remainder of the session).
  bool reports_are_scheduled_;

  // Assembles packets into frames, providing this receiver with complete,
  // decodable EncodedFrames.
  Framer framer_;

  // Manages sending/receiving of RTCP packets, including sender/receiver
  // reports.
  ReceiverRtcpSession rtcp_;

  // Decrypts encrypted frames.
  TransportEncryptionHandler decryptor_;

  // Outstanding callbacks to run to deliver on client requests for frames.
  std::list<ReceiveEncodedFrameCallback> frame_request_queue_;

  // True while there's an outstanding task to re-invoke
  // EmitAvailableEncodedFrames().
  bool is_waiting_for_consecutive_frame_;

  // This mapping allows us to log FRAME_ACK_SENT as a frame event. In addition
  // it allows the event to be transmitted via RTCP.  The index into this ring
  // buffer is the lower 8 bits of the FrameId.
  RtpTimeTicks frame_id_to_rtp_timestamp_[256];

  // Lip-sync values used to compute the playout time of each frame from its RTP
  // timestamp.  These are updated each time the first packet of a frame is
  // received.
  RtpTimeTicks lip_sync_rtp_timestamp_;
  base::TimeTicks lip_sync_reference_time_;
  ClockDriftSmoother lip_sync_drift_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<FrameReceiver> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FrameReceiver);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_RECEIVER_FRAME_RECEIVER_H_
