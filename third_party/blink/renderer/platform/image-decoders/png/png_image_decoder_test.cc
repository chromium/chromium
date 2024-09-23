// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/image-decoders/png/png_image_decoder.h"

#include <memory>

#include "base/logging.h"
#include "base/time/time.h"
#include "png.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/skia/include/core/SkColorPriv.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkRefCnt.h"

// web_tests/images/resources/png-animated-idat-part-of-animation.png
// is modified in multiple tests to simulate erroneous PNGs. As a reference,
// the table below shows how the file is structured.
//
// Offset | 8     33    95    133   172   210   241   279   314   352   422
// -------------------------------------------------------------------------
// Chunk  | IHDR  acTL  fcTL  IDAT  fcTL  fdAT  fcTL  fdAT  fcTL  fdAT  IEND
//
// In between the acTL and fcTL there are two other chunks, PLTE and tRNS, but
// those are not specifically used in this test suite. The same holds for a
// tEXT chunk in between the last fdAT and IEND.
//
// In the current behavior of PNG image decoders, the 4 frames are detected when
// respectively 141, 249, 322 and 430 bytes are received. The first frame should
// be detected when the IDAT has been received, and non-first frames when the
// next fcTL or IEND chunk has been received. Note that all offsets are +8,
// because a chunk is identified by byte 4-7.

