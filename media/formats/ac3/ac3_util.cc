// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/ac3/ac3_util.h"

#include <array>
#include <optional>

#include "base/logging.h"
#include "media/base/bit_reader.h"

namespace media {

namespace {

// The size in byte of a (E-)AC3 synchronization frame header.
const size_t kHeaderSizeInByte = 8;
// The number of new samples per (E-)AC3 audio block.
const size_t kAudioSamplesPerAudioBlock = 256;
// Each synchronization frame has 6 blocks that provide 256 new audio samples.
const size_t kAudioSamplePerAc3SyncFrame = 6 * kAudioSamplesPerAudioBlock;
// Number of audio blocks per E-AC3 synchronization frame, indexed by
// numblkscod.
constexpr auto kBlocksPerSyncFrame = std::to_array<size_t>({1, 2, 3, 6});
// Sample rates, indexed by fscod.
constexpr auto kSampleRate = std::to_array<size_t>({48000, 44100, 32000});
// Nominal bitrates in kbps, indexed by frmsizecod / 2.
constexpr auto kBitrate = std::to_array<size_t>({
    32,  40,  48,  56,  64,  80,  96,  112, 128, 160,
    192, 224, 256, 320, 384, 448, 512, 576, 640,
});
// 16-bit words per synchronization frame, indexed by frmsizecod.
constexpr auto kSyncFrameSizeInWordsFor44kHz = std::to_array<size_t>({
    69,  70,  87,  88,  104, 105, 121,  122,  139,  140,  174,  175,  208,
    209, 243, 244, 278, 279, 348, 349,  417,  418,  487,  488,  557,  558,
    696, 697, 835, 836, 975, 976, 1114, 1115, 1253, 1254, 1393, 1394,
});

// Utility for unpacking (E-)AC3 header. Note that all fields are encoded.
class Ac3Header {
 public:
  explicit Ac3Header(base::span<const uint8_t> data);

  uint32_t eac3_frame_size_code() const { return eac3_frame_size_code_; }

  uint32_t sample_rate_code() const { return sample_rate_code_; }

  uint32_t eac3_number_of_audio_block_code() const {
    CHECK_NE(sample_rate_code_, 3u);
    return eac3_number_of_audio_block_code_;
  }

  uint32_t ac3_frame_size_code() const { return ac3_frame_size_code_; }

  bool had_parsing_error() const { return !parsing_succeeded_; }

 private:
  // bit[5:15] for E-AC3
  uint32_t eac3_frame_size_code_;
  // bit[16:17] for (E-)AC3
  uint32_t sample_rate_code_;
  // bit[18:23] for AC3
  uint32_t ac3_frame_size_code_;
  // bit[18:19] for E-AC3
  uint32_t eac3_number_of_audio_block_code_;

  bool parsing_succeeded_ = true;
};

Ac3Header::Ac3Header(base::span<const uint8_t> data) {
  CHECK_GE(data.size(), kHeaderSizeInByte);

  BitReader reader(data);
  uint16_t sync_word;
  parsing_succeeded_ &= reader.ReadBits(16, &sync_word);
  CHECK_EQ(sync_word, 0x0B77);

  parsing_succeeded_ &= reader.SkipBits(5);
  parsing_succeeded_ &= reader.ReadBits(11, &eac3_frame_size_code_);
  parsing_succeeded_ &= reader.ReadBits(2, &sample_rate_code_);
  parsing_succeeded_ &= reader.ReadBits(6, &ac3_frame_size_code_);
  if (parsing_succeeded_) {
    eac3_number_of_audio_block_code_ = ac3_frame_size_code_ >> 4;
  }
}

// Search for next synchronization word, which is 0x0B-0x77.
base::span<const uint8_t> FindNextSyncWord(base::span<const uint8_t> data) {
  base::span<const uint8_t> remaining_data = data;

  while (remaining_data.size() >= 2) {
    if (remaining_data[0] == 0x0B && remaining_data[1] == 0x77) {
      if (remaining_data != data) {
        DVLOG(2) << __func__ << " skip " << remaining_data.data() - data.data()
                 << " bytes.";
      }

      return remaining_data;
    } else if (remaining_data[1] != 0x0B) {
      remaining_data = remaining_data.subspan<2u>();
    } else {
      remaining_data = remaining_data.subspan<1u>();
    }
  }

  return {};
}

// Returns the number of audio samples represented by the given E-AC3
// synchronization frame.
size_t ParseEac3SyncFrameSampleCount(const Ac3Header& header) {
  CHECK(!header.had_parsing_error());
  unsigned blocks =
      header.sample_rate_code() == 0x03
          ? 6
          : kBlocksPerSyncFrame[header.eac3_number_of_audio_block_code()];
  return kAudioSamplesPerAudioBlock * blocks;
}

// Returns the size in bytes of the given E-AC3 synchronization frame.
size_t ParseEac3SyncFrameSize(const Ac3Header& header) {
  CHECK(!header.had_parsing_error());
  return 2u * (header.eac3_frame_size_code() + 1);
}

// Returns the number of audio samples in an AC3 synchronization frame.
constexpr size_t GetAc3SyncFrameSampleCount() {
  return kAudioSamplePerAc3SyncFrame;
}

// Returns the size in bytes of the given AC3 synchronization frame.
std::optional<size_t> ParseAc3SyncFrameSize(const Ac3Header& header) {
  CHECK(!header.had_parsing_error());

  if (header.sample_rate_code() >= std::size(kSampleRate) ||
      header.ac3_frame_size_code() >=
          std::size(kSyncFrameSizeInWordsFor44kHz)) {
    DVLOG(2) << __func__ << " Invalid frame header."
             << " fscod:" << header.sample_rate_code()
             << " frmsizecod:" << header.ac3_frame_size_code();
    return std::nullopt;
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
size_t ParseTotalSampleCount(base::span<const uint8_t> data, bool is_eac3) {
  if (data.size() < kHeaderSizeInByte) {
    // Also covers the `data.empty()` case.
    return 0;
  }

  base::span<const uint8_t> remaining_data = FindNextSyncWord(data);
  int total_sample_count = 0;

  while (remaining_data.size() > kHeaderSizeInByte) {
    Ac3Header header(remaining_data);

    // We already check that `header` was given at least `kHeaderSizeInByte` of
    // data above, so this should never fail.
    CHECK(!header.had_parsing_error());

    size_t frame_size = is_eac3 ? ParseEac3SyncFrameSize(header)
                                : ParseAc3SyncFrameSize(header).value_or(0u);
    size_t sample_count = is_eac3 ? ParseEac3SyncFrameSampleCount(header)
                                  : GetAc3SyncFrameSampleCount();

    if (frame_size > 0u && sample_count > 0u) {
      if (remaining_data.size() < frame_size) {
        DVLOG(2) << __func__ << " Incomplete frame, missing "
                 << frame_size - remaining_data.size() << " bytes.";
        break;
      }
      remaining_data = remaining_data.subspan(frame_size);
      total_sample_count += sample_count;
    } else {
      DVLOG(2)
          << __func__
          << " Invalid frame, skip 2 bytes to find next synchronization word.";
      remaining_data = remaining_data.subspan<2u>();
    }

    remaining_data = FindNextSyncWord(remaining_data);
  }

  return total_sample_count;
}

}  // namespace anonymous

// static
size_t Ac3Util::ParseTotalAc3SampleCount(base::span<const uint8_t> data) {
  return ParseTotalSampleCount(data, false);
}

// static
size_t Ac3Util::ParseTotalEac3SampleCount(base::span<const uint8_t> data) {
  return ParseTotalSampleCount(data, true);
}

}  // namespace media
