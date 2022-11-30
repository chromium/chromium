// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_COMMON_OPUS_CONSTANTS_H_
#define MEDIA_FORMATS_COMMON_OPUS_CONSTANTS_H_

#include <stdint.h>

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

enum {
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

// Opus internal to Vorbis channel order mapping written in the header.
extern const uint8_t kOpusVorbisChannelMap[OPUS_MAX_VORBIS_CHANNELS]
                                          [OPUS_MAX_VORBIS_CHANNELS];

}  // namespace media

#endif  // MEDIA_FORMATS_COMMON_OPUS_CONSTANTS_H_
