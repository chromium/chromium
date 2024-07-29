// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/formats/ac3/ac3_util.h"

#include "base/logging.h"
#include "media/base/bit_reader.h"

namespace media {

namespace {

// The size in byte of a (E-)AC3 synchronization frame header.
const int kHeaderSizeInByte = 8;
// The number of new samples per (E-)AC3 audio block.
const int kAudioSamplesPerAudioBlock = 256;
// Each synchronization frame has 6 blocks that provide 256 new audio samples.
const int kAudioSamplePerAc3SyncFrame = 6 * kAudioSamplesPerAudioBlock;
// Number of audio blocks per E-AC3 synchronization frame, indexed by
// numblkscod.
const int kBlocksPerSyncFrame[] = {1, 2, 3, 6};
// Sample rates, indexed by fscod.
const int kSampleRate[] = {48000, 44100, 32000};
// Nominal bitrates in kbps, indexed by frmsizecod / 2.
const int kBitrate[] = {32,  40,  48,  56,  64,  80,  96,  112, 128, 160,
                        192, 224, 256, 320, 384, 448, 512, 576, 640};
// 16-bit words per synchronization frame, indexed by frmsizecod.
const int kSyncFrameSizeInWordsFor44kHz[] = {
    69,  70,  87,  88,  104, 105, 121,  122,  139,  140,  174,  175, 208,
    209, 243, 244, 278, 279, 348, 349,  417,  418,  487,  488,  557, 558,
    696, 697, 835, 836, 975, 976, 1114, 1115, 1253, 1254, 1393, 1394};

// Utility for unpacking (E-)AC3 header. Note that all fields are encoded.
class Ac3Header {
 public:
  Ac3Header(const uint8_t* data, int size);

  uint32_t eac3_frame_size_code() const { return eac3_frame_size_code_; }

  uint32_t sample_rate_code() const { return sample_rate_code_; }

  uint32_t eac3_number_of_audio_block_code() const {
    DCHECK(sample_rate_code_ != 3);
    return eac3_number_of_audio_block_code_;
  }

  uint32_t ac3_frame_size_code() const { return ac3_frame_size_code_; }

 private:
  // bit[5:15] for E-AC3
  uint32_t eac3_frame_size_code_;
  // bit[16:17] for (E-)AC3
  uint32_t sample_rate_code_;
  // bit[18:23] for AC3
  uint32_t ac3_frame_size_code_;
  // bit[18:19] for E-AC3
  uint32_t eac3_number_of_audio_block_code_;
};

Ac3Header::Ac3Header(const uint8_t* data, int size) {
  DCHECK_GE(size, kHeaderSizeInByte);

  BitReader reader(data, size);
  uint16_t sync_word;
  reader.ReadBits(16, &sync_word);
  DCHECK(sync_word == 0x0B77);

  reader.SkipBits(5);
  reader.ReadBits(11, &eac3_frame_size_code_);
  reader.ReadBits(2, &sample_rate_code_);
  reader.ReadBits(6, &ac3_frame_size_code_);
  eac3_number_of_audio_block_code_ = ac3_frame_size_code_ >> 4;
}

// Search for next synchronization word, which is 0x0B-0x77.
const uint8_t* FindNextSyncWord(const uint8_t* const begin,
                                const uint8_t* const end) {
  DCHECK(begin);
  DCHECK(end);
  DCHECK_LE(begin, end);

  const uint8_t* current = begin;

  while (current < end - 1) {
    if (current[0] == 0x0B && current[1] == 0x77) {
      if (current != begin)
        DVLOG(2) << __func__ << " skip " << current - begin << " bytes.";

      return current;
    } else if (current[1] != 0x0B) {
      current += 2;
    } else {
      ++current;
    }
  }

  return nullptr;
}

// Returns the number of audio samples represented by the given E-AC3
// synchronization frame.
int ParseEac3SyncFrameSampleCount(Ac3Header& header) {
  unsigned blocks =
      header.sample_rate_code() == 0x03
          ? 6
          : kBlocksPerSyncFrame[header.eac3_number_of_audio_block_code()];
  return kAudioSamplesPerAudioBlock * blocks;
}

// Returns the size in bytes of the given E-AC3 synchronization frame.
int ParseEac3SyncFrameSize(Ac3Header& header) {
  return 2 * (header.eac3_frame_size_code() + 1);
}

// Returns the number of audio samples in an AC3 synchronization frame.
int GetAc3SyncFrameSampleCount() {
  return kAudioSamplePerAc3SyncFrame;
}

// Returns the size in bytes of the given AC3 synchronization frame.
int ParseAc3SyncFrameSize(Ac3Header& header) {
  if (header.sample_rate_code() >= std::size(kSampleRate) ||
      header.ac3_frame_size_code() >=
          std::size(kSyncFrameSizeInWordsFor44kHz)) {
    DVLOG(2) << __func__ << " Invalid frame header."
             << " fscod:" << header.sample_rate_code()
             << " frmsizecod:" << header.ac3_frame_size_code();
    return -1;
  }

  // See http://atsc.org/wp-content/uploads/2015/03/A52-201212-17.pdf table
  // 5.18, frame size code table.

  int sample_rate = kSampleRate[header.sample_rate_code()];
  if (sample_rate == 44100) {
    return 2 * kSyncFrameSizeInWordsFor44kHz[header.ac3_frame_size_code()];
  }

  int bitrate = kBitrate[header.ac3_frame_size_code() / 2];
  if (sample_rate == 32000) {
    return 6 * bitrate;
  }

  // sample_rate == 48000
  return 4 * bitrate;
}

// Returns the total number of audio samples in the given buffer, which contains
// several complete (E-)AC3 syncframes.
int ParseTotalSampleCount(const uint8_t* data, size_t size, bool is_eac3) {
  DCHECK(data);

  if (size < kHeaderSizeInByte) {
    return 0;
  }

  const uint8_t* const end = data + size;
  const uint8_t* current = FindNextSyncWord(data, end);
  int total_sample_count = 0;

  while (current && end - current > kHeaderSizeInByte) {
    Ac3Header header(current, end - current);

    int frame_size = is_eac3 ? ParseEac3SyncFrameSize(header)
                             : ParseAc3SyncFrameSize(header);
    int sample_count = is_eac3 ? ParseEac3SyncFrameSampleCount(header)
                               : GetAc3SyncFrameSampleCount();

    if (frame_size > 0 && sample_count > 0) {
      current += frame_size;
      if (current > end) {
        DVLOG(2) << __func__ << " Incomplete frame, missing " << current - end
                 << " bytes.";
        break;
      }

      total_sample_count += sample_count;
    } else {
      DVLOG(2)
          << __func__
          << " Invalid frame, skip 2 bytes to find next synchronization word.";
      current += 2;
    }

    current = FindNextSyncWord(current, end);
  }

  return total_sample_count;
}

}  // namespace anonymous

// static
int Ac3Util::ParseTotalAc3SampleCount(const uint8_t* data, size_t size) {
  return ParseTotalSampleCount(data, size, false);
}

// static
int Ac3Util::ParseTotalEac3SampleCount(const uint8_t* data, size_t size) {
  return ParseTotalSampleCount(data, size, true);
}

}  // namespace media
