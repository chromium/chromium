// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/base_paths.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "base/path_service.h"
#include "media/parsers/vp8_parser.h"
#include "media/parsers/webp_parser.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

constexpr size_t kWebPFileAndVp8ChunkHeaderSizeInBytes = 20u;
// clang-format off
constexpr uint8_t kLossyWebPFileHeader[] = {
    'R', 'I', 'F', 'F',
    0x0c, 0x00, 0x00, 0x00,  // == 12 (little endian)
    'W', 'E', 'B', 'P',
    'V', 'P', '8', ' ',
    0x00, 0x00, 0x00, 0x00  // == 0
};
constexpr base::span<const uint8_t> kLossyWebPEncodedData(
    kLossyWebPFileHeader,
    kWebPFileAndVp8ChunkHeaderSizeInBytes);
constexpr base::span<const uint8_t> kInvalidWebPEncodedDataSize(
    kLossyWebPFileHeader,
    kWebPFileAndVp8ChunkHeaderSizeInBytes - 5u);

constexpr uint8_t kLosslessWebPFileHeader[] = {
    'R', 'I', 'F', 'F',
    0x0c, 0x00, 0x00, 0x00,
    'W', 'E', 'B', 'P',
    'V', 'P', '8', 'L',
    0x00, 0x00, 0x00, 0x00
};
constexpr base::span<const uint8_t> kLosslessWebPEncodedData(
    kLosslessWebPFileHeader,
    kWebPFileAndVp8ChunkHeaderSizeInBytes);

constexpr uint8_t kExtendedWebPFileHeader[] = {
    'R', 'I', 'F', 'F',
    0x0c, 0x00, 0x00, 0x00,
    'W', 'E', 'B', 'P',
    'V', 'P', '8', 'X',
    0x00, 0x00, 0x00, 0x00
};
constexpr base::span<const uint8_t> kExtendedWebPEncodedData(
    kExtendedWebPFileHeader,
    kWebPFileAndVp8ChunkHeaderSizeInBytes);

constexpr uint8_t kUnknownWebPFileHeader[] = {
    'R', 'I', 'F', 'F',
    0x0c, 0x00, 0x00, 0x00,
    'W', 'E', 'B', 'P',
    'V', 'P', '8', '~',
    0x00, 0x00, 0x00, 0x00
};
constexpr base::span<const uint8_t> kUnknownWebPEncodedData(
    kUnknownWebPFileHeader,
    kWebPFileAndVp8ChunkHeaderSizeInBytes);

constexpr uint8_t kInvalidRiffWebPFileHeader[] = {
    'X', 'I', 'F', 'F',
    0x0c, 0x00, 0x00, 0x00,
    'W', 'E', 'B', 'P',
    'V', 'P', '8', ' ',
    0x00, 0x00, 0x00, 0x00
};
constexpr base::span<const uint8_t> kInvalidRiffWebPEncodedData(
    kInvalidRiffWebPFileHeader,
    kWebPFileAndVp8ChunkHeaderSizeInBytes);

constexpr uint8_t kInvalidOddFileSizeInWebPFileHeader[] = {
    'R', 'I', 'F', 'F',
    0x0d, 0x00, 0x00, 0x00,  // == 13 (Invalid: should be even)
    'W', 'E', 'B', 'P',
    'V', 'P', '8', ' ',
    0x00, 0x00, 0x00, 0x00,
    0x00
};
constexpr base::span<const uint8_t> kInvalidOddFileSizeInHeaderWebPEncodedData(
    kInvalidOddFileSizeInWebPFileHeader,
    kWebPFileAndVp8ChunkHeaderSizeInBytes + 1u);  // Match the reported size

constexpr uint8_t kInvalidLargerThanLimitFileSizeInWebPFileHeader[] = {
    'R', 'I', 'F', 'F',
    0xfe, 0xff, 0xff, 0xff,  // == 2^32 - 2 (Invalid: should be <= 2^32 - 10)
    'W', 'E', 'B', 'P',
    'V', 'P', '8', ' ',
    0x00, 0x00, 0x00, 0x00
};
constexpr base::span<const uint8_t>
kInvalidLargerThanLimitFileSizeInHeaderWebPEncodedData(
    kInvalidLargerThanLimitFileSizeInWebPFileHeader,
    kWebPFileAndVp8ChunkHeaderSizeInBytes);

constexpr uint8_t kInvalidLargerFileSizeInWebPFileHeader[] = {
    'R', 'I', 'F', 'F',
    0x10, 0x00, 0x00, 0x00,  // == 16 (Invalid: should be 12)
    'W', 'E', 'B', 'P',
    'V', 'P', '8', ' ',
    0x00, 0x00, 0x00, 0x00
};
constexpr base::span<const uint8_t>
kInvalidLargerFileSizeInHeaderWebPEncodedData(
    kInvalidLargerFileSizeInWebPFileHeader,
    kWebPFileAndVp8ChunkHeaderSizeInBytes);

