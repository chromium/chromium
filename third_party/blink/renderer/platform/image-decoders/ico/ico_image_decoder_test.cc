// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/image-decoders/ico/ico_image_decoder.h"

#include <memory>
#include "base/files/file_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder_base_test.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder_test_helpers.h"

namespace blink {

namespace {

std::unique_ptr<ImageDecoder> CreateICODecoder() {
  return std::make_unique<ICOImageDecoder>(
      ImageDecoder::kAlphaNotPremultiplied, ColorBehavior::kTransformToSRGB,
      ImageDecoder::kNoDecodedImageByteLimit);
}
}  // namespace

TEST(ICOImageDecoderTests, trunctedIco) {
  const Vector<char> data = ReadFile("/images/resources/png-in-ico.ico");
  ASSERT_FALSE(data.empty());

  scoped_refptr<SharedBuffer> truncated_data =
      SharedBuffer::Create(data.data(), data.size() / 2);
  auto decoder = CreateICODecoder();

  decoder->SetData(truncated_data.get(), false);
  decoder->DecodeFrameBufferAtIndex(0);
  EXPECT_FALSE(decoder->Failed());

  decoder->SetData(truncated_data.get(), true);
  decoder->DecodeFrameBufferAtIndex(0);
  EXPECT_TRUE(decoder->Failed());
}

TEST(ICOImageDecoderTests, errorInPngInIco) {
  const Vector<char> data = ReadFile("/images/resources/png-in-ico.ico");
  ASSERT_FALSE(data.empty());

  // Modify the file to have a broken CRC in IHDR.
  constexpr size_t kCrcOffset = 22 + 29;
  constexpr size_t kCrcSize = 4;
  scoped_refptr<SharedBuffer> modified_data =
      SharedBuffer::Create(data.data(), kCrcOffset);
  Vector<char> bad_crc(kCrcSize, 0);
  modified_data->Append(bad_crc);
  modified_data->Append(data.data() + kCrcOffset + kCrcSize,
                        data.size() - kCrcOffset - kCrcSize);

  auto decoder = CreateICODecoder();
  decoder->SetData(modified_data.get(), true);

  // ICOImageDecoder reports the frame count based on whether enough data has
  // been received according to the icon directory. So even though the
  // embedded PNG is broken, there is enough data to include it in the frame
  // count.
  EXPECT_EQ(1u, decoder->FrameCount());

  decoder->DecodeFrameBufferAtIndex(0);
  EXPECT_TRUE(decoder->Failed());
}

TEST(ICOImageDecoderTests, parseAndDecodeByteByByte) {
  TestByteByByteDecode(&CreateICODecoder, "/images/resources/png-in-ico.ico",
                       1u, kAnimationNone);
  TestByteByByteDecode(&CreateICODecoder, "/images/resources/2entries.ico", 2u,
                       kAnimationNone);
  TestByteByByteDecode(&CreateICODecoder,
                       "/images/resources/greenbox-3frames.cur", 3u,
                       kAnimationNone);
  TestByteByByteDecode(&CreateICODecoder,
                       "/images/resources/icon-without-and-bitmap.ico", 1u,
                       kAnimationNone);
  TestByteByByteDecode(&CreateICODecoder, "/images/resources/1bit.ico", 1u,
                       kAnimationNone);
  TestByteByByteDecode(&CreateICODecoder, "/images/resources/bug653075.ico", 2u,
                       kAnimationNone);
}

TEST(ICOImageDecoderTests, NullData) {
  static constexpr size_t kSizeOfBadBlock = 6 + 16 + 1;

  Vector<char> ico_file_data = ReadFile("/images/resources/png-in-ico.ico");
  ASSERT_LT(kSizeOfBadBlock, ico_file_data.size());

  scoped_refptr<SharedBuffer> truncated_data =
      SharedBuffer::Create(ico_file_data.data(), kSizeOfBadBlock);
  auto decoder = CreateICODecoder();

  decoder->SetData(truncated_data.get(), false);
  decoder->SetMemoryAllocator(nullptr);
  EXPECT_FALSE(decoder->Failed());

  auto* frame = decoder->DecodeFrameBufferAtIndex(0);
  EXPECT_EQ(nullptr, frame);

  decoder->SetData(scoped_refptr<SegmentReader>(nullptr), false);
  decoder->ClearCacheExceptFrame(0);
  decoder->SetMemoryAllocator(nullptr);
  EXPECT_FALSE(decoder->Failed());
}

class ICOImageDecoderCorpusTest : public ImageDecoderBaseTest {
 public:
  ICOImageDecoderCorpusTest() : ImageDecoderBaseTest("ico") {}

 protected:
  std::unique_ptr<ImageDecoder> CreateImageDecoder() const override {
    return std::make_unique<ICOImageDecoder>(
        ImageDecoder::kAlphaPremultiplied, ColorBehavior::kTransformToSRGB,
        ImageDecoder::kNoDecodedImageByteLimit);
  }
};

TEST_F(ICOImageDecoderCorpusTest, Decoding) {
  TestDecoding();
}

TEST_F(ICOImageDecoderCorpusTest, ImageNonZeroFrameIndex) {
  // Test that the decoder decodes multiple sizes of icons which have them.
  // Load an icon that has both favicon-size and larger entries.
  base::FilePath multisize_icon_path(data_dir().AppendASCII("yahoo.ico"));

  // data_dir may not exist without src_internal checkouts.
  if (!base::PathExists(multisize_icon_path)) {
    return;
  }
  const base::FilePath md5_sum_path(GetMD5SumPath(multisize_icon_path).value() +
                                    FILE_PATH_LITERAL("2"));
  static const int kDesiredFrameIndex = 3;
  TestImageDecoder(multisize_icon_path, md5_sum_path, kDesiredFrameIndex);
}

}  // namespace blink
