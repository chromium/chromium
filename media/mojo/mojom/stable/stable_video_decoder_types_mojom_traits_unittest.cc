// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/mojom/stable/stable_video_decoder_types_mojom_traits.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(StableVideoDecoderTypesMojomTraitsTest, ValidNonEOSDecoderBuffer) {
  stable::mojom::DecoderBufferPtr mojom_decoder_buffer =
      stable::mojom::DecoderBuffer::New();
  mojom_decoder_buffer->timestamp = base::Milliseconds(32);
  mojom_decoder_buffer->duration = base::Milliseconds(16);
  mojom_decoder_buffer->is_end_of_stream = false;
  mojom_decoder_buffer->data_size = 100;
  mojom_decoder_buffer->is_key_frame = true;

  std::vector<uint8_t> serialized_decoder_buffer =
      stable::mojom::DecoderBuffer::Serialize(&mojom_decoder_buffer);

  scoped_refptr<DecoderBuffer> deserialized_decoder_buffer;
  ASSERT_TRUE(stable::mojom::DecoderBuffer::Deserialize(
      serialized_decoder_buffer, &deserialized_decoder_buffer));
  ASSERT_TRUE(deserialized_decoder_buffer);

  ASSERT_FALSE(deserialized_decoder_buffer->end_of_stream());
  EXPECT_EQ(deserialized_decoder_buffer->timestamp(),
            mojom_decoder_buffer->timestamp);
  EXPECT_EQ(deserialized_decoder_buffer->duration(),
            mojom_decoder_buffer->duration);
  EXPECT_EQ(deserialized_decoder_buffer->data_size(),
            base::strict_cast<size_t>(mojom_decoder_buffer->data_size));
  EXPECT_EQ(deserialized_decoder_buffer->is_key_frame(),
            mojom_decoder_buffer->is_key_frame);
}

TEST(StableVideoDecoderTypesMojomTraitsTest, InfiniteDecoderBufferDuration) {
  stable::mojom::DecoderBufferPtr mojom_decoder_buffer =
      stable::mojom::DecoderBuffer::New();
  mojom_decoder_buffer->timestamp = base::Milliseconds(32);
  mojom_decoder_buffer->duration = media::kInfiniteDuration;
  mojom_decoder_buffer->is_end_of_stream = false;
  mojom_decoder_buffer->data_size = 100;
  mojom_decoder_buffer->is_key_frame = true;

  std::vector<uint8_t> serialized_decoder_buffer =
      stable::mojom::DecoderBuffer::Serialize(&mojom_decoder_buffer);

  scoped_refptr<DecoderBuffer> deserialized_decoder_buffer;
  ASSERT_FALSE(stable::mojom::DecoderBuffer::Deserialize(
      serialized_decoder_buffer, &deserialized_decoder_buffer));
}

TEST(StableVideoDecoderTypesMojomTraitsTest, NegativeDecoderBufferDuration) {
  stable::mojom::DecoderBufferPtr mojom_decoder_buffer =
      stable::mojom::DecoderBuffer::New();
  mojom_decoder_buffer->timestamp = base::Milliseconds(32);
  mojom_decoder_buffer->duration = base::TimeDelta() - base::Milliseconds(16);
  mojom_decoder_buffer->is_end_of_stream = false;
  mojom_decoder_buffer->data_size = 100;
  mojom_decoder_buffer->is_key_frame = true;

  std::vector<uint8_t> serialized_decoder_buffer =
      stable::mojom::DecoderBuffer::Serialize(&mojom_decoder_buffer);

  scoped_refptr<DecoderBuffer> deserialized_decoder_buffer;
  ASSERT_FALSE(stable::mojom::DecoderBuffer::Deserialize(
      serialized_decoder_buffer, &deserialized_decoder_buffer));
}

}  // namespace media