constexpr uint8_t kInvalidKeyFrameSizeInWebPFileHeader[] = {
    'R', 'I', 'F', 'F',
    0x0c, 0x00, 0x00, 0x00,  // == 12
    'W', 'E', 'B', 'P',
    'V', 'P', '8', ' ',
    0xc8, 0x00, 0x00, 0x00  // == 200 (Invalid: should be 0)
};
constexpr base::span<const uint8_t> kInvalidKeyFrameSizeInWebPEncodedData(
    kInvalidKeyFrameSizeInWebPFileHeader,
    kWebPFileAndVp8ChunkHeaderSizeInBytes);

constexpr uint8_t kMismatchingOddVp8FrameSizeAndDataSize[] = {
    'R', 'I', 'F', 'F',
    0x12, 0x00, 0x00, 0x00,  // == 18
    'W', 'E', 'B', 'P',
    'V', 'P', '8', ' ',
    0x03, 0x00, 0x00, 0x00,  // == 3
    0x11, 0xa0, 0x23, 0x00,  // Valid padding byte
    0xfa,  0xcc  // Should not exist.
};
constexpr base::span<const uint8_t>
kMismatchingOddVp8FrameSizeAndDataSizeEncodedData(
    kMismatchingOddVp8FrameSizeAndDataSize,
    kWebPFileAndVp8ChunkHeaderSizeInBytes + 6u);

constexpr uint8_t kMismatchingEvenVp8FrameSizeAndDataSize[] = {
    'R', 'I', 'F', 'F',
    0x12, 0x00, 0x00, 0x00,  // == 18
    'W', 'E', 'B', 'P',
    'V', 'P', '8', ' ',
    0x04, 0x00, 0x00, 0x00,  // == 4
    0x11, 0xa0, 0x23, 0x12,
    0xfc, 0xcd  // Should not exist.
};
constexpr base::span<const uint8_t>
kMismatchingEvenVp8FrameSizeAndDataSizeEncodedData(
    kMismatchingEvenVp8FrameSizeAndDataSize,
    kWebPFileAndVp8ChunkHeaderSizeInBytes + 6u);

constexpr uint8_t kInvalidPaddingByteInVp8DataChunk[] = {
    'R', 'I', 'F', 'F',
    0x10, 0x00, 0x00, 0x00,  // == 16
    'W', 'E', 'B', 'P',
    'V', 'P', '8', ' ',
    0x03, 0x00, 0x00, 0x00,  // == 3
    0x11, 0xa0, 0x23, 0xff   // Invalid: last byte should be 0
};
constexpr base::span<const uint8_t>
kInvalidPaddingByteInVp8DataChunkEncodedData(
    kInvalidPaddingByteInVp8DataChunk,
    kWebPFileAndVp8ChunkHeaderSizeInBytes + 4u);
// clang-format on

}  // namespace

TEST(WebPParserTest, WebPImageFileValidator) {
  // Verify that only lossy WebP formats pass.
  ASSERT_TRUE(IsLossyWebPImage(kLossyWebPEncodedData));

  // Verify that lossless, extended, and unknown WebP formats fail.
  ASSERT_FALSE(IsLossyWebPImage(kLosslessWebPEncodedData));
  ASSERT_FALSE(IsLossyWebPImage(kExtendedWebPEncodedData));
  ASSERT_FALSE(IsLossyWebPImage(kUnknownWebPEncodedData));

  // Verify that invalid WebP file headers and sizes fail.
  ASSERT_FALSE(IsLossyWebPImage(kInvalidRiffWebPEncodedData));
  ASSERT_FALSE(IsLossyWebPImage(kInvalidWebPEncodedDataSize));
}

TEST(WebPParserTest, ParseLossyWebP) {
  base::FilePath data_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &data_dir));

  base::FilePath file_path = data_dir.AppendASCII("media")
                                 .AppendASCII("test")
                                 .AppendASCII("data")
                                 .AppendASCII("red_green_gradient_lossy.webp");

  base::MemoryMappedFile stream;
  ASSERT_TRUE(stream.Initialize(file_path))
      << "Couldn't open stream file: " << file_path.MaybeAsASCII();

  std::unique_ptr<Vp8FrameHeader> result =
      ParseWebPImage(base::span<const uint8_t>(stream.data(), stream.length()));
  ASSERT_TRUE(result);

  ASSERT_TRUE(result->IsKeyframe());
  ASSERT_TRUE(result->data);

  // Original image is 3000x3000.
  ASSERT_EQ(3000u, result->width);
  ASSERT_EQ(3000u, result->height);
}

