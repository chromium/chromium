// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/mojo/common/media_type_converters.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <memory>

#include "media/base/audio_buffer.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decrypt_config.h"
#include "media/base/encryption_pattern.h"
#include "media/base/encryption_scheme.h"
#include "media/base/sample_format.h"
#include "media/base/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

void CompareBytes(uint8_t* original_data, uint8_t* result_data, size_t length) {
  EXPECT_GT(length, 0u);
  EXPECT_EQ(memcmp(original_data, result_data, length), 0);
}

void CompareAudioBuffers(SampleFormat sample_format,
                         const AudioBuffer& original,
                         const AudioBuffer& result) {
  EXPECT_EQ(original.frame_count(), result.frame_count());
  EXPECT_EQ(original.timestamp(), result.timestamp());
  EXPECT_EQ(original.duration(), result.duration());
  EXPECT_EQ(original.sample_rate(), result.sample_rate());
  EXPECT_EQ(original.channel_count(), result.channel_count());
  EXPECT_EQ(original.channel_layout(), result.channel_layout());
  EXPECT_EQ(original.end_of_stream(), result.end_of_stream());

  // Compare bytes in buffer.
  int bytes_per_channel =
      original.frame_count() * SampleFormatToBytesPerChannel(sample_format);
  if (IsPlanar(sample_format)) {
    for (int i = 0; i < original.channel_count(); ++i) {
      CompareBytes(original.channel_data()[i], result.channel_data()[i],
                   bytes_per_channel);
    }
    return;
  }

  DCHECK(IsInterleaved(sample_format)) << sample_format;
  CompareBytes(original.channel_data()[0], result.channel_data()[0],
               bytes_per_channel * original.channel_count());
}

}  // namespace

TEST(MediaTypeConvertersTest, ConvertDecoderBuffer_Normal) {
  const uint8_t kData[] = "hello, world";
  const uint8_t kAlphaData[] = "sideshow bob";
  const uint32_t kSpatialLayers[] = {36, 24, 36};
  const size_t kDataSize = std::size(kData);
  const size_t kAlphaDataSize = std::size(kAlphaData);
  const size_t kSpatialLayersSize = std::size(kSpatialLayers);
  const size_t kSecureHandle = 42;

  // Original.
  scoped_refptr<DecoderBuffer> buffer(DecoderBuffer::CopyFrom(kData));
  buffer->set_timestamp(base::Milliseconds(123));
  buffer->set_duration(base::Milliseconds(456));
  buffer->set_discard_padding(DecoderBuffer::DiscardPadding(
      base::Milliseconds(5), base::Milliseconds(6)));
  buffer->WritableSideData().alpha_data.assign(kAlphaData,
                                               kAlphaData + kAlphaDataSize);
  buffer->WritableSideData().spatial_layers.assign(
      kSpatialLayers, kSpatialLayers + kSpatialLayersSize);
  buffer->WritableSideData().secure_handle = kSecureHandle;

  // Convert from and back.
  mojom::DecoderBufferPtr ptr(mojom::DecoderBuffer::From(*buffer));
  scoped_refptr<DecoderBuffer> result(ptr.To<scoped_refptr<DecoderBuffer>>());

  // Compare.
  // Note: We intentionally do not serialize the data section of the
  // DecoderBuffer; no need to check the data here.
  EXPECT_EQ(kDataSize, result->size());
  EXPECT_TRUE(result->has_side_data());
  EXPECT_TRUE(buffer->side_data()->Matches(result->side_data().value()));
  EXPECT_EQ(buffer->timestamp(), result->timestamp());
  EXPECT_EQ(buffer->duration(), result->duration());
  EXPECT_EQ(buffer->is_key_frame(), result->is_key_frame());
  EXPECT_EQ(buffer->discard_padding(), result->discard_padding());

  // Both |buffer| and |result| are not encrypted.
  EXPECT_FALSE(buffer->decrypt_config());
  EXPECT_FALSE(result->decrypt_config());
}

TEST(MediaTypeConvertersTest, ConvertDecoderBuffer_EOS) {
  // Original.
  scoped_refptr<DecoderBuffer> buffer(DecoderBuffer::CreateEOSBuffer());

  // Convert from and back.
  mojom::DecoderBufferPtr ptr(mojom::DecoderBuffer::From(*buffer));
  scoped_refptr<DecoderBuffer> result(ptr.To<scoped_refptr<DecoderBuffer>>());

  // Compare.
  EXPECT_TRUE(result->end_of_stream());
}

TEST(MediaTypeConvertersTest, ConvertDecoderBuffer_EOS_Video_NextConfig) {
  // Original.
  auto buffer = DecoderBuffer::CreateEOSBuffer(TestVideoConfig::Normal());

  // Convert from and back.
  auto ptr = mojom::DecoderBuffer::From(*buffer);
  auto result = ptr.To<scoped_refptr<DecoderBuffer>>();

  // Compare.
  EXPECT_TRUE(result->end_of_stream());
  ASSERT_TRUE(result->next_config());
  EXPECT_TRUE(absl::get<VideoDecoderConfig>(*result->next_config())
                  .Matches(TestVideoConfig::Normal()));
}

