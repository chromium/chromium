// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/encoding/vpx_quantizer_parser.h"

#include <array>
#include <optional>

#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_span.h"
#include "base/types/cxx23_to_underlying.h"

namespace media::cast {
namespace {
// VpxBitReader is a re-implementation of a subset of the VP8 entropy decoder.
// It is used to decompress the VP8 bitstream for the purposes of quickly
// parsing the VP8 frame headers.  It is mostly the exact same implementation
// found in third_party/libvpx/.../vp8/decoder/dboolhuff.h except that only
// the portion of the implementation needed to parse the frame headers is
// present. As of this writing, the implementation in libvpx could not be
// re-used because of the way that the code is structured, and lack of the
// necessary parts being exported.
class VpxBitReader {
 public:
  explicit VpxBitReader(base::span<const uint8_t> data) : data_(data) {
    VpxDecoderReadBytes();
  }
  ~VpxBitReader() = default;

  VpxBitReader(const VpxBitReader&) = delete;
  VpxBitReader& operator=(const VpxBitReader&) = delete;
  VpxBitReader(VpxBitReader&&) = delete;
  VpxBitReader& operator=(VpxBitReader&&) = delete;

  enum class BitResult : int {
    kError = -1,
    kZero = 0,
    kOne = 1,
  };
  BitResult DecodeBit();

  // Decode a value with |num_bits|. The decoding order is MSB first.
  std::optional<unsigned int> DecodeValue(unsigned int num_bits);

 private:
  // Read new bytes from the encoded data buffer until |bit_count_| > 0.
  void VpxDecoderReadBytes();

  // The current byte to decode.
  base::raw_span<const uint8_t> data_;

