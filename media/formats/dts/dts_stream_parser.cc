// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/dts/dts_stream_parser.h"

#include <stddef.h>

#include "build/build_config.h"
#include "media/base/media_log.h"

namespace media {

DTSStreamParser::DTSStreamParser()
    : MPEGAudioStreamParserBase(kDTSCoreSyncWord, AudioCodec::kDTS, 0) {}

DTSStreamParser::~DTSStreamParser() = default;

int DTSStreamParser::ParseFrameHeader(const uint8_t* data,
                                      int size,
                                      int* frame_size,
                                      int* sample_rate,
                                      ChannelLayout* channel_layout,
                                      int* sample_count,
                                      bool* metadata_frame,
                                      std::vector<uint8_t>* extra_data) {
  if (data == nullptr || size < kDTSCoreHeaderSizeInBytes)
    return 0;

  BitReader reader(data, size);

  // Read and validate Sync word.
  uint32_t sync_word = 0;
  reader.ReadBits(32, &sync_word);
  if (sync_word != kDTSCoreSyncWord)
    return 0;

  int fsize = 0, ext_audio = 0, ext_audio_id = 0, nblks = 0, sfreq = 0;

  // Skip ftype(1-bit) + DeficitSample Count(5-bits) + CRC Present Flag(1-bit)
  reader.SkipBits(7);
  reader.ReadBits(7, &nblks);
  reader.ReadBits(14, &fsize);
  reader.SkipBits(6);  // Skip AMODE
  reader.ReadBits(4, &sfreq);
  reader.SkipBits(10);  // Skip: RATE, FixedBit, DNYF, TIMEF, AUSX, HDCD
  reader.ReadBits(3, &ext_audio_id);
  reader.ReadBits(1, &ext_audio);

  constexpr int kSampleRateCore[16] = {0,     8000,  16000, 32000, 0, 0,
                                       11025, 22050, 44100, 0,     0, 12000,
                                       24000, 48000, 0,     0};

  if (fsize < 95)  // Invalid values of FSIZE is 0-94.
    return 0;

  if (nblks < 5 || nblks > 127)  // Valid values of nblks is 5-127.
    return 0;

  if (kSampleRateCore[sfreq] == 0)  // Table value of 0 indicates invalid
    return 0;

  // extended audio may modify sample count and rate
  bool is_core_x96 = ext_audio && ext_audio_id == 2;

  if (channel_layout)
    *channel_layout = media::CHANNEL_LAYOUT_5_1;

  if (extra_data)
    extra_data->clear();

  if (frame_size)
    *frame_size = fsize + 1;  // Framesize is FSIZE + 1.

  if (metadata_frame)
    *metadata_frame = false;

  // Use nblks to compute frame duration, a.k.a number of PCM samples per
  // channel in the current DTS frames in the buffer.
  if (sample_count) {
    *sample_count = (nblks + 1) * 32;  // Num of PCM samples in current frame
    if (is_core_x96)
      *sample_count <<= 1;
  }

  if (sample_rate) {
    *sample_rate = kSampleRateCore[sfreq];  // sfreq is table lookup
    if (is_core_x96)
      *sample_rate <<= 1;
  }

  return reader.bits_read() / 8;
}

}  // namespace media
