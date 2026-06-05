// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/mpeg/mpeg1_audio_stream_parser.h"

#include <array>

#include "base/compiler_specific.h"
#include "media/base/media_log.h"

namespace media {

namespace {

// Versions and layers as defined in ISO/IEC 11172-3.
enum Version : uint8_t {
  kVersion1 = 3,
  kVersion2 = 2,
  kVersionReserved = 1,
  kVersion2_5 = 0,
};

enum Layer : uint8_t {
  kLayer1 = 3,
  kLayer2 = 2,
  kLayer3 = 1,
  kLayerReserved = 0,
};

constexpr uint32_t kMPEG1StartCodeMask = 0xffe00000;

// Maps version and layer information in the frame header
// into an index for the |kBitrateMap|.
// Derived from: http://mpgedit.org/mpgedit/mpeg_format/MP3Format.html
constexpr std::array<std::array<int, 4>, 4> kVersionLayerMap = {{
    // { reserved, L3, L2, L1 }
    {5, 4, 4, 3},  // MPEG 2.5
    {5, 5, 5, 5},  // reserved
    {5, 4, 4, 3},  // MPEG 2
    {5, 2, 1, 0},  // MPEG 1
}};

// Maps the bitrate index field in the header and an index
// from |kVersionLayerMap| to a frame bitrate.
// Derived from: http://mpgedit.org/mpgedit/mpeg_format/MP3Format.html
constexpr std::array<std::array<int, 6>, 16> kBitrateMap = {{
    // { V1L1, V1L2, V1L3, V2L1, V2L2 & V2L3, reserved }
    {0, 0, 0, 0, 0, 0},
    {32, 32, 32, 32, 8, 0},
    {64, 48, 40, 48, 16, 0},
    {96, 56, 48, 56, 24, 0},
    {128, 64, 56, 64, 32, 0},
    {160, 80, 64, 80, 40, 0},
    {192, 96, 80, 96, 48, 0},
    {224, 112, 96, 112, 56, 0},
    {256, 128, 112, 128, 64, 0},
    {288, 160, 128, 144, 80, 0},
    {320, 192, 160, 160, 96, 0},
    {352, 224, 192, 176, 112, 0},
    {384, 256, 224, 192, 128, 0},
    {416, 320, 256, 224, 144, 0},
    {448, 384, 320, 256, 160, 0},
    {0, 0, 0, 0, 0},
}};

// Maps the sample rate index and version fields from the frame header
// to a sample rate.
// Derived from: http://mpgedit.org/mpgedit/mpeg_format/MP3Format.html
constexpr std::array<std::array<int, 4>, 4> kSampleRateMap = {{
    // { V2.5, reserved, V2, V1 }
    {11025, 0, 22050, 44100},
    {12000, 0, 24000, 48000},
    {8000, 0, 16000, 32000},
    {0, 0, 0, 0},
}};

// Offset in bytes from the end of the MP3 header to "Xing" or "Info" tags which
// indicate a frame is silent metadata frame.  Values taken from FFmpeg.
constexpr std::array<std::array<int, 2>, 2> kXingHeaderMap = {{
    {32, 17},
    {17, 9},
}};

// Frame header field constants.
constexpr int kBitrateFree = 0;
constexpr int kBitrateBad = 0xf;
constexpr int kSampleRateReserved = 3;
constexpr int kCodecDelay = 529;

}  // namespace

// static
std::optional<MPEG1AudioStreamParser::Header>
MPEG1AudioStreamParser::ParseHeader(base::span<const uint8_t> data) {
  BitReader reader(data.first<kHeaderSize>());
  uint16_t sync;
  uint8_t version;
  uint8_t layer;
  uint8_t is_protected;
  uint8_t bitrate_index;
  uint8_t sample_rate_index;
  uint8_t has_padding;
  uint8_t is_private;
  uint8_t channel_mode;
  uint8_t other_flags;

  if (!reader.ReadBits(11, &sync) || !reader.ReadBits(2, &version) ||
      !reader.ReadBits(2, &layer) || !reader.ReadBits(1, &is_protected) ||
      !reader.ReadBits(4, &bitrate_index) ||
      !reader.ReadBits(2, &sample_rate_index) ||
      !reader.ReadBits(1, &has_padding) || !reader.ReadBits(1, &is_private) ||
      !reader.ReadBits(2, &channel_mode) || !reader.ReadBits(6, &other_flags)) {
    return std::nullopt;
  }

  DVLOG(2) << "Header data :" << std::hex << " sync 0x" << sync << " version 0x"
           << version << " layer 0x" << layer << " bitrate_index 0x"
           << bitrate_index << " sample_rate_index 0x" << sample_rate_index
           << " channel_mode 0x" << channel_mode;

  if (sync != 0x7ff || version == kVersionReserved || layer == kLayerReserved ||
      bitrate_index == kBitrateFree || bitrate_index == kBitrateBad ||
      sample_rate_index == kSampleRateReserved) {
    return std::nullopt;
  }

  // Note: For MPEG2 we don't check if a given bitrate or channel layout is
  // allowed per spec since all tested decoders don't seem to care.

  int bitrate = kBitrateMap[bitrate_index][kVersionLayerMap[version][layer]];
  if (bitrate == 0) {
    return std::nullopt;
  }

  int frame_sample_rate = kSampleRateMap[sample_rate_index][version];
  if (frame_sample_rate == 0) {
    return std::nullopt;
  }

  // http://teslabs.com/openplayer/docs/docs/specs/mp3_structure2.pdf
  // Table 2.1.5
  int samples_per_frame;
  switch (layer) {
    case kLayer1:
      samples_per_frame = 384;
      break;

    case kLayer2:
      samples_per_frame = 1152;
      break;

    case kLayer3:
      if (version == kVersion2 || version == kVersion2_5) {
        samples_per_frame = 576;
      } else {
        samples_per_frame = 1152;
      }
      break;

    default:
      return std::nullopt;
  }

  Header header;
  header.sample_rate = frame_sample_rate;
  header.sample_count = samples_per_frame;

  // http://teslabs.com/openplayer/docs/docs/specs/mp3_structure2.pdf
  // Text just below Table 2.1.5.
  if (layer == kLayer1) {
    // This formulation is a slight variation on the equation below,
    // but has slightly different truncation characteristics to deal
    // with the fact that Layer 1 has 4 byte "slots" instead of single
    // byte ones.
    header.frame_size = 4 * (12 * bitrate * 1000 / frame_sample_rate);
  } else {
    header.frame_size =
        ((samples_per_frame / 8) * bitrate * 1000) / frame_sample_rate;
  }

  if (has_padding) {
    header.frame_size += (layer == kLayer1) ? 4 : 1;
  }

  // Map Stereo(0), Joint Stereo(1), and Dual Channel (2) to
  // CHANNEL_LAYOUT_STEREO and Single Channel (3) to CHANNEL_LAYOUT_MONO.
  header.channel_layout =
      (channel_mode == 3) ? CHANNEL_LAYOUT_MONO : CHANNEL_LAYOUT_STEREO;

  // XING header detection (MPEG-1 Layer 3 only)
  if (layer == kLayer3) {
    const int xing_header_index =
        kXingHeaderMap[version == kVersion2 || version == kVersion2_5]
                      [channel_mode == 3];
    const size_t tag_size = 4;
    const size_t needed_data = kHeaderSize + xing_header_index + tag_size;

    if (data.size() >= needed_data) {
      BitReader xing_reader(data.subspan(kHeaderSize));
      uint32_t tag = 0;
      if (xing_reader.SkipBits(xing_header_index * 8) &&
          xing_reader.ReadBits(tag_size * 8, &tag)) {
        if (tag == 0x496e666f || tag == 0x58696e67) {
          header.metadata_frame = true;
        }
      }
    }
  }

  return header;
}

MPEG1AudioStreamParser::MPEG1AudioStreamParser()
    : MPEGAudioStreamParserBase(kMPEG1StartCodeMask,
                                AudioCodec::kMP3,
                                kCodecDelay) {}

MPEG1AudioStreamParser::~MPEG1AudioStreamParser() = default;

size_t MPEG1AudioStreamParser::GetMinHeaderSize() const {
  return kHeaderSize;
}

std::optional<MPEG1AudioStreamParser::Header>
MPEG1AudioStreamParser::ParseFrameHeader(base::span<const uint8_t> data) {
  auto header = ParseHeader(data);
  if (!header) {
    LIMITED_MEDIA_LOG(DEBUG, media_log(), mp3_parse_error_limit_, 5)
        << "Invalid MP3 header.";
  }
  return header;
}

}  // namespace media
