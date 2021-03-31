// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the interface to the cast RTP sender.

#ifndef MEDIA_CAST_NET_RTP_RTP_SENDER_H_
#define MEDIA_CAST_NET_RTP_RTP_SENDER_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <set>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "media/cast/cast_environment.h"
#include "media/cast/net/cast_transport.h"
#include "media/cast/net/cast_transport_defines.h"
#include "media/cast/net/pacing/paced_sender.h"
#include "media/cast/net/rtp/packet_storage.h"
#include "media/cast/net/rtp/rtp_packetizer.h"

namespace media {
namespace cast {

// This object is only called from the main cast thread.
// This class handles splitting encoded audio and video frames into packets and
// add an RTP header to each packet. The sent packets are stored until they are
// acknowledged by the remote peer or timed out.
class RtpSender {
 public:
  RtpSender(
      const scoped_refptr<base::SingleThreadTaskRunner>& transport_task_runner,
      PacedSender* const transport);

  ~RtpSender();

  // This must be called before sending any frames. Returns false if
  // configuration is invalid.
  bool Initialize(const CastTransportRtpConfig& config);

  void SendFrame(const EncodedFrame& frame);

  void ResendPackets(const MissingFramesAndPacketsMap& missing_packets,
                     bool cancel_rtx_if_not_in_list,
                     const DedupInfo& dedup_info);

  // Returns the total number of bytes sent to the socket when the specified
  // frame was just sent.
  // Returns 0 if the frame cannot be found or the frame was only sent
  // partially.
  int64_t GetLastByteSentForFrame(FrameId frame_id);

  void CancelSendingFrames(const std::vector<FrameId>& frame_ids);

  void ResendFrameForKickstart(FrameId frame_id, base::TimeDelta dedupe_window);

  size_t send_packet_count() const {
    return packetizer_ ? packetizer_->send_packet_count() : 0;
  }
  size_t send_octet_count() const {
    return packetizer_ ? packetizer_->send_octet_count() : 0;
  }
  uint32_t ssrc() const { return config_.ssrc; }

 private:
  void UpdateSequenceNumber(Packet* packet);

  RtpPacketizerConfig config_;
  PacketStorage storage_;
  std::unique_ptr<RtpPacketizer> packetizer_;
  PacedSender* const transport_;
  scoped_refptr<base::SingleThreadTaskRunner> transport_task_runner_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<RtpSender> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(RtpSender);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_NET_RTP_RTP_SENDER_H_
