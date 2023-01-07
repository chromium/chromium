// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/encoded_video_chunk.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_typedefs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_encoded_video_chunk_init.h"
#include "third_party/blink/renderer/modules/webcodecs/allow_shared_buffer_source_util.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

class EncodedVideoChunkTest : public testing::Test {
 public:
  AllowSharedBufferSource* StringToBuffer(std::string data) {
    return MakeGarbageCollected<AllowSharedBufferSource>(
        DOMArrayBuffer::Create(data.data(), data.size()));
  }

  std::string BufferToString(const media::DecoderBuffer& buffer) {
    return std::string(reinterpret_cast<const char*>(buffer.data()),
                       buffer.data_size());
  }
};

TEST_F(EncodedVideoChunkTest, ConstructorAndAttributes) {
  String type = "key";
  int64_t timestamp = 1000000;
  std::string data = "test";
  auto* init = EncodedVideoChunkInit::Create();
  init->setTimestamp(timestamp);
  init->setType(type);
  init->setData(StringToBuffer(data));
  auto* encoded = EncodedVideoChunk::Create(init);

  EXPECT_EQ(type, encoded->type());
  EXPECT_EQ(timestamp, encoded->timestamp());
  EXPECT_EQ(data, BufferToString(*encoded->buffer()));
  EXPECT_FALSE(encoded->duration().has_value());
}

TEST_F(EncodedVideoChunkTest, ConstructorWithDuration) {
  String type = "key";
  int64_t timestamp = 1000000;
  uint64_t duration = 16667;
  std::string data = "test";
  auto* init = EncodedVideoChunkInit::Create();
  init->setTimestamp(timestamp);
  init->setDuration(duration);
  init->setType(type);
  init->setData(StringToBuffer(data));
  auto* encoded = EncodedVideoChunk::Create(init);

  EXPECT_EQ(type, encoded->type());
  EXPECT_EQ(timestamp, encoded->timestamp());
  EXPECT_EQ(data, BufferToString(*encoded->buffer()));
  ASSERT_TRUE(encoded->duration().has_value());
  EXPECT_EQ(duration, encoded->duration().value());
}

}  // namespace

}  // namespace blink
