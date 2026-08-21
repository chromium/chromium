// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_COMMON_OPUS_CONSTANTS_H_
#define MEDIA_FORMATS_COMMON_OPUS_CONSTANTS_H_

#include <stddef.h>
#include <stdint.h>

#include <array>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"

namespace media {

// The Opus specification is part of IETF RFC 6716:
// http://tools.ietf.org/html/rfc6716

// Opus Extra Data contents:
// - "OpusHead" magic signature (64 bits)
// - version number (8 bits)
// - Channels C (8 bits)
// - Pre-skip (16 bits)
// - Sampling rate (32 bits)
// - Gain in dB (16 bits, S7.8)
// - Mapping (8 bits, 0=single stream (mono/stereo) 1=Vorbis mapping,
//            2..254: reserved, 255: multistream with no mapping)
//
// - if (mapping != 0)
//    - N = total number of streams (8 bits)
//    - M = number of paired streams (8 bits)
//    - C times channel origin
//         - if (C<2*M)
//            - stream = byte/2
//            - if (byte&0x1 == 0)
//                - left
//              else
//                - right
//         - else
//            - stream = byte-M

enum : uint8_t {
  // Default audio output channel layout. Used to initialize |stream_map| in
  // OpusExtraData, and passed to opus_multistream_decoder_create() when the
  // extra data does not contain mapping information. The values are valid only
  // for mono and stereo output: Opus streams with more than 2 channels require
  // a stream map.
  OPUS_MAX_CHANNELS_WITH_DEFAULT_LAYOUT = 2,

  // Opus uses Vorbis channel mapping, and Vorbis channel mapping specifies
  // mappings for up to 8 channels. This information is part of the Vorbis I
  // Specification:
  // http://www.xiph.org/vorbis/doc/Vorbis_I_spec.html
  OPUS_MAX_VORBIS_CHANNELS = 8,

  // Opus Channel Mapping Families (RFC 7845 / RFC 8486).
  OPUS_CHANNEL_MAPPING_FAMILY_DEFAULT = 0,
  OPUS_CHANNEL_MAPPING_FAMILY_VORBIS = 1,
  OPUS_CHANNEL_MAPPING_FAMILY_AMBISONICS = 2,
  OPUS_CHANNEL_MAPPING_FAMILY_UNDEFINED = 255,

  // Size of the Opus extra data excluding optional mapping information.
  OPUS_EXTRADATA_SIZE = 19,
  // Offset for magic signature "OpusHead"
  OPUS_EXTRADATA_LABEL_OFFSET = 0,
  // Offset to the Opus version number
  OPUS_EXTRADATA_VERSION_OFFSET = 8,
  // Offset to the channel count byte in the Opus extra data
  OPUS_EXTRADATA_CHANNELS_OFFSET = 9,
  // Offset to the pre-skip value in the Opus extra data
  OPUS_EXTRADATA_SKIP_SAMPLES_OFFSET = 10,
  // Offset to the sampling rate value in the Opus extra data
  OPUS_EXTRADATA_SAMPLE_RATE_OFFSET = 12,
  // Offset to the gain value in the Opus extra data
  OPUS_EXTRADATA_GAIN_OFFSET = 16,
  // Offset to the channel mapping byte in the Opus extra data
  OPUS_EXTRADATA_CHANNEL_MAPPING_OFFSET = 18,

  // Extra Data contains a stream map, beyond the always present
  // |OPUS_EXTRADATA_SIZE| bytes of data. The mapping data contains stream
  // count, coupling information, and per channel mapping values:
  //   - Byte 0: Number of streams.
  //   - Byte 1: Number coupled.
  //   - Byte 2: Starting at byte 2 are |extra_data->channels| uint8_t mapping
  //             values.
  OPUS_EXTRADATA_NUM_STREAMS_OFFSET = OPUS_EXTRADATA_SIZE,
  OPUS_EXTRADATA_NUM_COUPLED_OFFSET = OPUS_EXTRADATA_NUM_STREAMS_OFFSET + 1,
  OPUS_EXTRADATA_STREAM_MAP_OFFSET = OPUS_EXTRADATA_NUM_STREAMS_OFFSET + 2,
};

// Returns the Vorbis-to-Chromium channel layout offset mapping (RFC 7845
// Section 5.1.1.2) for a stream with `channels` (1 to 8), matching FFmpeg's
// ff_vorbis_channel_layout_offsets.
//
// Used by OpusAudioDecoder to remap the Vorbis channel mapping table into
// Chromium's expected channel order for libopus multistream decoding.
//
// Slices the exact span of length `channels`.
ALWAYS_INLINE constexpr base::span<const uint8_t>
GetVorbisToChromiumChannelLayoutOffsets(size_t channels) {
  CHECK_GE(channels, 1u);
  CHECK_LE(channels, static_cast<size_t>(OPUS_MAX_VORBIS_CHANNELS));

  // Vorbis channel order per RFC 7845 Section 5.1.1.2:
  //   1ch: FC(0)
  //   2ch: FL(0), FR(1)
  //   3ch: FL(0), FC(1), FR(2)
  //   4ch: FL(0), FR(1), BL(2), BR(3)
  //   5ch: FL(0), FC(1), FR(2), BL(3), BR(4)
  //   6ch: FL(0), FC(1), FR(2), BL(3), BR(4), LFE(5)
  //   7ch: FL(0), FC(1), FR(2), SL(3), SR(4), BC(5), LFE(6)
  //   8ch: FL(0), FC(1), FR(2), SL(3), SR(4), BL(5), BR(6), LFE(7)
  //
  // Values below map Chromium channel order to Vorbis channel indices:
  static constexpr std::array<std::array<uint8_t, OPUS_MAX_VORBIS_CHANNELS>,
                              OPUS_MAX_VORBIS_CHANNELS>
      kOffsets = {{
          // 1ch (Mono):   FC
          {0},
          // 2ch (Stereo): FL, FR
          {0, 1},
          // 3ch (3.0):    FL, FR, FC
          {0, 2, 1},
          // 4ch (Quad):   FL, FR, BL, BR
          {0, 1, 2, 3},
          // 5ch (5.0):    FL, FR, FC, BL, BR
          {0, 2, 1, 3, 4},
          // 6ch (5.1):    FL, FR, FC, LFE, BL, BR
          {0, 2, 1, 5, 3, 4},
          // 7ch (6.1):    FL, FR, FC, LFE, BC, SL, SR
          {0, 2, 1, 6, 5, 3, 4},
          // 8ch (7.1):    FL, FR, FC, LFE, BL, BR, SL, SR
          {0, 2, 1, 7, 5, 6, 3, 4},
      }};

  return base::span(kOffsets[channels - 1]).first(channels);
}

}  // namespace media

#endif  // MEDIA_FORMATS_COMMON_OPUS_CONSTANTS_H_