namespace blink {

namespace {

std::unique_ptr<ImageDecoder> CreatePNGDecoder(
    ImageDecoder::AlphaOption alpha_option) {
  return std::make_unique<PNGImageDecoder>(
      alpha_option, ImageDecoder::kDefaultBitDepth,
      ColorBehavior::kTransformToSRGB, ImageDecoder::kNoDecodedImageByteLimit);
}

std::unique_ptr<ImageDecoder> CreatePNGDecoder() {
  return CreatePNGDecoder(ImageDecoder::kAlphaNotPremultiplied);
}

std::unique_ptr<ImageDecoder> Create16BitPNGDecoder() {
  return std::make_unique<PNGImageDecoder>(
      ImageDecoder::kAlphaNotPremultiplied,
      ImageDecoder::kHighBitDepthToHalfFloat, ColorBehavior::kTag,
      ImageDecoder::kNoDecodedImageByteLimit);
}

std::unique_ptr<ImageDecoder> CreatePNGDecoderWithPngData(
    const char* png_file) {
  auto decoder = CreatePNGDecoder();
  scoped_refptr<SharedBuffer> data = ReadFileToSharedBuffer(png_file);
  EXPECT_FALSE(data->empty());
  decoder->SetData(data.get(), true);
  return decoder;
}

void TestSize(const char* png_file, gfx::Size expected_size) {
  auto decoder = CreatePNGDecoderWithPngData(png_file);
  EXPECT_TRUE(decoder->IsSizeAvailable());
  EXPECT_EQ(expected_size, decoder->Size());
}

// Test whether querying for the size of the image works if we present the
// data byte by byte.
void TestSizeByteByByte(const char* png_file,
                        size_t bytes_needed_to_decode_size,
                        gfx::Size expected_size) {
  auto decoder = CreatePNGDecoder();
  Vector<char> data = ReadFile(png_file);
  ASSERT_FALSE(data.empty());
  ASSERT_LT(bytes_needed_to_decode_size, data.size());

  const char* source = data.data();
  scoped_refptr<SharedBuffer> partial_data = SharedBuffer::Create();
  for (size_t length = 1; length <= bytes_needed_to_decode_size; length++) {
    partial_data->Append(source++, 1u);
    decoder->SetData(partial_data.get(), false);

    if (length < bytes_needed_to_decode_size) {
      EXPECT_FALSE(decoder->IsSizeAvailable());
      EXPECT_TRUE(decoder->Size().IsEmpty());
      EXPECT_FALSE(decoder->Failed());
    } else {
      EXPECT_TRUE(decoder->IsSizeAvailable());
      EXPECT_EQ(expected_size, decoder->Size());
    }
  }
  EXPECT_FALSE(decoder->Failed());
}

void WriteUint32(uint32_t val, png_byte* data) {
  data[0] = val >> 24;
  data[1] = val >> 16;
  data[2] = val >> 8;
  data[3] = val;
}

void TestRepetitionCount(const char* png_file, int expected_repetition_count) {
  auto decoder = CreatePNGDecoderWithPngData(png_file);
  // Decoding the frame count sets the number of repetitions as well.
  decoder->FrameCount();
  EXPECT_FALSE(decoder->Failed());
  EXPECT_EQ(expected_repetition_count, decoder->RepetitionCount());
}

struct PublicFrameInfo {
  base::TimeDelta duration;
  gfx::Rect frame_rect;
  ImageFrame::AlphaBlendSource alpha_blend;
  ImageFrame::DisposalMethod disposal_method;
};

// This is the frame data for the following PNG image:
// web_tests/images/resources/png-animated-idat-part-of-animation.png
static PublicFrameInfo g_png_animated_frame_info[] = {
    {base::Milliseconds(500),
     {gfx::Point(0, 0), gfx::Size(5, 5)},
     ImageFrame::kBlendAtopBgcolor,
     ImageFrame::kDisposeKeep},
    {base::Milliseconds(900),
     {gfx::Point(1, 1), gfx::Size(3, 1)},
     ImageFrame::kBlendAtopBgcolor,
     ImageFrame::kDisposeOverwriteBgcolor},
    {base::Milliseconds(2000),
     {gfx::Point(1, 2), gfx::Size(3, 2)},
     ImageFrame::kBlendAtopPreviousFrame,
     ImageFrame::kDisposeKeep},
    {base::Milliseconds(1500),
     {gfx::Point(1, 2), gfx::Size(3, 1)},
     ImageFrame::kBlendAtopBgcolor,
     ImageFrame::kDisposeKeep},
};

void CompareFrameWithExpectation(const PublicFrameInfo& expected,
                                 ImageDecoder* decoder,
                                 size_t index) {
  EXPECT_EQ(expected.duration, decoder->FrameDurationAtIndex(index));

  const auto* frame = decoder->DecodeFrameBufferAtIndex(index);
  ASSERT_TRUE(frame);

  EXPECT_EQ(expected.duration, frame->Duration());
  EXPECT_EQ(expected.frame_rect, frame->OriginalFrameRect());
  EXPECT_EQ(expected.disposal_method, frame->GetDisposalMethod());
  EXPECT_EQ(expected.alpha_blend, frame->GetAlphaBlendSource());
}

// This function removes |length| bytes at |offset|, and then calls FrameCount.
// It assumes the missing bytes should result in a failed decode because the
// parser jumps |length| bytes too far in the next chunk.
void TestMissingDataBreaksDecoding(const char* png_file,
                                   size_t offset,
                                   size_t length) {
  auto decoder = CreatePNGDecoder();
  Vector<char> data = ReadFile(png_file);
  ASSERT_FALSE(data.empty());

  scoped_refptr<SharedBuffer> invalid_data =
      SharedBuffer::Create(data.data(), offset);
  invalid_data->Append(data.data() + offset + length,
                       data.size() - offset - length);
  ASSERT_EQ(data.size() - length, invalid_data->size());

  decoder->SetData(invalid_data, true);
  decoder->FrameCount();
  EXPECT_TRUE(decoder->Failed());
}

// Verify that a decoder with a parse error converts to a static image.
static void ExpectStatic(ImageDecoder* decoder) {
  EXPECT_EQ(1u, decoder->FrameCount());
  EXPECT_FALSE(decoder->Failed());

  ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(0);
  ASSERT_NE(nullptr, frame);
  EXPECT_EQ(ImageFrame::kFrameComplete, frame->GetStatus());
  EXPECT_FALSE(decoder->Failed());
  EXPECT_EQ(kAnimationNone, decoder->RepetitionCount());
}

// Decode up to the indicated fcTL offset and then provide an fcTL with the
// wrong chunk size (20 instead of 26).
void TestInvalidFctlSize(const char* png_file,
                         size_t offset_fctl,
                         size_t expected_frame_count,
                         bool should_fail) {
  Vector<char> data = ReadFile(png_file);
  ASSERT_FALSE(data.empty());

  auto decoder = CreatePNGDecoder();
  scoped_refptr<SharedBuffer> invalid_data =
      SharedBuffer::Create(data.data(), offset_fctl);

  // Test if this gives the correct frame count, before the fcTL is parsed.
  decoder->SetData(invalid_data, false);
  EXPECT_EQ(expected_frame_count, decoder->FrameCount());
  ASSERT_FALSE(decoder->Failed());

  // Append the wrong size to the data stream
  png_byte size_chunk[4];
  WriteUint32(20, size_chunk);
  invalid_data->Append(reinterpret_cast<char*>(size_chunk), 4u);

  // Skip the size in the original data, but provide a truncated fcTL,
  // which is 4B of tag, 20B of data and 4B of CRC, totalling 28B.
  invalid_data->Append(data.data() + offset_fctl + 4, 28u);
  // Append the rest of the data
  const size_t offset_post_fctl = offset_fctl + 38;
  invalid_data->Append(data.data() + offset_post_fctl,
                       data.size() - offset_post_fctl);

  decoder->SetData(invalid_data, false);
  if (should_fail) {
    EXPECT_EQ(expected_frame_count, decoder->FrameCount());
    EXPECT_EQ(true, decoder->Failed());
  } else {
    ExpectStatic(decoder.get());
  }
}

// Verify that the decoder can successfully decode the first frame when
// initially only half of the frame data is received, resulting in a partially
// decoded image, and then the rest of the image data is received. Verify that
// the bitmap hashes of the two stages are different. Also verify that the final
// bitmap hash is equivalent to the hash when all data is provided at once.
//
// This verifies that the decoder correctly keeps track of where it stopped
// decoding when the image was not yet fully received.
void TestProgressiveDecodingContinuesAfterFullData(
    const char* png_file,
    size_t offset_mid_first_frame) {
  Vector<char> full_data = ReadFile(png_file);
  ASSERT_FALSE(full_data.empty());

  auto decoder_upfront = CreatePNGDecoder();
  decoder_upfront->SetData(SharedBuffer::Create(full_data), true);
  EXPECT_GE(decoder_upfront->FrameCount(), 1u);
  const ImageFrame* const frame_upfront =
      decoder_upfront->DecodeFrameBufferAtIndex(0);
  ASSERT_EQ(ImageFrame::kFrameComplete, frame_upfront->GetStatus());
  const unsigned hash_upfront = HashBitmap(frame_upfront->Bitmap());

  auto decoder = CreatePNGDecoder();
  scoped_refptr<SharedBuffer> partial_data =
      SharedBuffer::Create(full_data.data(), offset_mid_first_frame);
  decoder->SetData(partial_data, false);

  EXPECT_EQ(1u, decoder->FrameCount());
  const ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(0);
  EXPECT_EQ(frame->GetStatus(), ImageFrame::kFramePartial);
  const unsigned hash_partial = HashBitmap(frame->Bitmap());

  decoder->SetData(SharedBuffer::Create(full_data), true);
  frame = decoder->DecodeFrameBufferAtIndex(0);
  EXPECT_EQ(frame->GetStatus(), ImageFrame::kFrameComplete);
  const unsigned hash_full = HashBitmap(frame->Bitmap());

  EXPECT_FALSE(decoder->Failed());
  EXPECT_NE(hash_full, hash_partial);
  EXPECT_EQ(hash_full, hash_upfront);
}

}  // Anonymous namespace

// Animated PNG Tests

TEST(AnimatedPNGTests, sizeTest) {
  TestSize(
      "/images/resources/"
      "png-animated-idat-part-of-animation.png",
      gfx::Size(5, 5));
  TestSize(
      "/images/resources/"
      "png-animated-idat-not-part-of-animation.png",
      gfx::Size(227, 35));
}

TEST(AnimatedPNGTests, repetitionCountTest) {
  TestRepetitionCount(
      "/images/resources/"
      "png-animated-idat-part-of-animation.png",
      6u);
  // This is an "animated" image with only one frame, that is, the IDAT is
  // ignored and there is one fdAT frame. so it should be considered
  // non-animated.
  TestRepetitionCount(
      "/images/resources/"
      "png-animated-idat-not-part-of-animation.png",
      kAnimationNone);
}

// Test if the decoded metdata corresponds to the defined expectations
TEST(AnimatedPNGTests, MetaDataTest) {
  const char* png_file =
      "/images/resources/"
      "png-animated-idat-part-of-animation.png";
  constexpr size_t kExpectedFrameCount = 4;

  auto decoder = CreatePNGDecoderWithPngData(png_file);
  ASSERT_EQ(kExpectedFrameCount, decoder->FrameCount());
  for (size_t i = 0; i < kExpectedFrameCount; i++) {
    CompareFrameWithExpectation(g_png_animated_frame_info[i], decoder.get(), i);
  }
}

TEST(AnimatedPNGTests, EmptyFrame) {
  const char* png_file = "/images/resources/empty-frame.png";
  auto decoder = CreatePNGDecoderWithPngData(png_file);
  // Frame 0 is empty. Ensure that decoding frame 1 (which depends on frame 0)
  // fails (rather than crashing).
  EXPECT_EQ(2u, decoder->FrameCount());
  EXPECT_FALSE(decoder->Failed());

  ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(1);
  EXPECT_TRUE(decoder->Failed());
  ASSERT_NE(nullptr, frame);
  EXPECT_EQ(ImageFrame::kFrameEmpty, frame->GetStatus());
}

TEST(AnimatedPNGTests, ByteByByteSizeAvailable) {
  TestSizeByteByByte(
      "/images/resources/"
      "png-animated-idat-part-of-animation.png",
      141u, gfx::Size(5, 5));
  TestSizeByteByByte(
      "/images/resources/"
      "png-animated-idat-not-part-of-animation.png",
      79u, gfx::Size(227, 35));
}

TEST(AnimatedPNGTests, ByteByByteMetaData) {
  const char* png_file =
      "/images/resources/"
      "png-animated-idat-part-of-animation.png";
  constexpr size_t kExpectedFrameCount = 4;

  // These are the byte offsets where each frame should have been parsed.
  // It boils down to the offset of the first fcTL / IEND after the last
  // frame data chunk, plus 8 bytes for recognition. The exception on this is
  // the first frame, which is reported when its first framedata is seen.
  size_t frame_offsets[kExpectedFrameCount] = {141, 249, 322, 430};

  auto decoder = CreatePNGDecoder();
  Vector<char> data = ReadFile(png_file);
  ASSERT_FALSE(data.empty());
  size_t frames_parsed = 0;

  const char* source = data.data();
  scoped_refptr<SharedBuffer> partial_data = SharedBuffer::Create();
  for (size_t length = 1; length <= frame_offsets[kExpectedFrameCount - 1];
       length++) {
    partial_data->Append(source++, 1u);
    decoder->SetData(partial_data.get(), false);
    EXPECT_FALSE(decoder->Failed());
    if (length < frame_offsets[frames_parsed]) {
      EXPECT_EQ(frames_parsed, decoder->FrameCount());
    } else {
      ASSERT_EQ(frames_parsed + 1, decoder->FrameCount());
      CompareFrameWithExpectation(g_png_animated_frame_info[frames_parsed],
                                  decoder.get(), frames_parsed);
      frames_parsed++;
    }
  }
  EXPECT_EQ(kExpectedFrameCount, decoder->FrameCount());
  EXPECT_FALSE(decoder->Failed());
}

TEST(AnimatedPNGTests, TestRandomFrameDecode) {
  TestRandomFrameDecode(&CreatePNGDecoder,
                        "/images/resources/"
                        "png-animated-idat-part-of-animation.png",
                        2u);
}

TEST(AnimatedPNGTests, TestDecodeAfterReallocation) {
  TestDecodeAfterReallocatingData(&CreatePNGDecoder,
                                  "/images/resources/"
                                  "png-animated-idat-part-of-animation.png");
}

TEST(AnimatedPNGTests, ProgressiveDecode) {
  TestProgressiveDecoding(&CreatePNGDecoder,
                          "/images/resources/"
                          "png-animated-idat-part-of-animation.png",
                          13u);
}

TEST(AnimatedPNGTests, ParseAndDecodeByteByByte) {
  TestByteByByteDecode(&CreatePNGDecoder,
                       "/images/resources/"
                       "png-animated-idat-part-of-animation.png",
                       4u, 6u);
}

TEST(AnimatedPNGTests, FailureDuringParsing) {
  // Test the first fcTL in the stream. Because no frame data has been set at
  // this point, the expected frame count is zero. 95 bytes is just before the
  // first fcTL chunk, at which the first frame is detected. This is before the
  // IDAT, so it should be treated as a static image.
  TestInvalidFctlSize(
      "/images/resources/"
      "png-animated-idat-part-of-animation.png",
      95u, 0u, false);

  // Test for the third fcTL in the stream. This should see 1 frame before the
  // fcTL, and then fail when parsing it.
  TestInvalidFctlSize(
      "/images/resources/"
      "png-animated-idat-part-of-animation.png",
      241u, 1u, true);
}

TEST(AnimatedPNGTests, ActlErrors) {
  const char* png_file =
      "/images/resources/"
      "png-animated-idat-part-of-animation.png";
  Vector<char> data = ReadFile(png_file);
  ASSERT_FALSE(data.empty());

  const size_t kOffsetActl = 33u;
  const size_t kAcTLSize = 20u;
  {
    // Remove the acTL chunk from the stream. This results in a static image.
    scoped_refptr<SharedBuffer> no_actl_data =
        SharedBuffer::Create(data.data(), kOffsetActl);
    no_actl_data->Append(data.data() + kOffsetActl + kAcTLSize,
                         data.size() - kOffsetActl - kAcTLSize);

    auto decoder = CreatePNGDecoder();
    decoder->SetData(no_actl_data, true);
    EXPECT_EQ(1u, decoder->FrameCount());
    EXPECT_FALSE(decoder->Failed());
    EXPECT_EQ(kAnimationNone, decoder->RepetitionCount());
  }

  // Store the acTL for more tests.
  char ac_tl[kAcTLSize];
  memcpy(ac_tl, data.data() + kOffsetActl, kAcTLSize);

  // Insert an extra acTL at a couple of different offsets.
  // Prior to the IDAT, this should result in a static image. After, this
  // should fail.
  const struct {
    size_t offset;
    bool should_fail;
  } kGRecs[] = {{8u, false},
                {kOffsetActl, false},
                {133u, false},
                {172u, true},
                {422u, true}};
  for (const auto& rec : kGRecs) {
    const size_t offset = rec.offset;
    scoped_refptr<SharedBuffer> extra_actl_data =
        SharedBuffer::Create(data.data(), offset);
    extra_actl_data->Append(ac_tl, kAcTLSize);
    extra_actl_data->Append(data.data() + offset, data.size() - offset);
    auto decoder = CreatePNGDecoder();
    decoder->SetData(extra_actl_data, true);
    EXPECT_EQ(rec.should_fail ? 0u : 1u, decoder->FrameCount());
    EXPECT_EQ(rec.should_fail, decoder->Failed());
  }

  // An acTL after IDAT is ignored.
  png_file =
      "/images/resources/"
      "cHRM_color_spin.png";
  {
    Vector<char> data2 = ReadFile(png_file);
    ASSERT_FALSE(data2.empty());
    const size_t kPostIDATOffset = 30971u;
    for (size_t times = 0; times < 2; times++) {
      scoped_refptr<SharedBuffer> extra_actl_data =
          SharedBuffer::Create(data2.data(), kPostIDATOffset);
      for (size_t i = 0; i < times; i++) {
        extra_actl_data->Append(ac_tl, kAcTLSize);
      }
      extra_actl_data->Append(data2.data() + kPostIDATOffset,
                              data2.size() - kPostIDATOffset);

      auto decoder = CreatePNGDecoder();
      decoder->SetData(extra_actl_data, true);
      EXPECT_EQ(1u, decoder->FrameCount());
      EXPECT_FALSE(decoder->Failed());
      EXPECT_EQ(kAnimationNone, decoder->RepetitionCount());
      EXPECT_NE(nullptr, decoder->DecodeFrameBufferAtIndex(0));
      EXPECT_FALSE(decoder->Failed());
    }
  }
}

TEST(AnimatedPNGTests, fdatBeforeIdat) {
  const char* png_file =
      "/images/resources/"
      "png-animated-idat-not-part-of-animation.png";
  Vector<char> data = ReadFile(png_file);
  ASSERT_FALSE(data.empty());

  // Insert fcTL and fdAT prior to the IDAT
  const size_t kIdatOffset = 71u;
  scoped_refptr<SharedBuffer> modified_data_buffer =
      SharedBuffer::Create(data.data(), kIdatOffset);
  // Copy fcTL and fdAT
  const size_t kFctlPlusFdatSize = 38u + 1566u;
  modified_data_buffer->Append(data.data() + 2519u, kFctlPlusFdatSize);
  // Copy IDAT
  modified_data_buffer->Append(data.data() + kIdatOffset, 2448u);
  // Copy the remaining
  modified_data_buffer->Append(data.data() + 4123u, 39u + 12u);
  // Data has just been rearranged.
  ASSERT_EQ(data.size(), modified_data_buffer->size());

  {
    // This broken APNG will be treated as a static png.
    auto decoder = CreatePNGDecoder();
    decoder->SetData(modified_data_buffer.get(), true);
    ExpectStatic(decoder.get());
  }

  Vector<char> modified_data = modified_data_buffer->CopyAs<Vector<char>>();

  {
    // Remove the acTL from the modified image. It now has fdAT before
    // IDAT, but no acTL, so fdAT should be ignored.
    const size_t kOffsetActl = 33u;
    const size_t kAcTLSize = 20u;
    scoped_refptr<SharedBuffer> modified_data_buffer2 =
        SharedBuffer::Create(modified_data.data(), kOffsetActl);
    modified_data_buffer2->Append(
        modified_data.data() + kOffsetActl + kAcTLSize,
        modified_data.size() - kOffsetActl - kAcTLSize);
    auto decoder = CreatePNGDecoder();
    decoder->SetData(modified_data_buffer2.get(), true);
    ExpectStatic(decoder.get());

    Vector<char> modified_data2 = modified_data_buffer2->CopyAs<Vector<char>>();
    // Likewise, if an acTL follows the fdAT, it is ignored.
    const size_t kInsertionOffset = kIdatOffset + kFctlPlusFdatSize - kAcTLSize;
    scoped_refptr<SharedBuffer> modified_data3 =
        SharedBuffer::Create(modified_data2.data(), kInsertionOffset);
    modified_data3->Append(data.data() + kOffsetActl, kAcTLSize);
    modified_data3->Append(modified_data2.data() + kInsertionOffset,
                           modified_data2.size() - kInsertionOffset);
    decoder = CreatePNGDecoder();
    decoder->SetData(modified_data3.get(), true);
    ExpectStatic(decoder.get());
  }
}

TEST(AnimatedPNGTests, FrameOverflowX) {
  const char* png_file =
      "/images/resources/"
      "png-animated-idat-part-of-animation.png";
  Vector<char> data = ReadFile(png_file);
  ASSERT_FALSE(data.empty());

  // Change the x_offset for frame 1
  const size_t kFctlOffset = 172u;
  scoped_refptr<SharedBuffer> modified_data =
      SharedBuffer::Create(data.data(), kFctlOffset);
  const size_t kFctlSize = 38u;
  png_byte fctl[kFctlSize];
  memcpy(fctl, data.data() + kFctlOffset, kFctlSize);

  // Set the x_offset to a value that will overflow
  WriteUint32(4294967295, fctl + 20);
  // Correct the crc
  WriteUint32(689600712, fctl + 34);
  modified_data->Append((const char*)fctl, kFctlSize);
  const size_t kAfterFctl = kFctlOffset + kFctlSize;
  modified_data->Append(data.data() + kAfterFctl, data.size() - kAfterFctl);

  auto decoder = CreatePNGDecoder();
  decoder->SetData(modified_data.get(), true);
  for (size_t i = 0; i < decoder->FrameCount(); i++) {
    decoder->DecodeFrameBufferAtIndex(i);
  }
  ASSERT_TRUE(decoder->Failed());
}

// This test is exactly the same as above, except it changes y_offset.
TEST(AnimatedPNGTests, FrameOverflowY) {
  const char* png_file =
      "/images/resources/"
      "png-animated-idat-part-of-animation.png";
  Vector<char> data = ReadFile(png_file);
  ASSERT_FALSE(data.empty());

  // Change the y_offset for frame 1
  const size_t kFctlOffset = 172u;
  scoped_refptr<SharedBuffer> modified_data =
      SharedBuffer::Create(data.data(), kFctlOffset);
  const size_t kFctlSize = 38u;
  png_byte fctl[kFctlSize];
  memcpy(fctl, data.data() + kFctlOffset, kFctlSize);

  // Set the y_offset to a value that will overflow
  WriteUint32(4294967295, fctl + 24);
  // Correct the crc
  WriteUint32(2094185741, fctl + 34);
  modified_data->Append((const char*)fctl, kFctlSize);
  const size_t kAfterFctl = kFctlOffset + kFctlSize;
  modified_data->Append(data.data() + kAfterFctl, data.size() - kAfterFctl);

  auto decoder = CreatePNGDecoder();
  decoder->SetData(modified_data.get(), true);
  for (size_t i = 0; i < decoder->FrameCount(); i++) {
    decoder->DecodeFrameBufferAtIndex(i);
  }
  ASSERT_TRUE(decoder->Failed());
}

TEST(AnimatedPNGTests, IdatSizeMismatch) {
  // The default image must fill the image
  const char* png_file =
      "/images/resources/"
      "png-animated-idat-part-of-animation.png";
  Vector<char> data = ReadFile(png_file);
  ASSERT_FALSE(data.empty());

  const size_t kFctlOffset = 95u;
  scoped_refptr<SharedBuffer> modified_data =
      SharedBuffer::Create(data.data(), kFctlOffset);
  const size_t kFctlSize = 38u;
  png_byte fctl[kFctlSize];
  memcpy(fctl, data.data() + kFctlOffset, kFctlSize);
  // Set the height to a smaller value, so it does not fill the image.
  WriteUint32(3, fctl + 16);
  // Correct the crc
  WriteUint32(3210324191, fctl + 34);
  modified_data->Append((const char*)fctl, kFctlSize);
  const size_t kAfterFctl = kFctlOffset + kFctlSize;
  modified_data->Append(data.data() + kAfterFctl, data.size() - kAfterFctl);

  auto decoder = CreatePNGDecoder();
  decoder->SetData(modified_data.get(), true);
  ExpectStatic(decoder.get());
}

TEST(AnimatedPNGTests, EmptyFdatFails) {
  const char* png_file =
      "/images/resources/"
      "png-animated-idat-part-of-animation.png";
  Vector<char> data = ReadFile(png_file);
  ASSERT_FALSE(data.empty());

  // Modify the third fdAT to be empty.
  constexpr size_t kOffsetThirdFdat = 352;
  scoped_refptr<SharedBuffer> modified_data =
      SharedBuffer::Create(data.data(), kOffsetThirdFdat);
  png_byte four_bytes[4u];
  WriteUint32(0, four_bytes);
  modified_data->Append(reinterpret_cast<char*>(four_bytes), 4u);

  // fdAT tag
  modified_data->Append(data.data() + kOffsetThirdFdat + 4u, 4u);

  // crc computed from modified fdAT chunk
  WriteUint32(4122214294, four_bytes);
  modified_data->Append(reinterpret_cast<char*>(four_bytes), 4u);

  // IEND
  constexpr size_t kIENDOffset = 422u;
  modified_data->Append(data.data() + kIENDOffset, 12u);

  auto decoder = CreatePNGDecoder();
  decoder->SetData(std::move(modified_data), true);
  for (size_t i = 0; i < decoder->FrameCount(); i++) {
    decoder->DecodeFrameBufferAtIndex(i);
  }
  ASSERT_TRUE(decoder->Failed());
}

// Originally, the third frame has an offset of (1,2) and a size of (3,2). By
// changing the offset to (4,4), the frame rect is no longer within the image
// size of 5x5. This results in a failure.
TEST(AnimatedPNGTests, VerifyFrameOutsideImageSizeFails) {
  const char* png_file =
      "/images/resources/"
      "png-animated-idat-part-of-animation.png";
  Vector<char> data = ReadFile(png_file);
  auto decoder = CreatePNGDecoder();
  ASSERT_FALSE(data.empty());

  const size_t kOffsetThirdFctl = 241;
  scoped_refptr<SharedBuffer> modified_data =
      SharedBuffer::Create(data.data(), kOffsetThirdFctl);
  const size_t kFctlSize = 38u;
  png_byte fctl[kFctlSize];
  memcpy(fctl, data.data() + kOffsetThirdFctl, kFctlSize);
  // Modify offset and crc.
  WriteUint32(4, fctl + 20u);
  WriteUint32(4, fctl + 24u);
  WriteUint32(3700322018, fctl + 34u);

  modified_data->Append(const_cast<const char*>(reinterpret_cast<char*>(fctl)),
                        kFctlSize);
  modified_data->Append(data.data() + kOffsetThirdFctl + kFctlSize,
                        data.size() - kOffsetThirdFctl - kFctlSize);

  decoder->SetData(modified_data, true);

  gfx::Size expected_size(5, 5);
  EXPECT_TRUE(decoder->IsSizeAvailable());
  EXPECT_EQ(expected_size, decoder->Size());

  const size_t kExpectedFrameCount = 0;
  EXPECT_EQ(kExpectedFrameCount, decoder->FrameCount());
  EXPECT_TRUE(decoder->Failed());
}

TEST(AnimatedPNGTests, ProgressiveDecodingContinuesAfterFullData) {
  // 160u is a randomly chosen offset in the IDAT chunk of the first frame.
  TestProgressiveDecodingContinuesAfterFullData(
      "/images/resources/"
      "png-animated-idat-part-of-animation.png",
      160u);
}

TEST(AnimatedPNGTests, RandomDecodeAfterClearFrameBufferCache) {
  TestRandomDecodeAfterClearFrameBufferCache(
      &CreatePNGDecoder,
      "/images/resources/"
      "png-animated-idat-part-of-animation.png",
      2u);
}

TEST(AnimatedPNGTests, VerifyAlphaBlending) {
  TestAlphaBlending(&CreatePNGDecoder,
                    "/images/resources/"
                    "png-animated-idat-part-of-animation.png");
}

// This tests if the frame count gets set correctly when parsing FrameCount
// fails in one of the parsing queries.
//
// First, enough data is provided such that two frames should be registered.
// The decoder should at this point not be in the failed status.
//
// Then, we provide the rest of the data except for the last IEND chunk, but
// tell the decoder that this is all the data we have.  The frame count should
// be three, since one extra frame should be discovered. The fourth frame
// should *not* be registered since the reader should not be able to determine
// where the frame ends. The decoder should *not* be in the failed state since
// there are three frames which can be shown.
// Attempting to decode the third frame should fail, since the file is
// truncated.
TEST(AnimatedPNGTests, FailureMissingIendChunk) {
  Vector<char> full_data = ReadFile(
      "/images/resources/"
      "png-animated-idat-part-of-animation.png");
  ASSERT_FALSE(full_data.empty());
  auto decoder = CreatePNGDecoder();

  const size_t kOffsetTwoFrames = 249;
  const size_t kExpectedFramesAfter249Bytes = 2;
  scoped_refptr<SharedBuffer> temp_data =
      SharedBuffer::Create(full_data.data(), kOffsetTwoFrames);
  decoder->SetData(temp_data.get(), false);
  EXPECT_EQ(kExpectedFramesAfter249Bytes, decoder->FrameCount());
  EXPECT_FALSE(decoder->Failed());

  // Provide the rest of the data except for the last IEND chunk.
  const size_t kExpectedFramesAfterAllExcept12Bytes = 3;
  temp_data = SharedBuffer::Create(full_data.data(), full_data.size() - 12);
  decoder->SetData(temp_data.get(), true);
  ASSERT_EQ(kExpectedFramesAfterAllExcept12Bytes, decoder->FrameCount());

  for (size_t i = 0; i < kExpectedFramesAfterAllExcept12Bytes; i++) {
    EXPECT_FALSE(decoder->Failed());
    decoder->DecodeFrameBufferAtIndex(i);
  }

  EXPECT_TRUE(decoder->Failed());
}

// Verify that a malformatted PNG, where the IEND appears before any frame data
// (IDAT), invalidates the decoder.
TEST(AnimatedPNGTests, VerifyIENDBeforeIDATInvalidatesDecoder) {
  Vector<char> full_data = ReadFile(
      "/images/resources/"
      "png-animated-idat-part-of-animation.png");
  ASSERT_FALSE(full_data.empty());
  auto decoder = CreatePNGDecoder();

  const size_t kOffsetIDAT = 133;
  scoped_refptr<SharedBuffer> data =
      SharedBuffer::Create(full_data.data(), kOffsetIDAT);
  data->Append(full_data.data() + full_data.size() - 12u, 12u);
  data->Append(full_data.data() + kOffsetIDAT, full_data.size() - kOffsetIDAT);
  decoder->SetData(data.get(), true);

  const size_t kExpectedFrameCount = 0u;
  EXPECT_EQ(kExpectedFrameCount, decoder->FrameCount());
  EXPECT_TRUE(decoder->Failed());
}

// All IDAT chunks must be before all fdAT chunks
TEST(AnimatedPNGTests, MixedDataChunks) {
  const char* png_file =
      "/images/resources/"
      "png-animated-idat-part-of-animation.png";
  Vector<char> full_data = ReadFile(png_file);
  ASSERT_FALSE(full_data.empty());

  // Add an extra fdAT after the first IDAT, skipping fcTL.
  const size_t kPostIDAT = 172u;
  scoped_refptr<SharedBuffer> data =
      SharedBuffer::Create(full_data.data(), kPostIDAT);
  const size_t kFcTLSize = 38u;
  const size_t kFdATSize = 31u;
  png_byte fdat[kFdATSize];
  memcpy(fdat, full_data.data() + kPostIDAT + kFcTLSize, kFdATSize);
  // Modify the sequence number
  WriteUint32(1u, fdat + 8);
  data->Append((const char*)fdat, kFdATSize);
  const size_t kIENDOffset = 422u;
  data->Append(full_data.data() + kIENDOffset, full_data.size() - kIENDOffset);
  auto decoder = CreatePNGDecoder();
  decoder->SetData(data.get(), true);
  decoder->FrameCount();
  EXPECT_TRUE(decoder->Failed());

  // Insert an IDAT after an fdAT.
  const size_t kPostfdAT = kPostIDAT + kFcTLSize + kFdATSize;
  data = SharedBuffer::Create(full_data.data(), kPostfdAT);
  const size_t kIDATOffset = 133u;
  data->Append(full_data.data() + kIDATOffset, kPostIDAT - kIDATOffset);
  // Append the rest.
  data->Append(full_data.data() + kPostIDAT, full_data.size() - kPostIDAT);
  decoder = CreatePNGDecoder();
  decoder->SetData(data.get(), true);
  decoder->FrameCount();
  EXPECT_TRUE(decoder->Failed());
}

// Verify that erroneous values for the disposal method and alpha blending
// cause the decoder to fail.
TEST(AnimatedPNGTests, VerifyInvalidDisposalAndBlending) {
  const char* png_file =
      "/images/resources/"
      "png-animated-idat-part-of-animation.png";
  Vector<char> full_data = ReadFile(png_file);
  ASSERT_FALSE(full_data.empty());
  auto decoder = CreatePNGDecoder();

  // The disposal byte in the frame control chunk is the 24th byte, alpha
  // blending the 25th. |kOffsetDisposalOp| is 241 bytes to get to the third
  // fctl chunk, 8 bytes to skip the length and tag bytes, and 24 bytes to get
  // to the disposal op.
  //
  // Write invalid values to the disposal and alpha blending byte, correct the
  // crc and append the rest of the buffer.
  const size_t kOffsetDisposalOp = 241 + 8 + 24;
  scoped_refptr<SharedBuffer> data =
      SharedBuffer::Create(full_data.data(), kOffsetDisposalOp);
  png_byte disposal_and_blending[6u];
  disposal_and_blending[0] = 7;
  disposal_and_blending[1] = 9;
  WriteUint32(2408835439u, disposal_and_blending + 2u);
  data->Append(reinterpret_cast<char*>(disposal_and_blending), 6u);
  data->Append(full_data.data() + kOffsetDisposalOp + 6u,
               full_data.size() - kOffsetDisposalOp - 6u);

  decoder->SetData(data.get(), true);
  decoder->FrameCount();
  ASSERT_TRUE(decoder->Failed());
}

// This test verifies that the following situation does not invalidate the
// decoder:
// - Frame 0 is decoded progressively, but there's not enough data to fully
//   decode it.
// - The rest of the image data is received.
// - Frame X, with X > 0, and X does not depend on frame 0, is decoded.
// - Frame 0 is decoded.
// This is a tricky case since the decoder resets the png struct for each frame,
// and this test verifies that it does not break the decoding of frame 0, even
// though it already started in the first call.
TEST(AnimatedPNGTests, VerifySuccessfulFirstFrameDecodeAfterLaterFrame) {
  const char* png_file =
      "/images/resources/"
      "png-animated-three-independent-frames.png";
  auto decoder = CreatePNGDecoder();
  Vector<char> full_data = ReadFile(png_file);
  ASSERT_FALSE(full_data.empty());

  // 160u is a randomly chosen offset in the IDAT chunk of the first frame.
  const size_t kMiddleFirstFrame = 160u;
  scoped_refptr<SharedBuffer> data =
      SharedBuffer::Create(full_data.data(), kMiddleFirstFrame);
  decoder->SetData(data.get(), false);

  ASSERT_EQ(1u, decoder->FrameCount());
  ASSERT_EQ(ImageFrame::kFramePartial,
            decoder->DecodeFrameBufferAtIndex(0)->GetStatus());

  decoder->SetData(SharedBuffer::Create(full_data), true);
  ASSERT_EQ(3u, decoder->FrameCount());
  ASSERT_EQ(ImageFrame::kFrameComplete,
            decoder->DecodeFrameBufferAtIndex(1)->GetStatus());
  // The point is that this call does not decode frame 0, which it won't do if
  // it does not have it as its required previous frame.
  ASSERT_EQ(kNotFound,
            decoder->DecodeFrameBufferAtIndex(1)->RequiredPreviousFrameIndex());

  EXPECT_EQ(ImageFrame::kFrameComplete,
            decoder->DecodeFrameBufferAtIndex(0)->GetStatus());
  EXPECT_FALSE(decoder->Failed());
}

// If the decoder attempts to decode a non-first frame which is subset and
// independent, it needs to discard its png_struct so it can use a modified
// IHDR. Test this by comparing a decode of frame 1 after frame 0 to a decode
// of frame 1 without decoding frame 0.
TEST(AnimatedPNGTests, DecodeFromIndependentFrame) {
  const char* png_file =
      "/images/resources/"
      "png-animated-idat-part-of-animation.png";
  Vector<char> original_data = ReadFile(png_file);
  ASSERT_FALSE(original_data.empty());

  // This file almost fits the bill. Modify it to dispose frame 0, making
  // frame 1 independent.
  const size_t kDisposeOffset = 127u;
  auto data = SharedBuffer::Create(original_data.data(), kDisposeOffset);
  // 1 Corresponds to APNG_DISPOSE_OP_BACKGROUND
  const char kOne = '\001';
  data->Append(&kOne, 1u);
  // No need to modify the blend op
  data->Append(original_data.data() + kDisposeOffset + 1, 1u);
  // Modify the CRC
  png_byte crc[4];
  WriteUint32(2226670956, crc);
  data->Append(reinterpret_cast<const char*>(crc), 4u);
  data->Append(original_data.data() + data->size(),
               original_data.size() - data->size());
  ASSERT_EQ(original_data.size(), data->size());

  auto decoder = CreatePNGDecoder();
  decoder->SetData(data.get(), true);

  ASSERT_EQ(4u, decoder->FrameCount());
  ASSERT_FALSE(decoder->Failed());

  auto* frame = decoder->DecodeFrameBufferAtIndex(0);
  ASSERT_TRUE(frame);
  ASSERT_EQ(ImageFrame::kDisposeOverwriteBgcolor, frame->GetDisposalMethod());

  frame = decoder->DecodeFrameBufferAtIndex(1);
  ASSERT_TRUE(frame);
  ASSERT_FALSE(decoder->Failed());
  ASSERT_NE(gfx::Rect(decoder->Size()), frame->OriginalFrameRect());
  ASSERT_EQ(kNotFound, frame->RequiredPreviousFrameIndex());

  const auto hash = HashBitmap(frame->Bitmap());

  // Now decode starting from frame 1.
  decoder = CreatePNGDecoder();
  decoder->SetData(data.get(), true);

  frame = decoder->DecodeFrameBufferAtIndex(1);
  ASSERT_TRUE(frame);
  EXPECT_EQ(hash, HashBitmap(frame->Bitmap()));
}

// If the first frame is subset from IHDR (only allowed if the first frame is
// not the default image), the decoder has to destroy the png_struct it used
// for parsing so it can use a modified IHDR.
TEST(AnimatedPNGTests, SubsetFromIHDR) {
  const char* png_file =
      "/images/resources/"
      "png-animated-idat-not-part-of-animation.png";
  Vector<char> original_data = ReadFile(png_file);
  ASSERT_FALSE(original_data.empty());

  const size_t kFcTLOffset = 2519u;
  auto data = SharedBuffer::Create(original_data.data(), kFcTLOffset);

  const size_t kFcTLSize = 38u;
  png_byte fc_tl[kFcTLSize];
  memcpy(fc_tl, original_data.data() + kFcTLOffset, kFcTLSize);
  // Modify to have a subset frame (yOffset 1, height 34 out of 35).
  WriteUint32(34, fc_tl + 16u);
  WriteUint32(1, fc_tl + 24u);
  WriteUint32(3972842751, fc_tl + 34u);
  data->Append(reinterpret_cast<const char*>(fc_tl), kFcTLSize);

  // Append the rest of the data.
  // Note: If PNGImageDecoder changes to reject an image with too many
  // rows, the fdAT data will need to be modified as well.
  data->Append(original_data.data() + kFcTLOffset + kFcTLSize,
               original_data.size() - data->size());
  ASSERT_EQ(original_data.size(), data->size());

  // This will test both byte by byte and using the full data, and compare.
  TestByteByByteDecode(CreatePNGDecoder, data.get(), 1, kAnimationNone);
}

TEST(AnimatedPNGTests, Offset) {
  const char* png_file = "/images/resources/apng18.png";
  Vector<char> original_data = ReadFile(png_file);
  ASSERT_FALSE(original_data.empty());

  Vector<unsigned> baseline_hashes;
  scoped_refptr<SharedBuffer> original_data_buffer =
      SharedBuffer::Create(original_data);
  CreateDecodingBaseline(CreatePNGDecoder, original_data_buffer.get(),
                         &baseline_hashes);
  constexpr size_t kExpectedFrameCount = 13;
  ASSERT_EQ(kExpectedFrameCount, baseline_hashes.size());

  constexpr size_t kOffset = 37;
  char buffer[kOffset] = {};

  auto data = SharedBuffer::Create(buffer, kOffset);
  data->Append(original_data);

  // Use the same defaults as CreatePNGDecoder, except use the (arbitrary)
  // non-zero offset.
  auto decoder = std::make_unique<PNGImageDecoder>(
      ImageDecoder::kAlphaNotPremultiplied, ImageDecoder::kDefaultBitDepth,
      ColorBehavior::kTransformToSRGB, ImageDecoder::kNoDecodedImageByteLimit,
      kOffset);
  decoder->SetData(data, true);
  ASSERT_EQ(kExpectedFrameCount, decoder->FrameCount());

  for (size_t i = 0; i < kExpectedFrameCount; ++i) {
    auto* frame = decoder->DecodeFrameBufferAtIndex(i);
    EXPECT_EQ(baseline_hashes[i], HashBitmap(frame->Bitmap()));
  }
}

TEST(AnimatedPNGTests, ExtraChunksBeforeIHDR) {
  const char* png_file = "/images/resources/apng18.png";
  Vector<char> original_data = ReadFile(png_file);
  ASSERT_FALSE(original_data.empty());

  Vector<unsigned> baseline_hashes;
  scoped_refptr<SharedBuffer> original_data_buffer =
      SharedBuffer::Create(original_data);
  CreateDecodingBaseline(CreatePNGDecoder, original_data_buffer.get(),
                         &baseline_hashes);
  constexpr size_t kExpectedFrameCount = 13;
  ASSERT_EQ(kExpectedFrameCount, baseline_hashes.size());

  constexpr size_t kPngSignatureSize = 8;
  auto data = SharedBuffer::Create(original_data.data(), kPngSignatureSize);

  // Arbitrary chunk of data.
  constexpr size_t kExtraChunkSize = 13;
  constexpr png_byte kExtraChunk[kExtraChunkSize] = {
      0, 0, 0, 1, 't', 'R', 'c', 'N', 68, 82, 0, 87, 10};
  data->Append(reinterpret_cast<const char*>(kExtraChunk), kExtraChunkSize);

  // Append the rest of the data from the original.
  data->Append(original_data.data() + kPngSignatureSize,
               original_data.size() - kPngSignatureSize);
  ASSERT_EQ(original_data.size() + kExtraChunkSize, data->size());

  auto decoder = CreatePNGDecoder();
  decoder->SetData(data, true);
  ASSERT_EQ(kExpectedFrameCount, decoder->FrameCount());

  for (size_t i = 0; i < kExpectedFrameCount; ++i) {
    auto* frame = decoder->DecodeFrameBufferAtIndex(i);
    EXPECT_EQ(baseline_hashes[i], HashBitmap(frame->Bitmap()));
  }
}

// Static PNG tests

TEST(StaticPNGTests, repetitionCountTest) {
  TestRepetitionCount("/images/resources/png-simple.png", kAnimationNone);
}

TEST(StaticPNGTests, sizeTest) {
  TestSize("/images/resources/png-simple.png", gfx::Size(111, 29));
}

TEST(StaticPNGTests, MetaDataTest) {
  const size_t kExpectedFrameCount = 1;
  const base::TimeDelta kExpectedDuration;
  auto decoder =
      CreatePNGDecoderWithPngData("/images/resources/png-simple.png");
  EXPECT_EQ(kExpectedFrameCount, decoder->FrameCount());
  EXPECT_EQ(kExpectedDuration, decoder->FrameDurationAtIndex(0));
}

// circle-trns-before-plte.png is of color type 2 (PNG_COLOR_TYPE_RGB) and has
// a tRNS chunk before a PLTE chunk. The image has an opaque blue circle on a
// transparent green background.
//
// The PNG specification version 1.2 says:
//   When present, the tRNS chunk must precede the first IDAT chunk, and must
//   follow the PLTE chunk, if any.
// Therefore, in the default libpng configuration (which defines the
// PNG_READ_OPT_PLTE_SUPPORTED macro), the tRNS chunk is considered invalid and
// ignored. However, png_get_valid(png, info, PNG_INFO_tRNS) still returns a
// nonzero value, so an application may call png_set_tRNS_to_alpha(png) and
// assume libpng's output has alpha, resulting in memory errors. See
// https://github.com/glennrp/libpng/issues/482.
//
// Since Chromium chooses to undefine PNG_READ_OPT_PLTE_SUPPORTED in
// pnglibconf.h, it is not affected by this potential bug. For extra assurance,
// this test decodes this image and makes sure there are no errors.
TEST(StaticPNGTests, ColorType2TrnsBeforePlte) {
  auto decoder = CreatePNGDecoderWithPngData(
      "/images/resources/circle-trns-before-plte.png");
  ASSERT_EQ(decoder->FrameCount(), 1u);
  ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(0);
  ASSERT_TRUE(frame);
  ASSERT_EQ(frame->GetStatus(), ImageFrame::kFrameComplete);
  ASSERT_EQ(frame->GetPixelFormat(), ImageFrame::kN32);
#ifdef PNG_READ_OPT_PLTE_SUPPORTED
  // When the color type is not PNG_COLOR_TYPE_PALETTE, the PLTE chunk is
  // optional. If PNG_READ_OPT_PLTE_SUPPORTED is defined, libpng performs full
  // processing of an optional PLTE chunk. In particular, it checks if there is
  // a tRNS chunk before the PLTE chunk and ignores any such tRNS chunks.
  // Therefore the tRNS chunk in this image is ignored and the frame should not
  // have alpha.
  EXPECT_FALSE(frame->HasAlpha());
  // The background is opaque green.
  EXPECT_EQ(*frame->GetAddr(1, 1), SkPackARGB32(0xFF, 0, 0xFF, 0));
#else
  // If PNG_READ_OPT_PLTE_SUPPORTED is not defined, libpng performs only minimum
  // processing of an optional PLTE chunk. In particular, it doesn't check if
  // there is a tRNS chunk before the PLTE chunk (which would make the tRNS
  // chunk invalid). Therefore the tRNS chunk in this image is considered valid
  // and the frame should have alpha.
  EXPECT_TRUE(frame->HasAlpha());
  // The background is transparent green.
  EXPECT_EQ(*frame->GetAddr(1, 1), SkPackARGB32(0, 0, 0xFF, 0));
#endif
}

TEST(StaticPNGTests, InvalidIHDRChunk) {
  TestMissingDataBreaksDecoding("/images/resources/png-simple.png", 20u, 2u);
}

TEST(StaticPNGTests, ProgressiveDecoding) {
  TestProgressiveDecoding(&CreatePNGDecoder, "/images/resources/png-simple.png",
                          11u);
}

TEST(StaticPNGTests, ProgressiveDecodingContinuesAfterFullData) {
  TestProgressiveDecodingContinuesAfterFullData(
      "/images/resources/png-simple.png", 1000u);
}

struct PNGSample {
  String filename;
  String color_space;
  bool is_transparent;
  bool is_high_bit_depth;
  scoped_refptr<SharedBuffer> png_contents;
  Vector<float> expected_pixels;
};

static void TestHighBitDepthPNGDecoding(const PNGSample& png_sample,
                                        ImageDecoder* decoder) {
  scoped_refptr<SharedBuffer> png = png_sample.png_contents;
  ASSERT_TRUE(png.get());
  decoder->SetData(png.get(), true);
  ASSERT_TRUE(decoder->IsSizeAvailable());
  ASSERT_TRUE(decoder->IsDecodedSizeAvailable());

  gfx::Size size(2, 2);
  ASSERT_EQ(size, decoder->Size());
  ASSERT_EQ(size, decoder->DecodedSize());
  ASSERT_EQ(true, decoder->ImageIsHighBitDepth());

  ASSERT_TRUE(decoder->FrameIsReceivedAtIndex(0));
  ASSERT_EQ(size, decoder->FrameSizeAtIndex(0));

  ASSERT_EQ(1u, decoder->FrameCount());
  ASSERT_EQ(kAnimationNone, decoder->RepetitionCount());

  auto* frame = decoder->DecodeFrameBufferAtIndex(0);
  ASSERT_TRUE(frame);
  ASSERT_EQ(ImageFrame::kFrameComplete, frame->GetStatus());
  ASSERT_EQ(ImageFrame::kRGBA_F16, frame->GetPixelFormat());

  sk_sp<SkImage> image = frame->FinalizePixelsAndGetImage();
  ASSERT_TRUE(image);

  ASSERT_EQ(2, image->width());
  ASSERT_EQ(2, image->height());
  ASSERT_EQ(kRGBA_F16_SkColorType, image->colorType());

  // Readback pixels and convert color components from half float to float.
  SkImageInfo info =
      SkImageInfo::Make(2, 2, kRGBA_F16_SkColorType, kUnpremul_SkAlphaType,
                        image->refColorSpace());
  std::unique_ptr<uint8_t[]> decoded_pixels(
      new uint8_t[info.computeMinByteSize()]());
  ASSERT_TRUE(
      image->readPixels(info, decoded_pixels.get(), info.minRowBytes(), 0, 0));

  float decoded_pixels_float_32[16];
  ASSERT_TRUE(skcms_Transform(
      decoded_pixels.get(), skcms_PixelFormat_RGBA_hhhh,
      skcms_AlphaFormat_Unpremul, nullptr, decoded_pixels_float_32,
      skcms_PixelFormat_RGBA_ffff, skcms_AlphaFormat_Unpremul, nullptr, 4));

  Vector<float> expected_pixels = png_sample.expected_pixels;
  const float decoding_tolerance = 0.001;
  for (int i = 0; i < 16; i++) {
    if (fabs(decoded_pixels_float_32[i] - expected_pixels[i]) >
        decoding_tolerance) {
      FAIL() << "Pixel comparison failed. File: " << png_sample.filename
             << ", component index: " << i
             << ", actual: " << decoded_pixels_float_32[i]
             << ", expected: " << expected_pixels[i]
             << ", tolerance: " << decoding_tolerance;
    }
  }
}

static void FillPNGSamplesSourcePixels(Vector<PNGSample>& png_samples) {
  // Color components of opaque and transparent 16 bit PNG, read with libpng
  // in BigEndian and scaled to [0,1]. The values are read from non-interlaced
  // samples, but used for both interlaced and non-interlaced test cases.
  // The sample pngs were all created by color converting the 8 bit sRGB source
  // in Adobe Photoshop 18. The only exception is e-sRGB test case, for which
  // Adobe software created a non-matching color profile (see crbug.com/874939).
  // Hence, SkEncoder was used to generate the e-sRGB file (see the skia fiddle
  // here: https://fiddle.skia.org/c/17beedfd66dac1ec930f0c414c50f847).
  static const Vector<float> source_pixels_opaque_srgb = {
      0.4986953536, 0.5826657511, 0.7013199054, 1,   // Top left pixel
      0.907988098,  0.8309605554, 0.492011902,  1,   // Top right pixel
      0.6233157855, 0.9726558328, 0.9766536965, 1,   // Bottom left pixel
      0.8946517128, 0.9663080797, 0.9053025101, 1};  // Bottom right pixel
  static const Vector<float> source_pixels_opaque_adobe_rgb = {
      0.4448004883, 0.5216296635, 0.6506294347, 1,   // Top left pixel
      0.8830548562, 0.7978179599, 0.4323186084, 1,   // Top right pixel
      0.6841992828, 0.9704280156, 0.9711299306, 1,   // Bottom left pixel
      0.8874799725, 0.96099794,   0.8875715267, 1};  // Bottom right pixel
  static const Vector<float> source_pixels_opaque_p3 = {
      0.515648127,  0.5802243076, 0.6912489509, 1,   // Top left pixel
      0.8954146639, 0.8337987335, 0.5691767758, 1,   // Top right pixel
      0.772121767,  0.9671625849, 0.973510338,  1,   // Bottom left pixel
      0.9118944076, 0.9645685512, 0.9110704204, 1};  // Bottom right pixel
  static const Vector<float> source_pixels_opaque_e_srgb = {
      0.6977539062, 0.5839843750, 0.4978027344, 1,   // Top left pixel
      0.4899902344, 0.8310546875, 0.9096679688, 1,   // Top right pixel
      0.9760742188, 0.9721679688, 0.6230468750, 1,   // Bottom left pixel
      0.9057617188, 0.9643554688, 0.8940429688, 1};  // Bottom right pixel
  static const Vector<float> source_pixels_opaque_prophoto = {
      0.5032883192, 0.5191271839, 0.6309147784, 1,   // Top left pixel
      0.8184176394, 0.8002899214, 0.5526970321, 1,   // Top right pixel
      0.842526894,  0.945616846,  0.9667048142, 1,   // Bottom left pixel
      0.9119554437, 0.9507133593, 0.9001754788, 1};  // Bottom right pixel
  static const Vector<float> source_pixels_opaque_rec2020 = {
      0.5390554665, 0.5766842145, 0.6851758602, 1,   // Top left pixel
      0.871061265,  0.831326772,  0.5805294881, 1,   // Top right pixel
      0.8386205844, 0.9599603265, 0.9727168688, 1,   // Bottom left pixel
      0.9235217823, 0.9611200122, 0.9112840467, 1};  // Bottom right pixel

  static const Vector<float> source_pixels_transparent_srgb = {
      0.3733272297,  0.4783093004, 0.6266422522, 0.8,   // Top left pixel
      0.8466468299,  0.7182879377, 0.153322652,  0.6,   // Top right pixel
      0.05831998169, 0.9316395819, 0.9416495003, 0.4,   // Bottom left pixel
      0.4733043412,  0.8316319524, 0.5266346227, 0.2};  // Bottom right pixel
  static const Vector<float> source_pixels_transparent_adobe_rgb = {
      0.305943389,  0.4019836728, 0.5632867933,  0.8,   // Top left pixel
      0.8051117723, 0.6630197604, 0.05374227512, 0.6,   // Top right pixel
      0.210482948,  0.926115816,  0.9278248264,  0.4,   // Bottom left pixel
      0.4374456397, 0.8050812543, 0.4379644465,  0.2};  // Bottom right pixel
  static const Vector<float> source_pixels_transparent_p3 = {
      0.3945372702, 0.475257496,  0.6140383001, 0.8,   // Top left pixel
      0.8257114519, 0.7230182345, 0.2819256886, 0.6,   // Top right pixel
      0.4302738994, 0.9179064622, 0.933806363,  0.4,   // Bottom left pixel
      0.5595330739, 0.8228122377, 0.5554436561, 0.2};  // Bottom right pixel
  static const Vector<float> source_pixels_transparent_e_srgb = {
      0.6230468750, 0.4782714844, 0.3723144531, 0.8,   // Top left pixel
      0.1528320312, 0.7172851562, 0.8466796875, 0.6,   // Top right pixel
      0.9409179688, 0.9331054688, 0.0588073730, 0.4,   // Bottom left pixel
      0.5253906250, 0.8310546875, 0.4743652344, 0.2};  // Bottom right pixel
  static const Vector<float> source_pixels_transparent_prophoto = {
      0.379064622,  0.3988708324, 0.5386282139, 0.8,   // Top left pixel
      0.6973525597, 0.6671396963, 0.2544289311, 0.6,   // Top right pixel
      0.6063477531, 0.864103151,  0.9168078126, 0.4,   // Bottom left pixel
      0.5598077363, 0.7536278325, 0.5009384298, 0.2};  // Bottom right pixel
  static const Vector<float> source_pixels_transparent_rec2020 = {
      0.4237735561, 0.4708323796, 0.6064698253, 0.8,   // Top left pixel
      0.7851224537, 0.7188677806, 0.3008468757, 0.6,   // Top right pixel
      0.5965819791, 0.8999618524, 0.9318532082, 0.4,   // Bottom left pixel
      0.6176699474, 0.805600061,  0.5565117876, 0.2};  // Bottom right pixel

  for (PNGSample& png_sample : png_samples) {
    if (png_sample.color_space == "sRGB") {
      png_sample.expected_pixels = png_sample.is_transparent
                                       ? source_pixels_transparent_srgb
                                       : source_pixels_opaque_srgb;
    } else if (png_sample.color_space == "AdobeRGB") {
      png_sample.expected_pixels = png_sample.is_transparent
                                       ? source_pixels_transparent_adobe_rgb
                                       : source_pixels_opaque_adobe_rgb;
    } else if (png_sample.color_space == "DisplayP3") {
      png_sample.expected_pixels = png_sample.is_transparent
                                       ? source_pixels_transparent_p3
                                       : source_pixels_opaque_p3;
    } else if (png_sample.color_space == "e-sRGB") {
      png_sample.expected_pixels = png_sample.is_transparent
                                       ? source_pixels_transparent_e_srgb
                                       : source_pixels_opaque_e_srgb;
    } else if (png_sample.color_space == "ProPhoto") {
      png_sample.expected_pixels = png_sample.is_transparent
                                       ? source_pixels_transparent_prophoto
                                       : source_pixels_opaque_prophoto;
    } else if (png_sample.color_space == "Rec2020") {
      png_sample.expected_pixels = png_sample.is_transparent
                                       ? source_pixels_transparent_rec2020
                                       : source_pixels_opaque_rec2020;
    } else {
      NOTREACHED_IN_MIGRATION();
    }
  }
}

static Vector<PNGSample> GetPNGSamplesInfo(bool include_8bit_pngs) {
  Vector<PNGSample> png_samples;
  Vector<String> interlace_status = {"", "_interlaced"};
  Vector<String> color_spaces = {"sRGB",   "AdobeRGB", "DisplayP3",
                                 "e-sRGB", "ProPhoto", "Rec2020"};
  Vector<String> alpha_status = {"_opaque", "_transparent"};

  for (String color_space : color_spaces) {
    for (String alpha : alpha_status) {
      PNGSample png_sample;
      StringBuilder filename;
      filename.Append("_");
      filename.Append(color_space);
      filename.Append(alpha);
      filename.Append(".png");
      png_sample.filename = filename.ToString();
      png_sample.color_space = color_space;
      png_sample.is_transparent = (alpha == "_transparent");

      for (String interlace : interlace_status) {
        PNGSample high_bit_depth_sample(png_sample);
        high_bit_depth_sample.filename =
            "2x2_16bit" + interlace + high_bit_depth_sample.filename;
        high_bit_depth_sample.is_high_bit_depth = true;
        png_samples.push_back(high_bit_depth_sample);
      }
      if (include_8bit_pngs) {
        PNGSample regular_bit_depth_sample(png_sample);
        regular_bit_depth_sample.filename =
            "2x2_8bit" + regular_bit_depth_sample.filename;
        regular_bit_depth_sample.is_high_bit_depth = false;
        png_samples.push_back(regular_bit_depth_sample);
      }
    }
  }

  return png_samples;
}

TEST(StaticPNGTests, DecodeHighBitDepthPngToHalfFloat) {
  const bool include_8bit_pngs = false;
  Vector<PNGSample> png_samples = GetPNGSamplesInfo(include_8bit_pngs);
  FillPNGSamplesSourcePixels(png_samples);
  String path = "/images/resources/png-16bit/";
  for (PNGSample& png_sample : png_samples) {
    String full_path = path + png_sample.filename;
    png_sample.png_contents = ReadFileToSharedBuffer(full_path);
    auto decoder = Create16BitPNGDecoder();
    TestHighBitDepthPNGDecoding(png_sample, decoder.get());
  }
}

TEST(StaticPNGTests, ImageIsHighBitDepth) {
  const bool include_8bit_pngs = true;
  Vector<PNGSample> png_samples = GetPNGSamplesInfo(include_8bit_pngs);
  gfx::Size size(2, 2);

  String path = "/images/resources/png-16bit/";
  for (PNGSample& png_sample : png_samples) {
    String full_path = path + png_sample.filename;
    png_sample.png_contents = ReadFileToSharedBuffer(full_path);
    ASSERT_TRUE(png_sample.png_contents.get());

    std::unique_ptr<ImageDecoder> decoders[] = {CreatePNGDecoder(),
                                                Create16BitPNGDecoder()};
    for (auto& decoder : decoders) {
      decoder->SetData(png_sample.png_contents.get(), true);
      ASSERT_TRUE(decoder->IsSizeAvailable());
      ASSERT_TRUE(decoder->IsDecodedSizeAvailable());
      ASSERT_EQ(size, decoder->Size());
      ASSERT_EQ(size, decoder->DecodedSize());
      ASSERT_EQ(png_sample.is_high_bit_depth, decoder->ImageIsHighBitDepth());
    }
  }
}

TEST(PNGTests, VerifyFrameCompleteBehavior) {
  struct {
    const char* name;
    size_t expected_frame_count;
    size_t offset_in_first_frame;
  } g_recs[] = {
      {"/images/resources/"
       "png-animated-three-independent-frames.png",
       3u, 150u},
      {"/images/resources/"
       "png-animated-idat-part-of-animation.png",
       4u, 160u},

      {"/images/resources/png-simple.png", 1u, 700u},
      {"/images/resources/gracehopper.png", 1u, 40000u},
  };
  for (const auto& rec : g_recs) {
    Vector<char> full_data = ReadFile(rec.name);

    // Create with enough data for part of the first frame.
    auto decoder = CreatePNGDecoder();
    auto data =
        SharedBuffer::Create(full_data.data(), rec.offset_in_first_frame);
    decoder->SetData(data.get(), false);

    EXPECT_FALSE(decoder->FrameIsReceivedAtIndex(0));

    // Parsing the size is not enough to mark the frame as complete.
    EXPECT_TRUE(decoder->IsSizeAvailable());
    EXPECT_FALSE(decoder->FrameIsReceivedAtIndex(0));

    const auto partial_frame_count = decoder->FrameCount();
    EXPECT_EQ(1u, partial_frame_count);

    // Frame is not complete, even after decoding partially.
    EXPECT_FALSE(decoder->FrameIsReceivedAtIndex(0));
    auto* frame = decoder->DecodeFrameBufferAtIndex(0);
    ASSERT_TRUE(frame);
    EXPECT_NE(ImageFrame::kFrameComplete, frame->GetStatus());
    EXPECT_FALSE(decoder->FrameIsReceivedAtIndex(0));

    decoder->SetData(SharedBuffer::Create(full_data), true);

    // With full data, parsing the size still does not mark a frame as
    // complete for animated images.
    EXPECT_TRUE(decoder->IsSizeAvailable());
    if (rec.expected_frame_count > 1) {
      EXPECT_FALSE(decoder->FrameIsReceivedAtIndex(0));
    } else {
      EXPECT_TRUE(decoder->FrameIsReceivedAtIndex(0));
    }

    const auto frame_count = decoder->FrameCount();
    ASSERT_EQ(rec.expected_frame_count, frame_count);

    // After parsing (the full file), all frames are complete.
    for (size_t i = 0; i < frame_count; ++i) {
      EXPECT_TRUE(decoder->FrameIsReceivedAtIndex(i));
    }

    frame = decoder->DecodeFrameBufferAtIndex(0);
    ASSERT_TRUE(frame);
    EXPECT_EQ(ImageFrame::kFrameComplete, frame->GetStatus());
    EXPECT_TRUE(decoder->FrameIsReceivedAtIndex(0));
  }
}

TEST(PNGTests, sizeMayOverflow) {
  auto decoder =
      CreatePNGDecoderWithPngData("/images/resources/crbug702934.png");
  EXPECT_FALSE(decoder->IsSizeAvailable());
  EXPECT_TRUE(decoder->Failed());
}

TEST(PNGTests, truncated) {
  auto decoder =
      CreatePNGDecoderWithPngData("/images/resources/crbug807324.png");

  // An update to libpng (without using the libpng-provided workaround)
  // resulted in truncating this image. It has no transparency, so no pixel
  // should be transparent.
  auto* frame = decoder->DecodeFrameBufferAtIndex(0);
  auto size = decoder->Size();
  for (int i = 0; i < size.width(); ++i) {
    for (int j = 0; j < size.height(); ++j) {
      ASSERT_NE(SK_ColorTRANSPARENT, *frame->GetAddr(i, j));
    }
  }
}

TEST(PNGTests, crbug827754) {
  const char* png_file = "/images/resources/crbug827754.png";
  scoped_refptr<SharedBuffer> data = ReadFileToSharedBuffer(png_file);
  ASSERT_TRUE(data);

  auto decoder = CreatePNGDecoder();
  decoder->SetData(data.get(), true);
  auto* frame = decoder->DecodeFrameBufferAtIndex(0);
  ASSERT_TRUE(frame);
  ASSERT_FALSE(decoder->Failed());
}

TEST(PNGTests, cicp) {
  const char* png_file = "/images/resources/cicp_pq.png";
  scoped_refptr<SharedBuffer> data = ReadFileToSharedBuffer(png_file);
  ASSERT_TRUE(data);

  auto decoder = CreatePNGDecoder();
  decoder->SetData(data.get(), true);
  auto* frame = decoder->DecodeFrameBufferAtIndex(0);
  ASSERT_TRUE(frame);
  ASSERT_FALSE(decoder->Failed());
  ASSERT_TRUE(decoder->HasEmbeddedColorProfile());
  ColorProfileTransform* transform = decoder->ColorTransform();

  const skcms_ICCProfile* png_profile = transform->SrcProfile();
  EXPECT_TRUE(skcms_TransferFunction_isPQish(&png_profile->trc[0].parametric));
}

TEST(PNGTests, HDRMetadata) {
  const char* png_file = "/images/resources/cicp_pq.png";
  scoped_refptr<SharedBuffer> data = ReadFileToSharedBuffer(png_file);
  ASSERT_TRUE(data);

  auto decoder = CreatePNGDecoder();
  decoder->SetData(data.get(), true);
  auto* frame = decoder->DecodeFrameBufferAtIndex(0);
  ASSERT_TRUE(frame);
  ASSERT_FALSE(decoder->Failed());
  const std::optional<gfx::HDRMetadata> hdr_metadata =
      decoder->GetHDRMetadata();
  ASSERT_TRUE(hdr_metadata);

  ASSERT_TRUE(hdr_metadata->cta_861_3);
  EXPECT_EQ(hdr_metadata->cta_861_3->max_content_light_level, 4000u);
  EXPECT_EQ(hdr_metadata->cta_861_3->max_frame_average_light_level, 2627u);

  ASSERT_TRUE(hdr_metadata->smpte_st_2086);
  EXPECT_FLOAT_EQ(hdr_metadata->smpte_st_2086->primaries.fRX, .680f);
  EXPECT_FLOAT_EQ(hdr_metadata->smpte_st_2086->primaries.fRY, .320f);
  EXPECT_FLOAT_EQ(hdr_metadata->smpte_st_2086->primaries.fGX, .265f);
  EXPECT_FLOAT_EQ(hdr_metadata->smpte_st_2086->primaries.fGY, .690f);
  EXPECT_FLOAT_EQ(hdr_metadata->smpte_st_2086->primaries.fBX, .150f);
  EXPECT_FLOAT_EQ(hdr_metadata->smpte_st_2086->primaries.fBY, .060f);
  EXPECT_FLOAT_EQ(hdr_metadata->smpte_st_2086->primaries.fWX, .3127f);
  EXPECT_FLOAT_EQ(hdr_metadata->smpte_st_2086->primaries.fWY, .3290f);
  EXPECT_FLOAT_EQ(hdr_metadata->smpte_st_2086->luminance_max, 5000.f);
  EXPECT_FLOAT_EQ(hdr_metadata->smpte_st_2086->luminance_min, .01f);
}

TEST(AnimatedPNGTests, TrnsMeansAlpha) {
  const char* png_file =
      "/images/resources/"
      "png-animated-idat-part-of-animation.png";
  auto decoder = CreatePNGDecoderWithPngData(png_file);
  auto* frame = decoder->DecodeFrameBufferAtIndex(0);
  ASSERT_TRUE(frame->HasAlpha());
}

TEST(PNGTests, CriticalPrivateChunkBeforeIHDR) {
  auto decoder = CreatePNGDecoder();
  scoped_refptr<SharedBuffer> data = ReadFileToSharedBuffer(
      kDecodersTestingDir, "private-critical-chunk-before-ihdr.png");
  EXPECT_FALSE(data->empty());
  decoder->SetData(data.get(), true);
  EXPECT_FALSE(decoder->IsSizeAvailable());
  EXPECT_TRUE(decoder->Failed());
}

}  // namespace blink