TEST(MediaTypeConvertersTest, ConvertDecoderBuffer_EOS_Audio_NextConfig) {
  // Original.
  auto buffer = DecoderBuffer::CreateEOSBuffer(TestAudioConfig::Normal());

  // Convert from and back.
  auto ptr = mojom::DecoderBuffer::From(*buffer);
  ASSERT_TRUE(ptr);
  auto result = ptr.To<scoped_refptr<DecoderBuffer>>();

  // Compare.
  EXPECT_TRUE(result->end_of_stream());
  ASSERT_TRUE(result->next_config());
  EXPECT_TRUE(absl::get<AudioDecoderConfig>(*result->next_config())
                  .Matches(TestAudioConfig::Normal()));
}

TEST(MediaTypeConvertersTest, ConvertDecoderBuffer_EOS_NextConfig_Invalid) {
  // Original.
  const uint8_t kData[] = "hello, world";
  auto buffer = DecoderBuffer::CopyFrom(kData);
  // Ensure some side data exists.
  buffer->WritableSideData().secure_handle = 1;

  {
    auto ptr = mojom::DecoderBuffer::From(*buffer);
    ASSERT_TRUE(ptr);
    auto result = ptr.To<scoped_refptr<DecoderBuffer>>();
    EXPECT_TRUE(result);

    // Next audio config should only be on EOS buffers.
    ptr->side_data->next_config =
        mojom::DecoderBufferSideDataNextConfig::NewNextAudioConfig(
            TestAudioConfig::Normal());
    result = ptr.To<scoped_refptr<DecoderBuffer>>();
    EXPECT_FALSE(result);
  }

  {
    auto ptr = mojom::DecoderBuffer::From(*buffer);
    ASSERT_TRUE(ptr);
    auto result = ptr.To<scoped_refptr<DecoderBuffer>>();
    EXPECT_TRUE(result);

    // Next video config should only be on EOS buffers.
    ptr->side_data->next_config =
        mojom::DecoderBufferSideDataNextConfig::NewNextVideoConfig(
            TestVideoConfig::Normal());
    result = ptr.To<scoped_refptr<DecoderBuffer>>();
    EXPECT_FALSE(result);
  }

  {
    buffer = DecoderBuffer::CreateEOSBuffer();
    auto ptr = mojom::DecoderBuffer::From(*buffer);
    ASSERT_TRUE(ptr);
    auto result = ptr.To<scoped_refptr<DecoderBuffer>>();
    EXPECT_TRUE(result);

    // The only side data allowed on EOS buffers is a next config.
    ptr->side_data = media::mojom::DecoderBufferSideData::New();
    ptr->side_data->secure_handle = 3;
    result = ptr.To<scoped_refptr<DecoderBuffer>>();
    EXPECT_FALSE(result);
  }
}

TEST(MediaTypeConvertersTest, ConvertDecoderBuffer_KeyFrame) {
  const uint8_t kData[] = "hello, world";
  const size_t kDataSize = std::size(kData);

  // Original.
  scoped_refptr<DecoderBuffer> buffer(DecoderBuffer::CopyFrom(kData));
  buffer->set_is_key_frame(true);
  EXPECT_TRUE(buffer->is_key_frame());

  // Convert from and back.
  mojom::DecoderBufferPtr ptr(mojom::DecoderBuffer::From(*buffer));
  scoped_refptr<DecoderBuffer> result(ptr.To<scoped_refptr<DecoderBuffer>>());

  // Compare.
  // Note: We intentionally do not serialize the data section of the
  // DecoderBuffer; no need to check the data here.
  EXPECT_EQ(kDataSize, result->size());
  EXPECT_TRUE(result->is_key_frame());
}

TEST(MediaTypeConvertersTest, ConvertDecoderBuffer_CencEncryptedBuffer) {
  const uint8_t kData[] = "hello, world";
  const size_t kDataSize = std::size(kData);
  const char kKeyId[] = "00112233445566778899aabbccddeeff";
  const char kIv[] = "0123456789abcdef";

  std::vector<SubsampleEntry> subsamples;
  subsamples.push_back(SubsampleEntry(10, 20));
  subsamples.push_back(SubsampleEntry(30, 40));
  subsamples.push_back(SubsampleEntry(50, 60));

  // Original.
  scoped_refptr<DecoderBuffer> buffer(DecoderBuffer::CopyFrom(kData));
  buffer->set_decrypt_config(
      DecryptConfig::CreateCencConfig(kKeyId, kIv, subsamples));

  // Convert from and back.
  mojom::DecoderBufferPtr ptr(mojom::DecoderBuffer::From(*buffer));
  scoped_refptr<DecoderBuffer> result(ptr.To<scoped_refptr<DecoderBuffer>>());

  // Compare.
  // Note: We intentionally do not serialize the data section of the
  // DecoderBuffer; no need to check the data here.
  EXPECT_EQ(kDataSize, result->size());
  EXPECT_TRUE(buffer->decrypt_config()->Matches(*result->decrypt_config()));

  // Test without DecryptConfig. This is used for clear buffer in an
  // encrypted stream.
  buffer->set_decrypt_config(nullptr);
  EXPECT_FALSE(buffer->decrypt_config());
  result =
      mojom::DecoderBuffer::From(*buffer).To<scoped_refptr<DecoderBuffer>>();
  EXPECT_FALSE(result->decrypt_config());
}

