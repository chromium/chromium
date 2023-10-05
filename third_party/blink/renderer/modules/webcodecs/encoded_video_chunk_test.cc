// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/encoded_video_chunk.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_typedefs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_encoded_video_chunk_init.h"
#include "third_party/blink/renderer/modules/webcodecs/array_buffer_util.h"
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
  V8TestingScope v8_scope;
  String type = "key";
  int64_t timestamp = 1000000;
  std::string data = "test";
  auto* init = EncodedVideoChunkInit::Create();
  init->setTimestamp(timestamp);
  init->setType(type);
  init->setData(StringToBuffer(data));
  auto* encoded = EncodedVideoChunk::Create(v8_scope.GetScriptState(), init,
                                            v8_scope.GetExceptionState());

  EXPECT_EQ(type, encoded->type());
  EXPECT_EQ(timestamp, encoded->timestamp());
  EXPECT_EQ(data, BufferToString(*encoded->buffer()));
  EXPECT_FALSE(encoded->duration().has_value());
}

TEST_F(EncodedVideoChunkTest, ConstructorWithDuration) {
  V8TestingScope v8_scope;
  String type = "key";
  int64_t timestamp = 1000000;
  uint64_t duration = 16667;
  std::string data = "test";
  auto* init = EncodedVideoChunkInit::Create();
  init->setTimestamp(timestamp);
  init->setDuration(duration);
  init->setType(type);
  init->setData(StringToBuffer(data));
  auto* encoded = EncodedVideoChunk::Create(v8_scope.GetScriptState(), init,
                                            v8_scope.GetExceptionState());

  EXPECT_EQ(type, encoded->type());
  EXPECT_EQ(timestamp, encoded->timestamp());
  EXPECT_EQ(data, BufferToString(*encoded->buffer()));
  ASSERT_TRUE(encoded->duration().has_value());
  EXPECT_EQ(duration, encoded->duration().value());
}

TEST_F(EncodedVideoChunkTest, TransferBuffer) {
  V8TestingScope v8_scope;
  String type = "key";
  int64_t timestamp = 1000000;
  std::string data = "test";
  auto* init = EncodedVideoChunkInit::Create();
  init->setTimestamp(timestamp);
  init->setType(type);
  auto* buffer = DOMArrayBuffer::Create(data.data(), data.size());
  init->setData(MakeGarbageCollected<AllowSharedBufferSource>(buffer));
  HeapVector<Member<DOMArrayBuffer>> transfer;
  transfer.push_back(Member<DOMArrayBuffer>(buffer));
  init->setTransfer(std::move(transfer));
  auto* encoded = EncodedVideoChunk::Create(v8_scope.GetScriptState(), init,
                                            v8_scope.GetExceptionState());

  EXPECT_TRUE(buffer->IsDetached());
  EXPECT_EQ(encoded->byteLength(), data.size());
}

}  // namespace

}  // namespace blink
