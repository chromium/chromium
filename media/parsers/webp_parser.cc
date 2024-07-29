// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/parsers/webp_parser.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

#include "base/bits.h"
#include "base/check_op.h"
#include "base/numerics/safe_conversions.h"
#include "build/build_config.h"
#include "media/parsers/vp8_parser.h"

#if !defined(ARCH_CPU_LITTLE_ENDIAN)
#error Big-Endian architecture not supported.
#endif

namespace media {

namespace {

// The byte position storing the size of the file.
constexpr size_t kFileSizeBytePosition = 4u;

// The byte position in which the WebP image data begins.
constexpr size_t kWebPFileBeginBytePosition = 8u;

// The byte position storing the size of the VP8 frame.
constexpr size_t kVp8FrameSizePosition = 16u;

// The 12 bytes that include the FourCC "WEBPVP8 " plus the VP8 chunk size info.
constexpr size_t kWebPFileHeaderByteSize = 12u;

// A valid WebP image header and VP8 chunk header require 20 bytes.
// The VP8 Key Frame's payload also begins at byte 20.
constexpr size_t kWebPFileAndVp8ChunkHeaderSizeInBytes = 20u;

// The max WebP file size is (2^32 - 10) per the WebP spec:
// https://developers.google.com/speed/webp/docs/riff_container#webp_file_header
constexpr uint32_t kMaxWebPFileSize = (1ull << 32) - 10u;

constexpr size_t kSizeOfUint32t = sizeof(uint32_t);

}  // namespace

bool IsLossyWebPImage(base::span<const uint8_t> encoded_data) {
  if (encoded_data.size() < kWebPFileAndVp8ChunkHeaderSizeInBytes)
    return false;

  DCHECK(encoded_data.data());

  return !memcmp(encoded_data.data(), "RIFF", 4) &&
         !memcmp(encoded_data.data() + kWebPFileBeginBytePosition, "WEBPVP8 ",
                 8);
}

std::unique_ptr<Vp8FrameHeader> ParseWebPImage(
    base::span<const uint8_t> encoded_data) {
  if (!IsLossyWebPImage(encoded_data))
    return nullptr;

  static_assert(CHAR_BIT == 8, "Size of a char is not 8 bits.");
  static_assert(kSizeOfUint32t == 4u, "Size of uint32_t is not 4 bytes.");

  // Try to acquire the WebP file size. IsLossyWebPImage() has ensured
  // that we have enough data to read the file size.
  DCHECK_GE(encoded_data.size(), kFileSizeBytePosition + kSizeOfUint32t);

  // No need to worry about endianness because we assert little-endianness.
  const uint32_t file_size = *reinterpret_cast<const uint32_t*>(
      encoded_data.data() + kFileSizeBytePosition);

  // Check that |file_size| is even, per the WebP spec:
  // https://developers.google.com/speed/webp/docs/riff_container#webp_file_header
  if (file_size % 2 != 0)
    return nullptr;

  // Check that |file_size| <= 2^32 - 10, per the WebP spec:
  // https://developers.google.com/speed/webp/docs/riff_container#webp_file_header
  if (file_size > kMaxWebPFileSize)
    return nullptr;

  // Check that the file size in the header matches the encoded data's size.
  if (base::strict_cast<size_t>(file_size) !=
      encoded_data.size() - kWebPFileBeginBytePosition) {
    return nullptr;
  }

  // Try to acquire the VP8 key frame size and validate that it fits within the
  // encoded data's size.
  DCHECK_GE(encoded_data.size(), kVp8FrameSizePosition + kSizeOfUint32t);

  const uint32_t vp8_frame_size = *reinterpret_cast<const uint32_t*>(
      encoded_data.data() + kVp8FrameSizePosition);

  // Check that the VP8 frame size is bounded by the WebP size.
  if (base::strict_cast<size_t>(file_size) - kWebPFileHeaderByteSize <
      base::strict_cast<size_t>(vp8_frame_size)) {
    return nullptr;
  }

  // Check that the size of the encoded data is consistent.
  const size_t vp8_padded_frame_size =
      base::bits::AlignUp(size_t{vp8_frame_size}, size_t{2});
  if (encoded_data.size() - kWebPFileAndVp8ChunkHeaderSizeInBytes !=
      vp8_padded_frame_size) {
    return nullptr;
  }

  // Check that the last byte is 0 if |vp8_frame_size| is odd per WebP specs:
  // https://developers.google.com/speed/webp/docs/riff_container#riff_file_format
  if (vp8_frame_size % 2 &&
      encoded_data.data()[encoded_data.size() - 1] != 0u) {
    return nullptr;
  }

  // Attempt to parse the VP8 frame.
  Vp8Parser vp8_parser;
  auto result = std::make_unique<Vp8FrameHeader>();
  if (vp8_parser.ParseFrame(
          encoded_data.data() + kWebPFileAndVp8ChunkHeaderSizeInBytes,
          base::strict_cast<size_t>(vp8_frame_size), result.get())) {
    return result;
  }
  return nullptr;
}

}  // namespace media
