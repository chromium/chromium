// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/image-decoders/image_decoder_test_helpers.h"

#include <memory>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/image-decoders/image_frame.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hasher.h"

namespace blink {

scoped_refptr<SharedBuffer> ReadFile(StringView file_name) {
  StringBuilder file_path;
  file_path.Append(test::BlinkWebTestsDir());
  file_path.Append(file_name);
  return test::ReadFromFile(file_path.ToString());
}

scoped_refptr<SharedBuffer> ReadFile(const char* dir, const char* file_name) {
  StringBuilder file_path;
  if (strncmp(dir, "web_tests/", 10) == 0) {
    file_path.Append(test::BlinkWebTestsDir());
    file_path.Append('/');
    file_path.Append(dir + 10);
  } else {
    file_path.Append(test::BlinkRootDir());
    file_path.Append('/');
    file_path.Append(dir);
  }
  file_path.Append('/');
  file_path.Append(file_name);
  return test::ReadFromFile(file_path.ToString());
}

unsigned HashBitmap(const SkBitmap& bitmap) {
  return StringHasher::HashMemory(bitmap.getPixels(), bitmap.computeByteSize());
}

static unsigned CreateDecodingBaseline(DecoderCreator create_decoder,
                                       SharedBuffer* data) {
  std::unique_ptr<ImageDecoder> decoder = create_decoder();
  decoder->SetData(data, true);
  ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(0);
  return HashBitmap(frame->Bitmap());
}

void CreateDecodingBaseline(DecoderCreator create_decoder,
                            SharedBuffer* data,
                            Vector<unsigned>* baseline_hashes) {
  std::unique_ptr<ImageDecoder> decoder = create_decoder();
  decoder->SetData(data, true);
  size_t frame_count = decoder->FrameCount();
  for (size_t i = 0; i < frame_count; ++i) {
    ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(i);
    baseline_hashes->push_back(HashBitmap(frame->Bitmap()));
  }
}

void TestByteByByteDecode(DecoderCreator create_decoder,
                          SharedBuffer* shared_data,
                          size_t expected_frame_count,
                          int expected_repetition_count) {
  const Vector<char> data = shared_data->CopyAs<Vector<char>>();

  Vector<unsigned> baseline_hashes;
  CreateDecodingBaseline(create_decoder, shared_data, &baseline_hashes);

  std::unique_ptr<ImageDecoder> decoder = create_decoder();

  size_t frame_count = 0;
  size_t frames_decoded = 0;

  // Pass data to decoder byte by byte.
  scoped_refptr<SharedBuffer> source_data[2] = {SharedBuffer::Create(),
                                                SharedBuffer::Create()};
  const char* source = data.data();

  for (size_t length = 1; length <= data.size() && !decoder->Failed();
       ++length) {
    source_data[0]->Append(source, 1u);
    source_data[1]->Append(source++, 1u);
    // Alternate the buffers to cover the JPEGImageDecoder::OnSetData restart
    // code.
    decoder->SetData(source_data[length & 1].get(), length == data.size());

    EXPECT_LE(frame_count, decoder->FrameCount());
    frame_count = decoder->FrameCount();

    if (!decoder->IsSizeAvailable())
      continue;

    for (size_t i = frames_decoded; i < frame_count; ++i) {
      // In ICOImageDecoder memory layout could differ from frame order.
      // E.g. memory layout could be |<frame1><frame0>| and frame_count
      // would return 1 until receiving full file.
      // When file is completely received frame_count would return 2 and
      // only then both frames could be completely decoded.
      ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(i);
      if (frame && frame->GetStatus() == ImageFrame::kFrameComplete)
        ++frames_decoded;
    }
  }

  EXPECT_FALSE(decoder->Failed());
  EXPECT_EQ(expected_frame_count, decoder->FrameCount());
  EXPECT_EQ(expected_frame_count, frames_decoded);
  EXPECT_EQ(expected_repetition_count, decoder->RepetitionCount());

  ASSERT_EQ(expected_frame_count, baseline_hashes.size());
  for (size_t i = 0; i < decoder->FrameCount(); i++) {
    ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(i);
    EXPECT_EQ(baseline_hashes[i], HashBitmap(frame->Bitmap()));
  }
}

// This test verifies that calling SharedBuffer::MergeSegmentsIntoBuffer() does
// not break decoding at a critical point: in between a call to decode the size
// (when the decoder stops while it may still have input data to read) and a
// call to do a full decode.
static void TestMergeBuffer(DecoderCreator create_decoder,
                            SharedBuffer* shared_data) {
  const unsigned hash = CreateDecodingBaseline(create_decoder, shared_data);
  const Vector<char> data = shared_data->CopyAs<Vector<char>>();

  // In order to do any verification, this test needs to move the data owned
  // by the SharedBuffer. A way to guarantee that is to create a new one, and
  // then append a string of characters greater than kSegmentSize. This
  // results in writing the data into a segment, skipping the internal
  // contiguous buffer.
  scoped_refptr<SharedBuffer> segmented_data = SharedBuffer::Create();
  segmented_data->Append(data.data(), data.size());

  std::unique_ptr<ImageDecoder> decoder = create_decoder();
  decoder->SetData(segmented_data.get(), true);

  ASSERT_TRUE(decoder->IsSizeAvailable());

  // This will call SharedBuffer::MergeSegmentsIntoBuffer, copying all
  // segments into the contiguous buffer. If the ImageDecoder was pointing to
  // data in a segment, its pointer would no longer be valid.
  segmented_data->Data();

  ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(0);
  ASSERT_FALSE(decoder->Failed());
  EXPECT_EQ(frame->GetStatus(), ImageFrame::kFrameComplete);
  EXPECT_EQ(HashBitmap(frame->Bitmap()), hash);
}

static void TestRandomFrameDecode(DecoderCreator create_decoder,
                                  SharedBuffer* full_data,
                                  size_t skipping_step) {
  Vector<unsigned> baseline_hashes;
  CreateDecodingBaseline(create_decoder, full_data, &baseline_hashes);
  size_t frame_count = baseline_hashes.size();

  // Random decoding should get the same results as sequential decoding.
  std::unique_ptr<ImageDecoder> decoder = create_decoder();
  decoder->SetData(full_data, true);
  for (size_t i = 0; i < skipping_step; ++i) {
    for (size_t j = i; j < frame_count; j += skipping_step) {
      SCOPED_TRACE(testing::Message() << "Random i:" << i << " j:" << j);
      ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(j);
      EXPECT_EQ(baseline_hashes[j], HashBitmap(frame->Bitmap()));
    }
  }

  // Decoding in reverse order.
  decoder = create_decoder();
  decoder->SetData(full_data, true);
  for (size_t i = frame_count; i; --i) {
    SCOPED_TRACE(testing::Message() << "Reverse i:" << i);
    ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(i - 1);
    EXPECT_EQ(baseline_hashes[i - 1], HashBitmap(frame->Bitmap()));
  }
}

static void TestRandomDecodeAfterClearFrameBufferCache(
    DecoderCreator create_decoder,
    SharedBuffer* data,
    size_t skipping_step) {
  Vector<unsigned> baseline_hashes;
  CreateDecodingBaseline(create_decoder, data, &baseline_hashes);
  size_t frame_count = baseline_hashes.size();

  std::unique_ptr<ImageDecoder> decoder = create_decoder();
  decoder->SetData(data, true);
  for (size_t clear_except_frame = 0; clear_except_frame < frame_count;
       ++clear_except_frame) {
    decoder->ClearCacheExceptFrame(clear_except_frame);
    for (size_t i = 0; i < skipping_step; ++i) {
      for (size_t j = 0; j < frame_count; j += skipping_step) {
        SCOPED_TRACE(testing::Message() << "Random i:" << i << " j:" << j);
        ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(j);
        EXPECT_EQ(baseline_hashes[j], HashBitmap(frame->Bitmap()));
      }
    }
  }
}

static void TestDecodeAfterReallocatingData(DecoderCreator create_decoder,
                                            SharedBuffer* data) {
  std::unique_ptr<ImageDecoder> decoder = create_decoder();

  // Parse from 'data'.
  decoder->SetData(data, true);
  size_t frame_count = decoder->FrameCount();

  // ... and then decode frames from 'reallocated_data'.
  Vector<char> copy = data->CopyAs<Vector<char>>();
  scoped_refptr<SharedBuffer> reallocated_data =
      SharedBuffer::AdoptVector(copy);
  ASSERT_TRUE(reallocated_data.get());
  data->Clear();
  decoder->SetData(reallocated_data.get(), true);

  for (size_t i = 0; i < frame_count; ++i) {
    const ImageFrame* const frame = decoder->DecodeFrameBufferAtIndex(i);
    EXPECT_EQ(ImageFrame::kFrameComplete, frame->GetStatus());
  }
}

static void TestByteByByteSizeAvailable(DecoderCreator create_decoder,
                                        SharedBuffer* data,
                                        size_t frame_offset,
                                        bool has_color_space,
                                        int expected_repetition_count) {
  std::unique_ptr<ImageDecoder> decoder = create_decoder();
  EXPECT_LT(frame_offset, data->size());

  // Send data to the decoder byte-by-byte and use the provided frame offset in
  // the data to check that IsSizeAvailable() changes state only when that
  // offset is reached. Also check other decoder state.
  scoped_refptr<SharedBuffer> temp_data = SharedBuffer::Create();
  const Vector<char> source_buffer = data->CopyAs<Vector<char>>();
  const char* source = source_buffer.data();
  for (size_t length = 1; length <= frame_offset; ++length) {
    temp_data->Append(source++, 1u);
    decoder->SetData(temp_data.get(), false);

    if (length < frame_offset) {
      EXPECT_FALSE(decoder->IsSizeAvailable());
      EXPECT_TRUE(decoder->Size().IsEmpty());
      EXPECT_FALSE(decoder->HasEmbeddedColorProfile());
      EXPECT_EQ(0u, decoder->FrameCount());
      EXPECT_EQ(kAnimationLoopOnce, decoder->RepetitionCount());
      EXPECT_FALSE(decoder->DecodeFrameBufferAtIndex(0));
    } else {
      EXPECT_TRUE(decoder->IsSizeAvailable());
      EXPECT_FALSE(decoder->Size().IsEmpty());
      EXPECT_EQ(decoder->HasEmbeddedColorProfile(), has_color_space);
      EXPECT_EQ(1u, decoder->FrameCount());
      EXPECT_EQ(expected_repetition_count, decoder->RepetitionCount());
    }

    ASSERT_FALSE(decoder->Failed());
  }
}

static void TestProgressiveDecoding(DecoderCreator create_decoder,
                                    SharedBuffer* full_buffer,
                                    size_t increment) {
  const Vector<char> full_data = full_buffer->CopyAs<Vector<char>>();
  const size_t full_length = full_data.size();

  std::unique_ptr<ImageDecoder> decoder;

  Vector<unsigned> truncated_hashes;
  Vector<unsigned> progressive_hashes;

  // Compute hashes when the file is truncated.
  scoped_refptr<SharedBuffer> data = SharedBuffer::Create();
  const char* source = full_data.data();
  for (size_t i = 1; i <= full_length; i += increment) {
    decoder = create_decoder();
    data->Append(source++, 1u);
    decoder->SetData(data.get(), i == full_length);
    ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(0);
    if (!frame) {
      truncated_hashes.push_back(0);
      continue;
    }
    truncated_hashes.push_back(HashBitmap(frame->Bitmap()));
  }

  // Compute hashes when the file is progressively decoded.
  decoder = create_decoder();
  data = SharedBuffer::Create();
  source = full_data.data();
  for (size_t i = 1; i <= full_length; i += increment) {
    data->Append(source++, 1u);
    decoder->SetData(data.get(), i == full_length);
    ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(0);
    if (!frame) {
      progressive_hashes.push_back(0);
      continue;
    }
    progressive_hashes.push_back(HashBitmap(frame->Bitmap()));
  }

  for (size_t i = 0; i < truncated_hashes.size(); ++i)
    ASSERT_EQ(truncated_hashes[i], progressive_hashes[i]);
}

void TestUpdateRequiredPreviousFrameAfterFirstDecode(
    DecoderCreator create_decoder,
    SharedBuffer* full_buffer) {
  const Vector<char> full_data = full_buffer->CopyAs<Vector<char>>();
  std::unique_ptr<ImageDecoder> decoder = create_decoder();

  // Give it data that is enough to parse but not decode in order to check the
  // status of RequiredPreviousFrameIndex before decoding.
  scoped_refptr<SharedBuffer> data = SharedBuffer::Create();
  const char* source = full_data.data();
  do {
    data->Append(source++, 1u);
    decoder->SetData(data.get(), false);
  } while (!decoder->FrameCount() ||
           decoder->DecodeFrameBufferAtIndex(0)->GetStatus() ==
               ImageFrame::kFrameEmpty);

  EXPECT_EQ(kNotFound,
            decoder->DecodeFrameBufferAtIndex(0)->RequiredPreviousFrameIndex());
  unsigned frame_count = decoder->FrameCount();
  for (size_t i = 1; i < frame_count; ++i) {
    EXPECT_EQ(
        i - 1,
        decoder->DecodeFrameBufferAtIndex(i)->RequiredPreviousFrameIndex());
  }

  decoder->SetData(full_buffer, true);
  for (size_t i = 0; i < frame_count; ++i) {
    EXPECT_EQ(
        kNotFound,
        decoder->DecodeFrameBufferAtIndex(i)->RequiredPreviousFrameIndex());
  }
}

void TestResumePartialDecodeAfterClearFrameBufferCache(
    DecoderCreator create_decoder,
    SharedBuffer* full_buffer) {
  const Vector<char> full_data = full_buffer->CopyAs<Vector<char>>();
  Vector<unsigned> baseline_hashes;
  CreateDecodingBaseline(create_decoder, full_buffer, &baseline_hashes);
  size_t frame_count = baseline_hashes.size();

  std::unique_ptr<ImageDecoder> decoder = create_decoder();

  // Let frame 0 be partially decoded.
  scoped_refptr<SharedBuffer> data = SharedBuffer::Create();
  const char* source = full_data.data();
  do {
    data->Append(source++, 1u);
    decoder->SetData(data.get(), false);
  } while (!decoder->FrameCount() ||
           decoder->DecodeFrameBufferAtIndex(0)->GetStatus() ==
               ImageFrame::kFrameEmpty);

  // Skip to the last frame and clear.
  decoder->SetData(full_buffer, true);
  EXPECT_EQ(frame_count, decoder->FrameCount());
  ImageFrame* last_frame = decoder->DecodeFrameBufferAtIndex(frame_count - 1);
  EXPECT_EQ(baseline_hashes[frame_count - 1], HashBitmap(last_frame->Bitmap()));
  decoder->ClearCacheExceptFrame(kNotFound);

  // Resume decoding of the first frame.
  ImageFrame* first_frame = decoder->DecodeFrameBufferAtIndex(0);
  EXPECT_EQ(ImageFrame::kFrameComplete, first_frame->GetStatus());
  EXPECT_EQ(baseline_hashes[0], HashBitmap(first_frame->Bitmap()));
}

void TestByteByByteDecode(DecoderCreator create_decoder,
                          const char* file,
                          size_t expected_frame_count,
                          int expected_repetition_count) {
  SCOPED_TRACE(file);
  scoped_refptr<SharedBuffer> data = ReadFile(file);
  ASSERT_TRUE(data.get());
  TestByteByByteDecode(create_decoder, data.get(), expected_frame_count,
                       expected_repetition_count);
}
void TestByteByByteDecode(DecoderCreator create_decoder,
                          const char* dir,
                          const char* file,
                          size_t expected_frame_count,
                          int expected_repetition_count) {
  SCOPED_TRACE(file);
  scoped_refptr<SharedBuffer> data = ReadFile(dir, file);
  ASSERT_TRUE(data.get());
  TestByteByByteDecode(create_decoder, data.get(), expected_frame_count,
                       expected_repetition_count);
}

void TestMergeBuffer(DecoderCreator create_decoder, const char* file) {
  scoped_refptr<SharedBuffer> data = ReadFile(file);
  ASSERT_TRUE(data.get());
  TestMergeBuffer(create_decoder, data.get());
}

void TestMergeBuffer(DecoderCreator create_decoder,
                     const char* dir,
                     const char* file) {
  scoped_refptr<SharedBuffer> data = ReadFile(dir, file);
  ASSERT_TRUE(data.get());
  TestMergeBuffer(create_decoder, data.get());
}

void TestRandomFrameDecode(DecoderCreator create_decoder,
                           const char* file,
                           size_t skipping_step) {
  scoped_refptr<SharedBuffer> data = ReadFile(file);
  ASSERT_TRUE(data.get());
  SCOPED_TRACE(file);
  TestRandomFrameDecode(create_decoder, data.get(), skipping_step);
}
void TestRandomFrameDecode(DecoderCreator create_decoder,
                           const char* dir,
                           const char* file,
                           size_t skipping_step) {
  scoped_refptr<SharedBuffer> data = ReadFile(dir, file);
  ASSERT_TRUE(data.get());
  SCOPED_TRACE(file);
  TestRandomFrameDecode(create_decoder, data.get(), skipping_step);
}

void TestRandomDecodeAfterClearFrameBufferCache(DecoderCreator create_decoder,
                                                const char* file,
                                                size_t skipping_step) {
  scoped_refptr<SharedBuffer> data = ReadFile(file);
  ASSERT_TRUE(data.get());
  SCOPED_TRACE(file);
  TestRandomDecodeAfterClearFrameBufferCache(create_decoder, data.get(),
                                             skipping_step);
}

void TestRandomDecodeAfterClearFrameBufferCache(DecoderCreator create_decoder,
                                                const char* dir,
                                                const char* file,
                                                size_t skipping_step) {
  scoped_refptr<SharedBuffer> data = ReadFile(dir, file);
  ASSERT_TRUE(data.get());
  SCOPED_TRACE(file);
  TestRandomDecodeAfterClearFrameBufferCache(create_decoder, data.get(),
                                             skipping_step);
}

void TestDecodeAfterReallocatingData(DecoderCreator create_decoder,
                                     const char* file) {
  scoped_refptr<SharedBuffer> data = ReadFile(file);
  ASSERT_TRUE(data.get());
  TestDecodeAfterReallocatingData(create_decoder, data.get());
}

void TestDecodeAfterReallocatingData(DecoderCreator create_decoder,
                                     const char* dir,
                                     const char* file) {
  scoped_refptr<SharedBuffer> data = ReadFile(dir, file);
  ASSERT_TRUE(data.get());
  TestDecodeAfterReallocatingData(create_decoder, data.get());
}

void TestByteByByteSizeAvailable(DecoderCreator create_decoder,
                                 const char* file,
                                 size_t frame_offset,
                                 bool has_color_space,
                                 int expected_repetition_count) {
  scoped_refptr<SharedBuffer> data = ReadFile(file);
  ASSERT_TRUE(data.get());
  TestByteByByteSizeAvailable(create_decoder, data.get(), frame_offset,
                              has_color_space, expected_repetition_count);
}

void TestByteByByteSizeAvailable(DecoderCreator create_decoder,
                                 const char* dir,
                                 const char* file,
                                 size_t frame_offset,
                                 bool has_color_space,
                                 int expected_repetition_count) {
  scoped_refptr<SharedBuffer> data = ReadFile(dir, file);
  ASSERT_TRUE(data.get());
  TestByteByByteSizeAvailable(create_decoder, data.get(), frame_offset,
                              has_color_space, expected_repetition_count);
}

void TestProgressiveDecoding(DecoderCreator create_decoder,
                             const char* file,
                             size_t increment) {
  scoped_refptr<SharedBuffer> data = ReadFile(file);
  ASSERT_TRUE(data.get());
  TestProgressiveDecoding(create_decoder, data.get(), increment);
}

void TestProgressiveDecoding(DecoderCreator create_decoder,
                             const char* dir,
                             const char* file,
                             size_t increment) {
  scoped_refptr<SharedBuffer> data = ReadFile(dir, file);
  ASSERT_TRUE(data.get());
  TestProgressiveDecoding(create_decoder, data.get(), increment);
}

void TestUpdateRequiredPreviousFrameAfterFirstDecode(
    DecoderCreator create_decoder,
    const char* dir,
    const char* file) {
  scoped_refptr<SharedBuffer> data = ReadFile(dir, file);
  ASSERT_TRUE(data.get());
  TestUpdateRequiredPreviousFrameAfterFirstDecode(create_decoder, data.get());
}

void TestUpdateRequiredPreviousFrameAfterFirstDecode(
    DecoderCreator create_decoder,
    const char* file) {
  scoped_refptr<SharedBuffer> data = ReadFile(file);
  ASSERT_TRUE(data.get());
  TestUpdateRequiredPreviousFrameAfterFirstDecode(create_decoder, data.get());
}

void TestResumePartialDecodeAfterClearFrameBufferCache(
    DecoderCreator create_decoder,
    const char* dir,
    const char* file) {
  scoped_refptr<SharedBuffer> data = ReadFile(dir, file);
  ASSERT_TRUE(data.get());
  TestResumePartialDecodeAfterClearFrameBufferCache(create_decoder, data.get());
}

void TestResumePartialDecodeAfterClearFrameBufferCache(
    DecoderCreator create_decoder,
    const char* file) {
  scoped_refptr<SharedBuffer> data = ReadFile(file);
  ASSERT_TRUE(data.get());
  TestResumePartialDecodeAfterClearFrameBufferCache(create_decoder, data.get());
}

static uint32_t PremultiplyColor(uint32_t c) {
  return SkPremultiplyARGBInline(SkGetPackedA32(c), SkGetPackedR32(c),
                                 SkGetPackedG32(c), SkGetPackedB32(c));
}

static void VerifyFramesMatch(const char* file,
                              const ImageFrame* const a,
                              const ImageFrame* const b) {
  const SkBitmap& bitmap_a = a->Bitmap();
  const SkBitmap& bitmap_b = b->Bitmap();
  ASSERT_EQ(bitmap_a.width(), bitmap_b.width());
  ASSERT_EQ(bitmap_a.height(), bitmap_b.height());

  int max_difference = 0;
  for (int y = 0; y < bitmap_a.height(); ++y) {
    for (int x = 0; x < bitmap_a.width(); ++x) {
      uint32_t color_a = *bitmap_a.getAddr32(x, y);
      if (!a->PremultiplyAlpha())
        color_a = PremultiplyColor(color_a);
      uint32_t color_b = *bitmap_b.getAddr32(x, y);
      if (!b->PremultiplyAlpha())
        color_b = PremultiplyColor(color_b);
      uint8_t* pixel_a = reinterpret_cast<uint8_t*>(&color_a);
      uint8_t* pixel_b = reinterpret_cast<uint8_t*>(&color_b);
      for (int channel = 0; channel < 4; ++channel) {
        const int difference = abs(pixel_a[channel] - pixel_b[channel]);
        if (difference > max_difference)
          max_difference = difference;
      }
    }
  }

  // Pre-multiplication could round the RGBA channel values. So, we declare
  // that the frames match if the RGBA channel values differ by at most 2.
  EXPECT_GE(2, max_difference) << file;
}

// Verifies that result of alpha blending is similar for AlphaPremultiplied and
// AlphaNotPremultiplied cases.
void TestAlphaBlending(DecoderCreatorWithAlpha create_decoder,
                       const char* file) {
  scoped_refptr<SharedBuffer> data = ReadFile(file);
  ASSERT_TRUE(data.get());

  std::unique_ptr<ImageDecoder> decoder_a =
      create_decoder(ImageDecoder::kAlphaPremultiplied);
  decoder_a->SetData(data.get(), true);

  std::unique_ptr<ImageDecoder> decoder_b =
      create_decoder(ImageDecoder::kAlphaNotPremultiplied);
  decoder_b->SetData(data.get(), true);

  size_t frame_count = decoder_a->FrameCount();
  ASSERT_EQ(frame_count, decoder_b->FrameCount());

  for (size_t i = 0; i < frame_count; ++i) {
    VerifyFramesMatch(file, decoder_a->DecodeFrameBufferAtIndex(i),
                      decoder_b->DecodeFrameBufferAtIndex(i));
  }
}

}  // namespace blink
