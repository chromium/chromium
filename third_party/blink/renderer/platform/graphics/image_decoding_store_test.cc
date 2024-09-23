/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/graphics/image_decoding_store.h"

#include <memory>
#include "base/memory/memory_pressure_listener.h"
#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/graphics/image_frame_generator.h"
#include "third_party/blink/renderer/platform/graphics/test/mock_image_decoder.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

class ImageDecodingStoreTest : public testing::Test,
                               public MockImageDecoderClient {
 public:
  void SetUp() override {
    image_decoding_store_.SetCacheLimitInBytes(1024 * 1024);
    generator_ = ImageFrameGenerator::Create(SkISize::Make(100, 100), true,
                                             ColorBehavior::kIgnore,
                                             cc::AuxImage::kDefault, {});
    decoders_destroyed_ = 0;
  }

  void TearDown() override { image_decoding_store_.Clear(); }

  void DecoderBeingDestroyed() override { ++decoders_destroyed_; }

  void DecodeRequested() override {
    // Decoder is never used by ImageDecodingStore.
    ASSERT_TRUE(false);
  }

  ImageFrame::Status GetStatus(wtf_size_t index) override {
    return ImageFrame::kFramePartial;
  }

  wtf_size_t FrameCount() override { return 1; }
  int RepetitionCount() const override { return kAnimationNone; }
  base::TimeDelta FrameDuration() const override { return base::TimeDelta(); }

 protected:
  void EvictOneCache() {
    size_t memory_usage_in_bytes = image_decoding_store_.MemoryUsageInBytes();
    if (memory_usage_in_bytes)
      image_decoding_store_.SetCacheLimitInBytes(memory_usage_in_bytes - 1);
    else
      image_decoding_store_.SetCacheLimitInBytes(0);
  }

  test::TaskEnvironment task_environment_;
  ImageDecodingStore image_decoding_store_;
  scoped_refptr<ImageFrameGenerator> generator_;
  int decoders_destroyed_;
};

TEST_F(ImageDecodingStoreTest, insertDecoder) {
  const SkISize size = SkISize::Make(1, 1);
  auto decoder = std::make_unique<MockImageDecoder>(this);
  decoder->SetSize(1, 1);
  const ImageDecoder* ref_decoder = decoder.get();
  image_decoding_store_.InsertDecoder(generator_.get(),
                                      cc::PaintImage::kDefaultGeneratorClientId,
                                      std::move(decoder));
  EXPECT_EQ(1, image_decoding_store_.CacheEntries());
  EXPECT_EQ(4u, image_decoding_store_.MemoryUsageInBytes());

  ImageDecoder* test_decoder;
  EXPECT_TRUE(image_decoding_store_.LockDecoder(
      generator_.get(), size, ImageDecoder::kAlphaPremultiplied,
      cc::PaintImage::kDefaultGeneratorClientId, &test_decoder));
  EXPECT_TRUE(test_decoder);
  EXPECT_EQ(ref_decoder, test_decoder);
  image_decoding_store_.UnlockDecoder(generator_.get(),
                                      cc::PaintImage::kDefaultGeneratorClientId,
                                      test_decoder);
  EXPECT_EQ(1, image_decoding_store_.CacheEntries());
}

TEST_F(ImageDecodingStoreTest, evictDecoder) {
  auto decoder1 = std::make_unique<MockImageDecoder>(this);
  auto decoder2 = std::make_unique<MockImageDecoder>(this);
  auto decoder3 = std::make_unique<MockImageDecoder>(this);
  decoder1->SetSize(1, 1);
  decoder2->SetSize(2, 2);
  decoder3->SetSize(3, 3);
  image_decoding_store_.InsertDecoder(generator_.get(),
                                      cc::PaintImage::kDefaultGeneratorClientId,
                                      std::move(decoder1));
  image_decoding_store_.InsertDecoder(generator_.get(),
                                      cc::PaintImage::kDefaultGeneratorClientId,
                                      std::move(decoder2));
  image_decoding_store_.InsertDecoder(generator_.get(),
                                      cc::PaintImage::kDefaultGeneratorClientId,
                                      std::move(decoder3));
  EXPECT_EQ(3, image_decoding_store_.CacheEntries());
  EXPECT_EQ(56u, image_decoding_store_.MemoryUsageInBytes());

  EvictOneCache();
  EXPECT_EQ(2, image_decoding_store_.CacheEntries());
  EXPECT_EQ(52u, image_decoding_store_.MemoryUsageInBytes());

  EvictOneCache();
  EXPECT_EQ(1, image_decoding_store_.CacheEntries());
  EXPECT_EQ(36u, image_decoding_store_.MemoryUsageInBytes());

  EvictOneCache();
  EXPECT_FALSE(image_decoding_store_.CacheEntries());
  EXPECT_FALSE(image_decoding_store_.MemoryUsageInBytes());
}

