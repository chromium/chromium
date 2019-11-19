// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/image-decoders/bmp/bmp_image_decoder.h"

#include <memory>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder_base_test.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"

namespace blink {

namespace {

std::unique_ptr<ImageDecoder> CreateBMPDecoder() {
  return std::make_unique<BMPImageDecoder>(
      ImageDecoder::kAlphaNotPremultiplied, ColorBehavior::TransformToSRGB(),
      ImageDecoder::kNoDecodedImageByteLimit);
}

}  // anonymous namespace

TEST(BMPImageDecoderTest, isSizeAvailable) {
  static constexpr char kBmpFile[] = "/images/resources/lenna.bmp";  // 256x256
  scoped_refptr<SharedBuffer> data = ReadFile(kBmpFile);
  ASSERT_TRUE(data.get());

  std::unique_ptr<ImageDecoder> decoder = CreateBMPDecoder();
  decoder->SetData(data.get(), true);
  EXPECT_TRUE(decoder->IsSizeAvailable());
  EXPECT_EQ(256, decoder->Size().Width());
  EXPECT_EQ(256, decoder->Size().Height());
}

TEST(BMPImageDecoderTest, parseAndDecode) {
  static constexpr char kBmpFile[] = "/images/resources/lenna.bmp";  // 256x256
  scoped_refptr<SharedBuffer> data = ReadFile(kBmpFile);
  ASSERT_TRUE(data.get());

  std::unique_ptr<ImageDecoder> decoder = CreateBMPDecoder();
  decoder->SetData(data.get(), true);

  ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(0);
  ASSERT_TRUE(frame);
  EXPECT_EQ(ImageFrame::kFrameComplete, frame->GetStatus());
  EXPECT_EQ(256, frame->Bitmap().width());
  EXPECT_EQ(256, frame->Bitmap().height());
  EXPECT_FALSE(decoder->Failed());
}

// Test if a BMP decoder returns a proper error while decoding an empty image.
TEST(BMPImageDecoderTest, emptyImage) {
  static constexpr char kBmpFile[] = "/images/resources/0x0.bmp";  // 0x0
  scoped_refptr<SharedBuffer> data = ReadFile(kBmpFile);
  ASSERT_TRUE(data.get());

  std::unique_ptr<ImageDecoder> decoder = CreateBMPDecoder();
  decoder->SetData(data.get(), true);

  ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(0);
  ASSERT_TRUE(frame);
  EXPECT_EQ(ImageFrame::kFrameEmpty, frame->GetStatus());
  EXPECT_TRUE(decoder->Failed());
}

TEST(BMPImageDecoderTest, int32MinHeight) {
  static constexpr char kBmpFile[] =
      "/images/resources/1xint32_min.bmp";  // 0xINT32_MIN
  scoped_refptr<SharedBuffer> data = ReadFile(kBmpFile);
  std::unique_ptr<ImageDecoder> decoder = CreateBMPDecoder();
  // Test when not all data is received.
  decoder->SetData(data.get(), false);
  EXPECT_FALSE(decoder->IsSizeAvailable());
  EXPECT_TRUE(decoder->Failed());
}

// This test verifies that calling SharedBuffer::MergeSegmentsIntoBuffer() does
// not break BMP decoding at a critical point: in between a call to decode the
// size (when BMPImageDecoder stops while it may still have input data to
// read) and a call to do a full decode.
TEST(BMPImageDecoderTest, mergeBuffer) {
  static constexpr char kBmpFile[] = "/images/resources/lenna.bmp";
  TestMergeBuffer(&CreateBMPDecoder, kBmpFile);
}

// Verify that decoding this image does not crash.
TEST(BMPImageDecoderTest, crbug752898) {
  static constexpr char kBmpFile[] = "/images/resources/crbug752898.bmp";
  scoped_refptr<SharedBuffer> data = ReadFile(kBmpFile);
  ASSERT_TRUE(data.get());

  std::unique_ptr<ImageDecoder> decoder = CreateBMPDecoder();
  decoder->SetData(data.get(), true);
  decoder->DecodeFrameBufferAtIndex(0);
}

class BMPImageDecoderCorpusTest : public ImageDecoderBaseTest {
 public:
  BMPImageDecoderCorpusTest() : ImageDecoderBaseTest("bmp") {}

 protected:
  std::unique_ptr<ImageDecoder> CreateImageDecoder() const override {
    return std::make_unique<BMPImageDecoder>(
        ImageDecoder::kAlphaPremultiplied, ColorBehavior::TransformToSRGB(),
        ImageDecoder::kNoDecodedImageByteLimit);
  }

  // The BMPImageDecoderCorpusTest tests are really slow under Valgrind.
  // Thus it is split into fast and slow versions. The threshold is
  // set to 10KB because the fast test can finish under Valgrind in
  // less than 30 seconds.
  static const int64_t kThresholdSize = 10240;
};

TEST_F(BMPImageDecoderCorpusTest, DecodingFast) {
  TestDecoding(FileSelection::kSmaller, kThresholdSize);
}

#if defined(THREAD_SANITIZER)
// BMPImageDecoderCorpusTest.DecodingSlow always times out under ThreadSanitizer
// v2.
#define MAYBE_DecodingSlow DISABLED_DecodingSlow
#else
#define MAYBE_DecodingSlow DecodingSlow
#endif
TEST_F(BMPImageDecoderCorpusTest, MAYBE_DecodingSlow) {
  TestDecoding(FileSelection::kBigger, kThresholdSize);
}

}  // namespace blink
