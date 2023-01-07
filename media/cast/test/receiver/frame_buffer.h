// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_TEST_RECEIVER_FRAME_BUFFER_H_
#define MEDIA_CAST_TEST_RECEIVER_FRAME_BUFFER_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <vector>

#include "media/cast/common/rtp_time.h"
#include "media/cast/net/cast_transport_config.h"
#include "media/cast/net/rtp/rtp_defines.h"

namespace media {
namespace cast {

struct EncodedFrame;

typedef std::map<uint16_t, std::vector<uint8_t>> PacketMap;

class FrameBuffer {
 public:
  FrameBuffer();

  FrameBuffer(const FrameBuffer&) = delete;
  FrameBuffer& operator=(const FrameBuffer&) = delete;

  ~FrameBuffer();
  bool InsertPacket(const uint8_t* payload_data,
                    size_t payload_size,
                    const RtpCastHeader& rtp_header);
  bool Complete() const;

  void GetMissingPackets(bool newest_frame, PacketIdSet* missing_packets) const;

  // If a frame is complete, sets the frame IDs and RTP timestamp in |frame|,
  // and also copies the data from all packets into the data field in |frame|.
  // Returns true if the frame was complete; false if incomplete and |frame|
  // remains unchanged.
  bool AssembleEncodedFrame(EncodedFrame* frame) const;

  bool is_key_frame() const { return is_key_frame_; }
  FrameId last_referenced_frame_id() const { return last_referenced_frame_id_; }
  FrameId frame_id() const { return frame_id_; }

 private:
  FrameId frame_id_;
  uint16_t max_packet_id_;
  uint16_t num_packets_received_;
  uint16_t max_seen_packet_id_;
  uint16_t new_playout_delay_ms_;
  bool is_key_frame_;
  size_t total_data_size_;
  FrameId last_referenced_frame_id_;
  RtpTimeTicks rtp_timestamp_;
  PacketMap packets_;
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_TEST_RECEIVER_FRAME_BUFFER_H_