TEST_F(ImageDecodingStoreTest, decoderInUseNotEvicted) {
  auto decoder1 = std::make_unique<MockImageDecoder>(this);
  auto decoder2 = std::make_unique<MockImageDecoder>(this);
  auto decoder3 = std::make_unique<MockImageDecoder>(this);
  decoder1->SetSize(1, 1);
  decoder2->SetSize(2, 2);
  decoder3->SetSize(3, 3);
  image_decoding_store_.InsertDecoder(generator_.get(),
                                      cc::PaintImage::kDefaultGeneratorClientId,
                                      std::move(decoder1));
  image_decoding_store_.InsertDecoder(generator_.get(),
                                      cc::PaintImage::kDefaultGeneratorClientId,
                                      std::move(decoder2));
  image_decoding_store_.InsertDecoder(generator_.get(),
                                      cc::PaintImage::kDefaultGeneratorClientId,
                                      std::move(decoder3));
  EXPECT_EQ(3, image_decoding_store_.CacheEntries());

  ImageDecoder* test_decoder;
  EXPECT_TRUE(image_decoding_store_.LockDecoder(
      generator_.get(), SkISize::Make(2, 2), ImageDecoder::kAlphaPremultiplied,
      cc::PaintImage::kDefaultGeneratorClientId, &test_decoder));

  EvictOneCache();
  EvictOneCache();
  EvictOneCache();
  EXPECT_EQ(1, image_decoding_store_.CacheEntries());
  EXPECT_EQ(16u, image_decoding_store_.MemoryUsageInBytes());

  image_decoding_store_.UnlockDecoder(generator_.get(),
                                      cc::PaintImage::kDefaultGeneratorClientId,
                                      test_decoder);
  EvictOneCache();
  EXPECT_FALSE(image_decoding_store_.CacheEntries());
  EXPECT_FALSE(image_decoding_store_.MemoryUsageInBytes());
}

TEST_F(ImageDecodingStoreTest, removeDecoder) {
  const SkISize size = SkISize::Make(1, 1);
  auto decoder = std::make_unique<MockImageDecoder>(this);
  decoder->SetSize(1, 1);
  const ImageDecoder* ref_decoder = decoder.get();
  image_decoding_store_.InsertDecoder(generator_.get(),
                                      cc::PaintImage::kDefaultGeneratorClientId,
                                      std::move(decoder));
  EXPECT_EQ(1, image_decoding_store_.CacheEntries());
  EXPECT_EQ(4u, image_decoding_store_.MemoryUsageInBytes());

  ImageDecoder* test_decoder;
  EXPECT_TRUE(image_decoding_store_.LockDecoder(
      generator_.get(), size, ImageDecoder::kAlphaPremultiplied,
      cc::PaintImage::kDefaultGeneratorClientId, &test_decoder));
  EXPECT_TRUE(test_decoder);
  EXPECT_EQ(ref_decoder, test_decoder);
  image_decoding_store_.RemoveDecoder(generator_.get(),
                                      cc::PaintImage::kDefaultGeneratorClientId,
                                      test_decoder);
  EXPECT_FALSE(image_decoding_store_.CacheEntries());

  EXPECT_FALSE(image_decoding_store_.LockDecoder(
      generator_.get(), size, ImageDecoder::kAlphaPremultiplied,
      cc::PaintImage::kDefaultGeneratorClientId, &test_decoder));
}

TEST_F(ImageDecodingStoreTest, MultipleClientsForSameGenerator) {
  image_decoding_store_.Clear();
  ASSERT_EQ(image_decoding_store_.CacheEntries(), 0);

  const SkISize size = SkISize::Make(1, 1);

  auto decoder = std::make_unique<MockImageDecoder>(this);
  ImageDecoder* decoder_1 = decoder.get();
  decoder_1->SetSize(1, 1);
  auto client_id_1 = cc::PaintImage::GetNextGeneratorClientId();
  image_decoding_store_.InsertDecoder(generator_.get(), client_id_1,
                                      std::move(decoder));
  EXPECT_EQ(image_decoding_store_.CacheEntries(), 1);

  decoder = std::make_unique<MockImageDecoder>(this);
  ImageDecoder* decoder_2 = decoder.get();
  decoder_2->SetSize(1, 1);
  auto client_id_2 = cc::PaintImage::GetNextGeneratorClientId();
  image_decoding_store_.InsertDecoder(generator_.get(), client_id_2,
                                      std::move(decoder));
  EXPECT_EQ(image_decoding_store_.CacheEntries(), 2);

  ImageDecoder* cached_decoder = nullptr;
  image_decoding_store_.LockDecoder(generator_.get(), size,
                                    ImageDecoder::kAlphaPremultiplied,
                                    client_id_1, &cached_decoder);
  EXPECT_EQ(decoder_1, cached_decoder);

  image_decoding_store_.LockDecoder(generator_.get(), size,
                                    ImageDecoder::kAlphaPremultiplied,
                                    client_id_2, &cached_decoder);
  EXPECT_EQ(decoder_2, cached_decoder);

  image_decoding_store_.RemoveDecoder(generator_.get(), client_id_1, decoder_1);
  image_decoding_store_.RemoveDecoder(generator_.get(), client_id_2, decoder_2);
  EXPECT_EQ(image_decoding_store_.CacheEntries(), 0);
}

TEST_F(ImageDecodingStoreTest, OnMemoryPressure) {
  auto decoder = std::make_unique<MockImageDecoder>(this);
  decoder->SetSize(1, 1);
  image_decoding_store_.InsertDecoder(generator_.get(),
                                      cc::PaintImage::kDefaultGeneratorClientId,
                                      std::move(decoder));
  EXPECT_EQ(1, image_decoding_store_.CacheEntries());
  EXPECT_EQ(4u, image_decoding_store_.MemoryUsageInBytes());

  base::MemoryPressureListener::SimulatePressureNotification(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, image_decoding_store_.CacheEntries());
  EXPECT_EQ(4u, image_decoding_store_.MemoryUsageInBytes());

  base::MemoryPressureListener::SimulatePressureNotification(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0, image_decoding_store_.CacheEntries());
  EXPECT_EQ(0u, image_decoding_store_.MemoryUsageInBytes());
}

}  // namespace blink
