// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/decrypt_config_util.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_encryption_pattern.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_subsample_entry.h"
#include "third_party/blink/renderer/modules/webcodecs/test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

namespace {

TEST(DecryptConfigUtilTest, BadScheme) {
  test::TaskEnvironment task_environment;
  auto* js_config = MakeGarbageCollected<DecryptConfig>();
  js_config->setEncryptionScheme("test");
  EXPECT_EQ(nullptr, CreateMediaDecryptConfig(*js_config));
}

TEST(DecryptConfigUtilTest, WrongIVSize) {
  test::TaskEnvironment task_environment;
  auto* js_config = MakeGarbageCollected<DecryptConfig>();
  js_config->setEncryptionScheme("cenc");
  js_config->setInitializationVector(StringToBuffer("1234567890"));
  EXPECT_EQ(nullptr, CreateMediaDecryptConfig(*js_config));
}

TEST(DecryptConfigUtilTest, CreateCbcsWithoutPattern) {
  test::TaskEnvironment task_environment;
  auto expected_media_config =
      CreateTestDecryptConfig(media::EncryptionScheme::kCbcs);

  auto* js_config = MakeGarbageCollected<DecryptConfig>();
  js_config->setEncryptionScheme("cbcs");
  js_config->setKeyId(StringToBuffer(expected_media_config->key_id()));
  js_config->setInitializationVector(
      StringToBuffer(expected_media_config->iv()));

  HeapVector<Member<SubsampleEntry>> subsamples;
  for (const auto& entry : expected_media_config->subsamples()) {
    auto* js_entry = MakeGarbageCollected<SubsampleEntry>();
    js_entry->setClearBytes(entry.clear_bytes);
    js_entry->setCypherBytes(entry.cypher_bytes);
    subsamples.push_back(js_entry);
  }
  js_config->setSubsampleLayout(subsamples);

  auto created_media_config = CreateMediaDecryptConfig(*js_config);
  ASSERT_NE(nullptr, created_media_config);
  EXPECT_TRUE(expected_media_config->Matches(*created_media_config));
}

TEST(DecryptConfigUtilTest, CreateCbcsWithPattern) {
  test::TaskEnvironment task_environment;
  const media::EncryptionPattern kPattern(1, 2);

  auto expected_media_config =
      CreateTestDecryptConfig(media::EncryptionScheme::kCbcs, kPattern);

  auto* js_config = MakeGarbageCollected<DecryptConfig>();
  js_config->setEncryptionScheme("cbcs");
  js_config->setKeyId(StringToBuffer(expected_media_config->key_id()));
  js_config->setInitializationVector(
      StringToBuffer(expected_media_config->iv()));

  HeapVector<Member<SubsampleEntry>> subsamples;
  for (const auto& entry : expected_media_config->subsamples()) {
    auto* js_entry = MakeGarbageCollected<SubsampleEntry>();
    js_entry->setClearBytes(entry.clear_bytes);
    js_entry->setCypherBytes(entry.cypher_bytes);
    subsamples.push_back(js_entry);
  }
  js_config->setSubsampleLayout(subsamples);

  auto* pattern = MakeGarbageCollected<EncryptionPattern>();
  pattern->setCryptByteBlock(
      expected_media_config->encryption_pattern()->crypt_byte_block());
  pattern->setSkipByteBlock(
      expected_media_config->encryption_pattern()->skip_byte_block());
  js_config->setEncryptionPattern(pattern);

  auto created_media_config = CreateMediaDecryptConfig(*js_config);
  ASSERT_NE(nullptr, created_media_config);
  EXPECT_TRUE(expected_media_config->Matches(*created_media_config));
}

TEST(DecryptConfigUtilTest, CreateCenc) {
  test::TaskEnvironment task_environment;
  auto expected_media_config =
      CreateTestDecryptConfig(media::EncryptionScheme::kCenc);

  auto* js_config = MakeGarbageCollected<DecryptConfig>();
  js_config->setEncryptionScheme("cenc");
  js_config->setKeyId(StringToBuffer(expected_media_config->key_id()));
  js_config->setInitializationVector(
      StringToBuffer(expected_media_config->iv()));

  HeapVector<Member<SubsampleEntry>> subsamples;
  for (const auto& entry : expected_media_config->subsamples()) {
    auto* js_entry = MakeGarbageCollected<SubsampleEntry>();
    js_entry->setClearBytes(entry.clear_bytes);
    js_entry->setCypherBytes(entry.cypher_bytes);
    subsamples.push_back(js_entry);
  }
  js_config->setSubsampleLayout(subsamples);

  auto created_media_config = CreateMediaDecryptConfig(*js_config);
  ASSERT_NE(nullptr, created_media_config);
  EXPECT_TRUE(expected_media_config->Matches(*created_media_config));
}

}  // namespace

}  // namespace blink
