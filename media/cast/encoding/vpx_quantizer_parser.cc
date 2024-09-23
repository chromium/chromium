// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/cast/encoding/vpx_quantizer_parser.h"

#include "base/logging.h"
#include "base/memory/raw_ptr.h"

namespace media {
namespace cast {

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
  VpxBitReader(const uint8_t* data, size_t size)
      : encoded_data_(data), encoded_data_end_(data + size) {
    VpxDecoderReadBytes();
  }
  ~VpxBitReader() = default;

  VpxBitReader(const VpxBitReader&) = delete;
  VpxBitReader& operator=(const VpxBitReader&) = delete;
  VpxBitReader(VpxBitReader&&) = delete;
  VpxBitReader& operator=(VpxBitReader&&) = delete;

  // Decode one bit. The output is 0 or 1.
  unsigned int DecodeBit();
  // Decode a value with |num_bits|. The decoding order is MSB first.
  unsigned int DecodeValue(unsigned int num_bits);

 private:
  // Read new bytes from the encoded data buffer until |bit_count_| > 0.
  void VpxDecoderReadBytes();

  raw_ptr<const uint8_t, AllowPtrArithmetic>
      encoded_data_;  // Current byte to decode.
  const raw_ptr<const uint8_t>
      encoded_data_end_;  // The end of the byte to decode.
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
const uint8_t vpx_shift[128] = {
    0, 7, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};

// Mapping from the q_index(0-127) to the quantizer value(0-63).
const uint8_t vpx_quantizer_lookup[128] = {
    0,  1,  2,  3,  4,  5,  6,  6,  7,  8,  9,  10, 10, 11, 12, 12, 13, 13, 14,
    15, 16, 17, 18, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 27, 28, 28, 29, 29,
    30, 30, 31, 31, 32, 32, 33, 33, 34, 34, 35, 35, 36, 36, 37, 37, 38, 38, 39,
    39, 40, 40, 41, 41, 42, 42, 42, 43, 43, 43, 44, 44, 44, 45, 45, 45, 46, 46,
    46, 47, 47, 47, 48, 48, 48, 49, 49, 49, 50, 50, 50, 51, 51, 51, 52, 52, 52,
    53, 53, 53, 54, 54, 54, 55, 55, 55, 56, 56, 56, 57, 57, 57, 58, 58, 58, 59,
    59, 59, 60, 60, 60, 61, 61, 61, 62, 62, 62, 63, 63, 63};

void VpxBitReader::VpxDecoderReadBytes() {
  int shift = -bit_count_;
  while ((shift >= 0) && (encoded_data_ < encoded_data_end_)) {
    bit_count_ += 8;
    value_ |= static_cast<unsigned int>(*encoded_data_) << shift;
    ++encoded_data_;
    shift -= 8;
  }
}

unsigned int VpxBitReader::DecodeBit() {
  unsigned int decoded_bit = 0;
  unsigned int split = 1 + (((range_ - 1) * 128) >> 8);
  if (bit_count_ < 0) {
    VpxDecoderReadBytes();
  }
  DCHECK_GE(bit_count_, 0);
  unsigned int shifted_split = split << 8;
  if (value_ >= shifted_split) {
    range_ -= split;
    value_ -= shifted_split;
    decoded_bit = 1;
  } else {
    range_ = split;
  }
  if (range_ < 128) {
    int shift = vpx_shift[range_];
    range_ <<= shift;
    value_ <<= shift;
    bit_count_ -= shift;
  }
  return decoded_bit;
}

unsigned int VpxBitReader::DecodeValue(unsigned int num_bits) {
  unsigned int decoded_value = 0;
  for (int i = static_cast<int>(num_bits) - 1; i >= 0; i--) {
    decoded_value |= (DecodeBit() << i);
  }
  return decoded_value;
}

// Parse the Segment Header part in the first partition.
void ParseSegmentHeader(VpxBitReader* bit_reader) {
  const bool segmentation_enabled = (bit_reader->DecodeBit() != 0);
  DVLOG(2) << "segmentation_enabled:" << segmentation_enabled;
  if (segmentation_enabled) {
    const bool update_mb_segmentation_map = (bit_reader->DecodeBit() != 0);
    const bool update_mb_segmentation_data = (bit_reader->DecodeBit() != 0);
    DVLOG(2) << "update_mb_segmentation_data:" << update_mb_segmentation_data;
    if (update_mb_segmentation_data) {
      bit_reader->DecodeBit();
      for (int i = 0; i < 4; ++i) {
        if (bit_reader->DecodeBit()) {
          bit_reader->DecodeValue(7 + 1);  // Parse 7 bits value + 1 sign bit.
        }
      }
      for (int i = 0; i < 4; ++i) {
        if (bit_reader->DecodeBit()) {
          bit_reader->DecodeValue(6 + 1);  // Parse 6 bits value + 1 sign bit.
        }
      }
    }

    if (update_mb_segmentation_map) {
      for (int i = 0; i < 3; ++i) {
        if (bit_reader->DecodeBit()) {
          bit_reader->DecodeValue(8);
        }
      }
    }
  }
}

// Parse the Filter Header in the first partition.
void ParseFilterHeader(VpxBitReader* bit_reader) {
  // Parse 1 bit filter_type + 6 bits loop_filter_level + 3 bits
  // sharpness_level.
  bit_reader->DecodeValue(1 + 6 + 3);
  if (bit_reader->DecodeBit()) {
    if (bit_reader->DecodeBit()) {
      for (int i = 0; i < 4; ++i) {
        if (bit_reader->DecodeBit()) {
          bit_reader->DecodeValue(6 + 1);  // Parse 6 bits value + 1 sign bit.
        }
      }
      for (int i = 0; i < 4; ++i) {
        if (bit_reader->DecodeBit()) {
          bit_reader->DecodeValue(6 + 1);  // Parse 6 bits value + 1 sign bit.
        }
      }
    }
  }
}
}  // unnamed namespace

int ParseVpxHeaderQuantizer(const uint8_t* encoded_data, size_t size) {
  DCHECK(encoded_data);
  if (size <= 3) {
    return -1;
  }
  const bool is_key = !(encoded_data[0] & 1);
  const unsigned int header_3bytes =
      encoded_data[0] | (encoded_data[1] << 8) | (encoded_data[2] << 16);
  // Parse the size of the first partition.
  unsigned int partition_size = (header_3bytes >> 5);
  encoded_data += 3;  // Skip 3 bytes.
  size -= 3;
  if (is_key) {
    if (size <= 7) {
      return -1;
    }
    encoded_data += 7;  // Skip 7 bytes.
    size -= 7;
  }
  if (size < partition_size) {
    return -1;
  }
  VpxBitReader bit_reader(encoded_data, partition_size);
  if (is_key) {
    bit_reader.DecodeValue(1 + 1);  // Parse two bits: color_space + clamp_type.
  }
  ParseSegmentHeader(&bit_reader);
  ParseFilterHeader(&bit_reader);
  // Parse the number of coefficient data partitions.
  bit_reader.DecodeValue(2);
  // Parse the base q_index.
  uint8_t q_index = static_cast<uint8_t>(bit_reader.DecodeValue(7));
  if (q_index > 127) {
    return 63;
  }
  return vpx_quantizer_lookup[q_index];
}

}  //  namespace cast
}  //  namespace media
