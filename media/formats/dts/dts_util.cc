// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/dts/dts_util.h"

#include <algorithm>

#include "base/logging.h"
#include "media/base/audio_parameters.h"
#include "media/base/bit_reader.h"
#include "media/formats/dts/dts_stream_parser.h"

namespace media {

namespace dts {

namespace {
// Match a 32-bit sync word with the content in the buffer.
bool MatchSyncWord(const uint8_t* data, uint32_t sync_word) {
  return data[0] == static_cast<uint8_t>(sync_word >> 24) &&
         data[1] == static_cast<uint8_t>(sync_word >> 16) &&
         data[2] == static_cast<uint8_t>(sync_word >> 8) &&
         data[3] == static_cast<uint8_t>(sync_word);
}

// Search for the next sync word 0x7ffe8001.
const uint8_t* FindNextSyncWord(const uint8_t* begin,
                                const uint8_t* end,
                                uint32_t sync_word) {
  DCHECK(begin);
  DCHECK(end);
  DCHECK_LE(begin, end);

  const int sync_word_len_less_one = 3;
  const uint8_t* current = begin;
  const uint8_t first_sync_byte = static_cast<uint8_t>(sync_word >> 24);

  while (current && (current < end - sync_word_len_less_one)) {
    if (MatchSyncWord(current, sync_word)) {
      if (current != begin)
        DVLOG(2) << __func__ << " skip " << current - begin << " bytes.";
      return current;
    }

    ++current;
    current = static_cast<const uint8_t*>(
        memchr(current, first_sync_byte, end - current));
  }

  return nullptr;
}

}  // namespace

// Returns the total number of audio samples in the given buffer,
// which could contain several complete DTS sync frames.
// The parameter AudioCodec is for future samplecount support for DTSHD and
// DTSX bitstreams.
int ParseTotalSampleCount(const uint8_t* data,
                          size_t size,
                          AudioCodec dts_codec_type) {
  if (!data)
    return 0;

  uint32_t sync_word = 0;
  uint32_t header_size = 0;

  // Switch statement used here for future expansion to support
  // other DTS audio types
  switch (dts_codec_type) {
    case AudioCodec::kDTS:
      sync_word = DTSStreamParser::kDTSCoreSyncWord;
      header_size = DTSStreamParser::kDTSCoreHeaderSizeInBytes;
      break;
    default:
      sync_word = 0;
      header_size = 0;
  }

  if (size < header_size)
    return 0;

  DTSStreamParser parser;
  const uint8_t* dend = data + size;
  const uint8_t* current = FindNextSyncWord(data, dend, sync_word);
  int total_sample_count = 0;

  while (current && (dend > current + header_size)) {
    int frame_size;
    int sample_count;
    int bytes_processed =
        parser.ParseFrameHeader(current, dend - current, &frame_size, nullptr,
                                nullptr, &sample_count, nullptr, nullptr);

    if ((bytes_processed > 0) && (frame_size > 0) && (sample_count > 0)) {
      current += frame_size;
      if (current > dend) {
        DVLOG(2) << __func__ << " Incomplete frame, missing " << current - dend
                 << " bytes.";
        break;
      }

      total_sample_count += sample_count;
    } else {
      DVLOG(2)
          << __func__
          << " Invalid frame, skip 1 byte to find next synchronization word.";
      current++;
    }

    current = FindNextSyncWord(current, dend, sync_word);
  }

  return total_sample_count;
}

namespace {

constexpr size_t kDTSSamplesPerFrame = 512;
constexpr size_t kDTSXP2SamplesPerFrame = 1024;

}  // namespace

int WrapDTSWithIEC61937(base::span<const uint8_t> input,
                        base::span<uint8_t> output,
                        AudioCodec dts_codec_type) {
  if (dts_codec_type == AudioCodec::kDTS) {
    // IEC 61937 frame for DTS-CA (IEC 61937-5) is defined as
    // 2 bytes per sample * 2 channel * 512 samples per frame.
    constexpr size_t kDTSFrameSize = 2 * 2 * kDTSSamplesPerFrame;
    static constexpr uint8_t kDTSCAHeader[] = {0x72, 0xF8, 0x1F, 0x4E,
                                               0x0B, 0x00, 0x00, 0x20};

    // Output bytes: header + data + optional 2-byte alignment.
    size_t output_bytes = sizeof(kDTSCAHeader) + input.size();
    if (output_bytes & 1)
      output_bytes++;

    // Header + input data must fit in output buffer, limited to one DTS frame.
    if (input.size() > kDTSFrameSize - sizeof(kDTSCAHeader) ||
        output_bytes > output.size()) {
      return 0;
    }

    // Copy header to output buffer.
    auto [output_header, output_rem] = output.split_at<sizeof(kDTSCAHeader)>();
    output_header.copy_from(kDTSCAHeader);

    // Perform 16-bit byte swap while copying from input to output. If the input
    // buffer is not even-sized, we drop the last byte.
    //
    // NOTE: This was historically done with a cast to `uint16_t*` however the
    // input is not correctly aligned for that, so the dereference of the
    // pointer would cause UB.
    const size_t byte_pairs = input.size() / 2u;
    auto [output_data, output_padding] = output_rem.split_at(byte_pairs * 2u);
    for (size_t i = 0u; i < byte_pairs; ++i) {
      output_data[2u * i] = input[2u * i + 1u];
      output_data[2u * i + 1u] = input[2u * i];
    }

    // Zero fill the remaining output buffer.
    std::ranges::fill(output_padding, uint8_t{0});

    return kDTSFrameSize;
  }
  if (dts_codec_type == AudioCodec::kDTSXP2) {
    NOTIMPLEMENTED();
  }
  return 0;
}

int GetDTSSamplesPerFrame(AudioCodec dts_codec_type) {
  switch (dts_codec_type) {
    case AudioCodec::kDTS:
      return kDTSSamplesPerFrame;
    case AudioCodec::kDTSXP2:
      return kDTSXP2SamplesPerFrame;
    default:
      return 0;
  }
}

}  // namespace dts

}  // namespace media