TEST(WebPParserTest, ParseLosslessWebP) {
  base::FilePath data_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &data_dir));

  base::FilePath file_path =
      data_dir.AppendASCII("media")
          .AppendASCII("test")
          .AppendASCII("data")
          .AppendASCII("yellow_pink_gradient_lossless.webp");

  base::MemoryMappedFile stream;
  ASSERT_TRUE(stream.Initialize(file_path))
      << "Couldn't open stream file: " << file_path.MaybeAsASCII();

  // Should fail because WebP parser does not parse lossless webp images.
  std::unique_ptr<Vp8FrameHeader> result =
      ParseWebPImage(base::span<const uint8_t>(stream.data(), stream.length()));
  ASSERT_FALSE(result);
}

TEST(WebPParserTest, ParseExtendedWebP) {
  base::FilePath data_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &data_dir));

  base::FilePath file_path = data_dir.AppendASCII("media")
                                 .AppendASCII("test")
                                 .AppendASCII("data")
                                 .AppendASCII("bouncy_ball.webp");

  base::MemoryMappedFile stream;
  ASSERT_TRUE(stream.Initialize(file_path))
      << "Couldn't open stream file: " << file_path.MaybeAsASCII();

  // Should fail because WebP parser does not parse extended webp images.
  std::unique_ptr<Vp8FrameHeader> result =
      ParseWebPImage(base::span<const uint8_t>(stream.data(), stream.length()));
  ASSERT_FALSE(result);
}

TEST(WebPParserTest, ParseWebPWithUnknownFormat) {
  // Should fail when the specifier byte at position 16 holds anything but ' '.
  std::unique_ptr<Vp8FrameHeader> result =
      ParseWebPImage(kUnknownWebPEncodedData);
  ASSERT_FALSE(result);
}

TEST(WebPParserTest, ParseWebPWithInvalidHeaders) {
  // Should fail because the header is an invalid WebP container.
  std::unique_ptr<Vp8FrameHeader> result =
      ParseWebPImage(kInvalidRiffWebPEncodedData);
  ASSERT_FALSE(result);

  // Should fail because the header has an invalid size.
  result = ParseWebPImage(kInvalidWebPEncodedDataSize);
  ASSERT_FALSE(result);
}

TEST(WebPParserTest, ParseWebPWithInvalidOddSizeInHeader) {
  // Should fail because the size reported in the header is odd.
  std::unique_ptr<Vp8FrameHeader> result =
      ParseWebPImage(kInvalidOddFileSizeInHeaderWebPEncodedData);
  ASSERT_FALSE(result);
}

TEST(WebPParserTest, ParseWebPWithInvalidLargerThanLimitSizeInHeader) {
  // Should fail because the size reported in the header is larger than
  // 2^32 - 10 per the WebP spec.
  std::unique_ptr<Vp8FrameHeader> result =
      ParseWebPImage(kInvalidLargerThanLimitFileSizeInHeaderWebPEncodedData);
  ASSERT_FALSE(result);
}

TEST(WebPParserTest, ParseWebPWithInvalidFileSizeInHeader) {
  // Should fail because the size reported in the header does not match the
  // actual data size.
  std::unique_ptr<Vp8FrameHeader> result =
      ParseWebPImage(kInvalidLargerFileSizeInHeaderWebPEncodedData);
  ASSERT_FALSE(result);
}

TEST(WebPParserTest, ParseWebPWithEmptyVp8KeyFrameAndIncorrectKeyFrameSize) {
  // Should fail because the reported VP8 key frame size is larger than the
  // the existing data.
  std::unique_ptr<Vp8FrameHeader> result =
      ParseWebPImage(kInvalidKeyFrameSizeInWebPEncodedData);
  ASSERT_FALSE(result);
}

TEST(WebPParserTest, ParseWebPWithMismatchingVp8FrameAndDataSize) {
  // Should fail because the reported VP8 key frame size (even or odd) does not
  // match the encoded data's size.
  std::unique_ptr<Vp8FrameHeader> result =
      ParseWebPImage(kMismatchingOddVp8FrameSizeAndDataSizeEncodedData);
  ASSERT_FALSE(result);

  result = ParseWebPImage(kMismatchingEvenVp8FrameSizeAndDataSizeEncodedData);
  ASSERT_FALSE(result);
}

TEST(WebPParserTest, ParseWebPWithInvalidPaddingByteInVp8DataChunk) {
  // Should fail because the reported VP8 key frame size is odd and the added
  // padding byte is not 0.
  std::unique_ptr<Vp8FrameHeader> result =
      ParseWebPImage(kInvalidPaddingByteInVp8DataChunkEncodedData);
  ASSERT_FALSE(result);
}

TEST(WebPParserTest, ParseWebPWithEmptyVp8KeyFrame) {
  // Should fail because the VP8 parser is passed a data chunk of size 0.
  std::unique_ptr<Vp8FrameHeader> result =
      ParseWebPImage(kLossyWebPEncodedData);
  ASSERT_FALSE(result);
}

}  // namespace media
