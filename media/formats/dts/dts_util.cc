// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/dts/dts_util.h"

#include "base/logging.h"
#include "base/stl_util.h"
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

}  // namespace dts

}  // namespace media
