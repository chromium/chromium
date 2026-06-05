// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/mpeg/adts_stream_parser.h"

#include <stddef.h>

#include "build/build_config.h"
#include "media/base/media_log.h"
#include "media/formats/mp4/aac.h"
#include "media/formats/mpeg/adts_constants.h"

namespace media {

// static
std::optional<ADTSStreamParser::Header> ADTSStreamParser::ParseHeader(
    base::span<const uint8_t> data) {
  if (data.size() < kADTSHeaderMinSize) {
    return std::nullopt;
  }

  BitReader reader(data);
  uint16_t sync;
  uint8_t version;
  uint8_t layer;
  uint8_t protection_absent;
  uint8_t profile;
  size_t sample_rate_index;
  size_t channel_layout_index;
  size_t frame_length;
  size_t num_data_blocks;
  uint16_t unused;

  if (!reader.ReadBits(12, &sync) ||
      !reader.ReadBits(1, &version) ||
      !reader.ReadBits(2, &layer) ||
      !reader.ReadBits(1, &protection_absent) ||
      !reader.ReadBits(2, &profile) ||
      !reader.ReadBits(4, &sample_rate_index) ||
      !reader.ReadBits(1, &unused) ||
      !reader.ReadBits(3, &channel_layout_index) ||
      !reader.ReadBits(4, &unused) ||
      !reader.ReadBits(13, &frame_length) ||
      !reader.ReadBits(11, &unused) ||
      !reader.ReadBits(2, &num_data_blocks) ||
      (!protection_absent && !reader.ReadBits(16, &unused))) {
    return std::nullopt;
  }

  const size_t bytes_read = reader.bits_read() / 8;
  if (sync != 0xfff || layer != 0 || frame_length < bytes_read ||
      sample_rate_index >= kADTSFrequencyTable.size() ||
      channel_layout_index >= kADTSChannelLayoutTable.size()) {
    return std::nullopt;
  }

  Header header;
  header.frame_size = frame_length;
  header.sample_rate = kADTSFrequencyTable[sample_rate_index];
  header.channel_layout = kADTSChannelLayoutTable[channel_layout_index];
  header.sample_count = (num_data_blocks + 1) * kSamplesPerAACFrame;

  DCHECK_NE(sample_rate_index, 15u);
  const uint16_t esds =
      (((((profile + 1) << 4) + sample_rate_index) << 4) + channel_layout_index)
      << 3;
  header.extra_data.push_back(esds >> 8);
  header.extra_data.push_back(esds & 0xFF);

  return header;
}

constexpr uint32_t kADTSStartCodeMask = 0xfff00000;

ADTSStreamParser::ADTSStreamParser()
    : MPEGAudioStreamParserBase(kADTSStartCodeMask, AudioCodec::kAAC, 0) {}

ADTSStreamParser::~ADTSStreamParser() = default;

size_t ADTSStreamParser::GetMinHeaderSize() const {
  return kADTSHeaderMinSize;
}

std::optional<ADTSStreamParser::Header> ADTSStreamParser::ParseFrameHeader(
    base::span<const uint8_t> data) {
  auto header = ParseHeader(data);
  if (!header) {
    LIMITED_MEDIA_LOG(DEBUG, media_log(), adts_parse_error_limit_, 5)
        << "Invalid ADTS header.";
  }
  return header;
}

}  // namespace media