TEST(MediaTypeConvertersTest, ConvertDecoderBuffer_CbcsEncryptedBuffer) {
  const uint8_t kData[] = "hello, world";
  const size_t kDataSize = std::size(kData);
  const char kKeyId[] = "00112233445566778899aabbccddeeff";
  const char kIv[] = "0123456789abcdef";

  std::vector<SubsampleEntry> subsamples;
  subsamples.push_back(SubsampleEntry(10, 20));
  subsamples.push_back(SubsampleEntry(30, 40));
  subsamples.push_back(SubsampleEntry(50, 60));

  EncryptionPattern pattern{1, 2};

  // Original.
  scoped_refptr<DecoderBuffer> buffer(DecoderBuffer::CopyFrom(kData));
  buffer->set_decrypt_config(
      DecryptConfig::CreateCbcsConfig(kKeyId, kIv, subsamples, pattern));

  // Convert from and back.
  mojom::DecoderBufferPtr ptr(mojom::DecoderBuffer::From(*buffer));
  scoped_refptr<DecoderBuffer> result(ptr.To<scoped_refptr<DecoderBuffer>>());

  // Compare.
  // Note: We intentionally do not serialize the data section of the
  // DecoderBuffer; no need to check the data here.
  EXPECT_EQ(kDataSize, result->size());
  EXPECT_TRUE(buffer->decrypt_config()->Matches(*result->decrypt_config()));

  // Test without DecryptConfig. This is used for clear buffer in an
  // encrypted stream.
  buffer->set_decrypt_config(nullptr);
  EXPECT_FALSE(buffer->decrypt_config());
  result =
      mojom::DecoderBuffer::From(*buffer).To<scoped_refptr<DecoderBuffer>>();
  EXPECT_FALSE(result->decrypt_config());
}

TEST(MediaTypeConvertersTest, ConvertAudioBuffer_EOS) {
  // Original.
  scoped_refptr<AudioBuffer> buffer(AudioBuffer::CreateEOSBuffer());

  // Convert to and back.
  mojom::AudioBufferPtr ptr(mojom::AudioBuffer::From(*buffer));
  scoped_refptr<AudioBuffer> result(ptr.To<scoped_refptr<AudioBuffer>>());

  // Compare.
  EXPECT_TRUE(result->end_of_stream());
}

TEST(MediaTypeConvertersTest, ConvertAudioBuffer_MONO) {
  // Original.
  const ChannelLayout kChannelLayout = CHANNEL_LAYOUT_MONO;
  const int kSampleRate = 48000;
  scoped_refptr<AudioBuffer> buffer = MakeAudioBuffer<uint8_t>(
      kSampleFormatU8, kChannelLayout,
      ChannelLayoutToChannelCount(kChannelLayout), kSampleRate, 1, 1,
      kSampleRate / 100, base::TimeDelta());

  // Convert to and back.
  mojom::AudioBufferPtr ptr(mojom::AudioBuffer::From(*buffer));
  scoped_refptr<AudioBuffer> result(ptr.To<scoped_refptr<AudioBuffer>>());

  // Compare.
  CompareAudioBuffers(kSampleFormatU8, *buffer, *result);
}

TEST(MediaTypeConvertersTest, ConvertAudioBuffer_FLOAT) {
  // Original.
  const ChannelLayout kChannelLayout = CHANNEL_LAYOUT_4_0;
  const int kSampleRate = 48000;
  const base::TimeDelta start_time = base::Seconds(1000.0);
  scoped_refptr<AudioBuffer> buffer = MakeAudioBuffer<float>(
      kSampleFormatPlanarF32, kChannelLayout,
      ChannelLayoutToChannelCount(kChannelLayout), kSampleRate, 0.0f, 1.0f,
      kSampleRate / 10, start_time);
  // Convert to and back.
  mojom::AudioBufferPtr ptr(mojom::AudioBuffer::From(*buffer));
  scoped_refptr<AudioBuffer> result(ptr.To<scoped_refptr<AudioBuffer>>());

  // Compare.
  CompareAudioBuffers(kSampleFormatPlanarF32, *buffer, *result);
}

}  // namespace media