  // The following two variables are maintained by the decoder.
  // General decoding rule:
  // If |value_| is in the range of 0 to half of |range_|, output 0.
  // Otherwise output 1.
  // |range_| and |value_| need to be shifted when necessary to avoid underflow.
  unsigned int range_ = 255;
  unsigned int value_ = 0;
  // Number of valid bits left to decode. Initializing it to -8 to let the
  // decoder load two bytes at the beginning. The lower byte is used as
  // a buffer byte. During the decoding, decoder needs to call
  // VpxDecoderReadBytes() to load new bytes when it becomes negative.
  int bit_count_ = -8;
};

// The number of bits to be left-shifted to make the variable range_ over 128.
constexpr const std::array<uint8_t, 128> kVpxShift = {
    0, 7, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};

// Mapping from the q_index(0-127) to the quantizer value(0-63).
constexpr const std::array<uint8_t, 128> kVpxQuantizerLookup = {
    0,  1,  2,  3,  4,  5,  6,  6,  7,  8,  9,  10, 10, 11, 12, 12, 13, 13, 14,
    15, 16, 17, 18, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 27, 28, 28, 29, 29,
    30, 30, 31, 31, 32, 32, 33, 33, 34, 34, 35, 35, 36, 36, 37, 37, 38, 38, 39,
    39, 40, 40, 41, 41, 42, 42, 42, 43, 43, 43, 44, 44, 44, 45, 45, 45, 46, 46,
    46, 47, 47, 47, 48, 48, 48, 49, 49, 49, 50, 50, 50, 51, 51, 51, 52, 52, 52,
    53, 53, 53, 54, 54, 54, 55, 55, 55, 56, 56, 56, 57, 57, 57, 58, 58, 58, 59,
    59, 59, 60, 60, 60, 61, 61, 61, 62, 62, 62, 63, 63, 63};

void VpxBitReader::VpxDecoderReadBytes() {
  int shift = -bit_count_;
  while ((shift >= 0) && (!data_.empty())) {
    bit_count_ += 8;
    value_ |= static_cast<unsigned int>(*data_.data()) << shift;
    data_ = data_.subspan<1>();
    shift -= 8;
  }
}

VpxBitReader::BitResult VpxBitReader::DecodeBit() {
  unsigned int decoded_bit = 0;
  unsigned int split = 1 + (((range_ - 1) * 128) >> 8);
  if (bit_count_ < 0) {
    VpxDecoderReadBytes();
  }

  // We ran out of data unexpectedly. Input data is improperly formatted.
  if (bit_count_ < 0) {
    return BitResult::kError;
  }
  const unsigned int shifted_split = split << 8;
  if (value_ >= shifted_split) {
    range_ -= split;
    value_ -= shifted_split;
    decoded_bit = 1;
  } else {
    range_ = split;
  }
  if (range_ < 128) {
    int shift = kVpxShift[range_];
    range_ <<= shift;
    value_ <<= shift;
    bit_count_ -= shift;
  }
  return decoded_bit ? BitResult::kOne : BitResult::kZero;
}

std::optional<unsigned int> VpxBitReader::DecodeValue(unsigned int num_bits) {
  unsigned int decoded_value = 0;
  for (int i = static_cast<int>(num_bits) - 1; i >= 0; i--) {
    switch (DecodeBit()) {
      case BitResult::kError:
        return std::nullopt;

      case BitResult::kOne:
        decoded_value |= 1 << i;
        break;

      case BitResult::kZero:
        break;
    }
  }
  return decoded_value;
}

// Returns whether or not the consumption of bits was successful.
bool ConsumeBits(VpxBitReader* bit_reader, int bits) {
  switch (bit_reader->DecodeBit()) {
    case VpxBitReader::BitResult::kError:
      return false;
    case VpxBitReader::BitResult::kZero:
      return true;
    case VpxBitReader::BitResult::kOne:
      return bit_reader->DecodeValue(bits).has_value();
  }
}

// Parse the Segment Header part in the first partition.
void ParseSegmentHeader(VpxBitReader* bit_reader) {
  if (bit_reader->DecodeBit() != VpxBitReader::BitResult::kOne) {
    return;
  }
  DVLOG(2) << "segmentation_enabled.";
  const VpxBitReader::BitResult update_mb_segmentation_map =
      bit_reader->DecodeBit();
  const VpxBitReader::BitResult update_mb_segmentation_data =
      bit_reader->DecodeBit();
  DVLOG(2) << "update_mb_segmentation_data:"
           << base::to_underlying(update_mb_segmentation_data);
  if (update_mb_segmentation_map == VpxBitReader::BitResult::kError ||
      update_mb_segmentation_data == VpxBitReader::BitResult::kError) {
    return;
  }
  if (update_mb_segmentation_data == VpxBitReader::BitResult::kOne) {
    bit_reader->DecodeBit();
    for (int i = 0; i < 4; ++i) {
      // Parse 7 bits value + 1 sign bit.
      if (!ConsumeBits(bit_reader, 7 + 1)) {
        return;
      }
    }
    for (int i = 0; i < 4; ++i) {
      // Parse 6 bits value + 1 sign bit.
      if (!ConsumeBits(bit_reader, 6 + 1)) {
        return;
      }
    }
  }

  if (update_mb_segmentation_map == VpxBitReader::BitResult::kOne) {
    for (int i = 0; i < 3; ++i) {
      // Consume an entire byte.
      if (!ConsumeBits(bit_reader, 8)) {
        return;
      }
    }
  }
}

// Parse the Filter Header in the first partition.
void ParseFilterHeader(VpxBitReader* bit_reader) {
  // Parse 1 bit filter_type + 6 bits loop_filter_level + 3 bits
  // sharpness_level.
  bit_reader->DecodeValue(1 + 6 + 3);
  if (bit_reader->DecodeBit() != VpxBitReader::BitResult::kOne) {
    return;
  }
  if (bit_reader->DecodeBit() != VpxBitReader::BitResult::kOne) {
    return;
  }
  for (int i = 0; i < 8; ++i) {
    // Parse 6 bits value + 1 sign bit.
    if (!ConsumeBits(bit_reader, 6 + 1)) {
      return;
    }
  }
}
}  // unnamed namespace

std::optional<int> ParseVpxHeaderQuantizer(base::span<const uint8_t> data) {
  if (data.size() <= 3) {
    return std::nullopt;
  }
  const bool is_key = !(data[0] & 1);
  const unsigned int header_3bytes = data[0] | (data[1] << 8) | (data[2] << 16);

  // Parse the size of the first partition.
  const unsigned int partition_size = (header_3bytes >> 5);
  data = data.subspan<3>();

  if (is_key) {
    if (data.size() <= 7) {
      return std::nullopt;
    }
    data = data.subspan<7>();
  }
  if (data.size() < partition_size) {
    return std::nullopt;
  }

  VpxBitReader bit_reader(data.first(partition_size));
  if (is_key) {
    // Parse two bits: color_space + clamp_type.
    if (!bit_reader.DecodeValue(1 + 1).has_value()) {
      return std::nullopt;
    }
  }

  ParseSegmentHeader(&bit_reader);
  ParseFilterHeader(&bit_reader);

  // Parse the number of coefficient data partitions.
  if (!bit_reader.DecodeValue(2).has_value()) {
    return std::nullopt;
  }

  // Parse the base q_index.
  const std::optional<unsigned int> q_result = bit_reader.DecodeValue(7);
  if (!q_result.has_value()) {
    return std::nullopt;
  }
  return q_result.value() > 127 ? 63 : kVpxQuantizerLookup[q_result.value()];
}

}  //  namespace media::cast
