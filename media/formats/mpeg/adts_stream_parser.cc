// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/formats/mpeg/adts_stream_parser.h"

#include <stddef.h>

#include "build/build_config.h"
#include "media/base/media_log.h"
#include "media/formats/mp4/aac.h"
#include "media/formats/mpeg/adts_constants.h"

namespace media {

constexpr uint32_t kADTSStartCodeMask = 0xfff00000;

ADTSStreamParser::ADTSStreamParser()
    : MPEGAudioStreamParserBase(kADTSStartCodeMask, AudioCodec::kAAC, 0) {}

ADTSStreamParser::~ADTSStreamParser() = default;

int ADTSStreamParser::ParseFrameHeader(const uint8_t* data,
                                       int size,
                                       int* frame_size,
                                       int* sample_rate,
                                       ChannelLayout* channel_layout,
                                       int* sample_count,
                                       bool* metadata_frame,
                                       std::vector<uint8_t>* extra_data) {
  DCHECK(data);
  DCHECK_GE(size, 0);

  if (size < kADTSHeaderMinSize)
    return 0;

  BitReader reader(data, size);
  int sync;
  int version;
  int layer;
  int protection_absent;
  int profile;
  size_t sample_rate_index;
  size_t channel_layout_index;
  int frame_length;
  size_t num_data_blocks;
  int unused;

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
    return -1;
  }

  DVLOG(2) << "Header data :" << std::hex << " sync 0x" << sync << " version 0x"
           << version << " layer 0x" << layer << " profile 0x" << profile
           << " sample_rate_index 0x" << sample_rate_index
           << " channel_layout_index 0x" << channel_layout_index;

  const int bytes_read = reader.bits_read() / 8;
  if (sync != 0xfff || layer != 0 || frame_length < bytes_read ||
      sample_rate_index >= kADTSFrequencyTableSize ||
      channel_layout_index >= kADTSChannelLayoutTableSize) {
    if (media_log()) {
      LIMITED_MEDIA_LOG(DEBUG, media_log(), adts_parse_error_limit_, 5)
          << "Invalid header data :" << std::hex << " sync 0x" << sync
          << " version 0x" << version << " layer 0x" << layer
          << " sample_rate_index 0x" << sample_rate_index
          << " channel_layout_index 0x" << channel_layout_index;
    }
    return -1;
  }

  if (sample_rate)
    *sample_rate = kADTSFrequencyTable[sample_rate_index];

  if (frame_size)
    *frame_size = frame_length;

  if (sample_count)
    *sample_count = (num_data_blocks + 1) * kSamplesPerAACFrame;

  if (channel_layout)
    *channel_layout = kADTSChannelLayoutTable[channel_layout_index];

  if (metadata_frame)
    *metadata_frame = false;

  if (extra_data) {
    // See mp4::AAC::Parse() for details. We don't need to worry about writing
    // extensions since we can't have extended ADTS by this point (it's
    // explicitly rejected as invalid above).
    DCHECK_NE(sample_rate_index, 15u);

    // The following code is written according to ISO 14496 Part 3 Table 1.13 -
    // Syntax of AudioSpecificConfig.
    const uint16_t esds = (((((profile + 1) << 4) + sample_rate_index) << 4) +
                           channel_layout_index)
                          << 3;
    extra_data->push_back(esds >> 8);
    extra_data->push_back(esds & 0xFF);
  }

  return bytes_read;
}

}  // namespace media
