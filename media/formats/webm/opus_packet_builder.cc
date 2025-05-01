// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/formats/webm/opus_packet_builder.h"

#include "base/check_op.h"
#include "media/formats/webm/webm_cluster_parser.h"

namespace media {

OpusPacket::OpusPacket(uint8_t config, uint8_t frame_count, bool is_VBR) {
  DCHECK_GE(config, 0);
  DCHECK_LT(config, kNumPossibleOpusConfigs);
  DCHECK_GE(frame_count, kMinOpusPacketFrameCount);
  DCHECK_LE(frame_count, kMaxOpusPacketFrameCount);

  duration_ms_ = frame_count *
                 WebMClusterParser::kOpusFrameDurationsMu[config] /
                 static_cast<float>(1000);

  uint8_t frame_count_code;
  uint8_t frame_count_byte;

  if (frame_count == 1) {
    frame_count_code = 0;
  } else if (frame_count == 2) {
    frame_count_code = is_VBR ? 2 : 1;
  } else {
    frame_count_code = 3;
    frame_count_byte = (is_VBR ? 1 << 7 : 0) | frame_count;
  }

  // All opus packets must have TOC byte.
  uint8_t opus_toc_byte = (config << 3) | frame_count_code;
  data_.push_back(opus_toc_byte);

  // For code 3 packets, the number of frames is signaled in the "frame
  // count byte".
  if (frame_count_code == 3) {
    data_.push_back(frame_count_byte);
  }

  // Packet will only conform to layout specification for the TOC byte
  // and optional frame count bytes appended above. This last byte
  // is purely dummy padding where frame size data or encoded data might
  // otherwise start.
  data_.push_back(static_cast<uint8_t>(0));
}

OpusPacket::~OpusPacket() = default;

const uint8_t* OpusPacket::data() const {
  return &(data_[0]);
}

int OpusPacket::size() const {
  return data_.size();
}

double OpusPacket::duration_ms() const {
  return duration_ms_;
}

std::vector<std::unique_ptr<OpusPacket>> BuildAllOpusPackets() {
  std::vector<std::unique_ptr<OpusPacket>> opus_packets;

  for (int frame_count = kMinOpusPacketFrameCount;
       frame_count <= kMaxOpusPacketFrameCount; frame_count++) {
    for (int opus_config_num = 0; opus_config_num < kNumPossibleOpusConfigs;
         opus_config_num++) {
      bool is_VBR = false;
      opus_packets.push_back(
          std::make_unique<OpusPacket>(opus_config_num, frame_count, is_VBR));

      if (frame_count >= 2) {
        // Add another packet with VBR flag toggled. For frame counts >= 2,
        // VBR triggers changes to packet framing.
        is_VBR = true;
        opus_packets.push_back(
            std::make_unique<OpusPacket>(opus_config_num, frame_count, is_VBR));
      }
    }
  }

  return opus_packets;
}

}  // namespace media
