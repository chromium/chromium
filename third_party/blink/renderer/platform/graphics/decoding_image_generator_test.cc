// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/decoding_image_generator.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder_test_helpers.h"
#include "third_party/blink/renderer/platform/image-decoders/segment_reader.h"

namespace blink {

namespace {

constexpr unsigned kTooShortForSignature = 5;

scoped_refptr<SegmentReader> CreateSegmentReader(char* reference_data,
                                                 size_t data_length) {
  PrepareReferenceData(reference_data, data_length);
  scoped_refptr<SharedBuffer> data = SharedBuffer::Create();
  data->Append(reference_data, data_length);
  return SegmentReader::CreateFromSharedBuffer(std::move(data));
}

}  // namespace

class DecodingImageGeneratorTest : public testing::Test {};

TEST_F(DecodingImageGeneratorTest, Create) {
  scoped_refptr<SharedBuffer> reference_data =
      ReadFileToSharedBuffer(kDecodersTestingDir, "radient.gif");
  scoped_refptr<SegmentReader> reader =
      SegmentReader::CreateFromSharedBuffer(std::move(reference_data));
  std::unique_ptr<SkImageGenerator> generator =
      DecodingImageGenerator::CreateAsSkImageGenerator(reader->GetAsSkData());
  // Sanity-check the image to make sure it was loaded.
  EXPECT_EQ(generator->getInfo().width(), 32);
  EXPECT_EQ(generator->getInfo().height(), 32);
}

TEST_F(DecodingImageGeneratorTest, CreateWithNoSize) {
  // Construct dummy image data that produces no valid size from the
  // ImageDecoder.
  char reference_data[kDefaultTestSize];
  EXPECT_EQ(nullptr, DecodingImageGenerator::CreateAsSkImageGenerator(
                         CreateSegmentReader(reference_data, kDefaultTestSize)
                             ->GetAsSkData()));
}

TEST_F(DecodingImageGeneratorTest, CreateWithNullImageDecoder) {
  // Construct dummy image data that will produce a null image decoder
  // due to data being too short for a signature.
  char reference_data[kTooShortForSignature];
  EXPECT_EQ(nullptr,
            DecodingImageGenerator::CreateAsSkImageGenerator(
                CreateSegmentReader(reference_data, kTooShortForSignature)
                    ->GetAsSkData()));
}

// This is a regression test for crbug.com/341812566 and passes if it does not
// crash under ASAN.
TEST_F(DecodingImageGeneratorTest, AdjustedGetPixels) {
  scoped_refptr<SharedBuffer> reference_data =
      ReadFileToSharedBuffer(kDecodersTestingDir, "radient.gif");
  scoped_refptr<SegmentReader> reader =
      SegmentReader::CreateFromSharedBuffer(std::move(reference_data));
  std::unique_ptr<SkImageGenerator> generator =
      DecodingImageGenerator::CreateAsSkImageGenerator(reader->GetAsSkData());
  SkImageInfo info = SkImageInfo::MakeA8(32, 32);
  std::vector<size_t> memory(info.computeMinByteSize());
  EXPECT_TRUE(generator->getPixels(info, memory.data(), info.minRowBytes()));
}

// TODO(wkorman): Test Create with a null ImageFrameGenerator. We'd
// need a way to intercept construction of the instance (and could do
// same for ImageDecoder above to reduce fragility of knowing a short
// signature will produce a null ImageDecoder). Note that it's not
// clear that it's possible to end up with a null ImageFrameGenerator,
// so maybe we can just remove that check from
// DecodingImageGenerator::Create.

}  // namespace blink
