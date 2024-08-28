// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/encoded_video_chunk.h"

#include "base/containers/span.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_decrypt_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_encoded_video_chunk_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_subsample_entry.h"
#include "third_party/blink/renderer/modules/webcodecs/array_buffer_util.h"
#include "third_party/blink/renderer/modules/webcodecs/test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

TEST(EncodedVideoChunkTest, ConstructorAndAttributes) {
  test::TaskEnvironment task_environment;
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

TEST(EncodedVideoChunkTest, ConstructorWithDuration) {
  test::TaskEnvironment task_environment;
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

TEST(EncodedVideoChunkTest, TransferBuffer) {
  test::TaskEnvironment task_environment;
  V8TestingScope v8_scope;
  String type = "key";
  int64_t timestamp = 1000000;
  std::string data = "test";
  auto* init = EncodedVideoChunkInit::Create();
  init->setTimestamp(timestamp);
  init->setType(type);
  auto* buffer = DOMArrayBuffer::Create(base::as_byte_span(data));
  init->setData(MakeGarbageCollected<AllowSharedBufferSource>(buffer));
  HeapVector<Member<DOMArrayBuffer>> transfer;
  transfer.push_back(Member<DOMArrayBuffer>(buffer));
  init->setTransfer(std::move(transfer));
  auto* encoded = EncodedVideoChunk::Create(v8_scope.GetScriptState(), init,
                                            v8_scope.GetExceptionState());

  EXPECT_TRUE(buffer->IsDetached());
  EXPECT_EQ(encoded->byteLength(), data.size());
}

TEST(EncodedVideoChunkTest, DecryptConfig) {
  test::TaskEnvironment task_environment;
  V8TestingScope v8_scope;
  auto* init = EncodedVideoChunkInit::Create();
  init->setTimestamp(1);
  init->setType("key");
  init->setData(StringToBuffer("test"));

  auto expected_media_decrypt_config =
      CreateTestDecryptConfig(media::EncryptionScheme::kCenc);

  auto* decrypt_config = MakeGarbageCollected<DecryptConfig>();
  decrypt_config->setEncryptionScheme("cenc");
  decrypt_config->setKeyId(
      StringToBuffer(expected_media_decrypt_config->key_id()));
  decrypt_config->setInitializationVector(
      StringToBuffer(expected_media_decrypt_config->iv()));

  HeapVector<Member<SubsampleEntry>> subsamples;
  for (const auto& entry : expected_media_decrypt_config->subsamples()) {
    auto* js_entry = MakeGarbageCollected<SubsampleEntry>();
    js_entry->setClearBytes(entry.clear_bytes);
    js_entry->setCypherBytes(entry.cypher_bytes);
    subsamples.push_back(js_entry);
  }
  decrypt_config->setSubsampleLayout(subsamples);
  init->setDecryptConfig(decrypt_config);

  auto* encoded = EncodedVideoChunk::Create(v8_scope.GetScriptState(), init,
                                            v8_scope.GetExceptionState());

  ASSERT_NE(nullptr, encoded->buffer()->decrypt_config());
  EXPECT_TRUE(expected_media_decrypt_config->Matches(
      *encoded->buffer()->decrypt_config()));
}

}  // namespace

}  // namespace blink
