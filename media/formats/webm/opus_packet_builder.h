// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_WEBM_OPUS_PACKET_BUILDER_H_
#define MEDIA_FORMATS_WEBM_OPUS_PACKET_BUILDER_H_

#include <stdint.h>

#include <memory>
#include <vector>

namespace media {

// From Opus RFC. See https://tools.ietf.org/html/rfc6716#page-14
enum OpusConstants {
  kNumPossibleOpusConfigs = 32,
  kMinOpusPacketFrameCount = 1,
  kMaxOpusPacketFrameCount = 48
};

class OpusPacket {
 public:
  OpusPacket(uint8_t config, uint8_t frame_count, bool is_VBR);

  OpusPacket(const OpusPacket&) = delete;
  OpusPacket& operator=(const OpusPacket&) = delete;

  ~OpusPacket();

  const uint8_t* data() const;
  int size() const;
  double duration_ms() const;

 private:
  std::vector<uint8_t> data_;
  double duration_ms_;
};

// Builds an exhaustive collection of Opus packet configurations.
std::vector<std::unique_ptr<OpusPacket>> BuildAllOpusPackets();

}  // namespace media

#endif  // MEDIA_FORMATS_WEBM_OPUS_PACKET_BUILDER_H_
