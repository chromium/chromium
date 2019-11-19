// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/decrypt_config.h"

#include <sstream>

#include "media/base/encryption_pattern.h"
#include "media/base/subsample_entry.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

const char kDefaultKeyId[] = "key_id";
const char kDefaultIV[] = "1234567890123456";
const char kAlternateKeyId[] = "alternate";
const char kAlternateIV[] = "abcdefghijklmnop";

}  // namespace

TEST(DecryptConfigTest, CencConstruction) {
  auto config = DecryptConfig::CreateCencConfig(kDefaultKeyId, kDefaultIV, {});
  EXPECT_EQ(config->key_id(), kDefaultKeyId);
  EXPECT_EQ(config->iv(), kDefaultIV);
  EXPECT_EQ(config->subsamples().size(), 0u);
  EXPECT_EQ(config->encryption_scheme(), EncryptionScheme::kCenc);

  // Now with single subsample entry.
  config =
      DecryptConfig::CreateCencConfig(kAlternateKeyId, kDefaultIV, {{1, 2}});
  EXPECT_EQ(config->key_id(), kAlternateKeyId);
  EXPECT_EQ(config->iv(), kDefaultIV);
  EXPECT_EQ(config->subsamples().size(), 1u);
  EXPECT_EQ(config->subsamples()[0].clear_bytes, 1u);
  EXPECT_EQ(config->subsamples()[0].cypher_bytes, 2u);
  EXPECT_EQ(config->encryption_scheme(), EncryptionScheme::kCenc);

  // Now with multiple subsample entries.
  config = DecryptConfig::CreateCencConfig(kDefaultKeyId, kAlternateIV,
                                           {{1, 2}, {3, 4}, {5, 6}, {7, 8}});
  EXPECT_EQ(config->key_id(), kDefaultKeyId);
  EXPECT_EQ(config->iv(), kAlternateIV);
  EXPECT_EQ(config->subsamples().size(), 4u);
  EXPECT_EQ(config->subsamples()[1].clear_bytes, 3u);
  EXPECT_EQ(config->subsamples()[3].cypher_bytes, 8u);
  EXPECT_EQ(config->encryption_scheme(), EncryptionScheme::kCenc);
}

TEST(DecryptConfigTest, CbcsConstruction) {
  auto config = DecryptConfig::CreateCbcsConfig(kDefaultKeyId, kDefaultIV, {},
                                                EncryptionPattern(1, 2));
  EXPECT_EQ(config->key_id(), kDefaultKeyId);
  EXPECT_EQ(config->iv(), kDefaultIV);
  EXPECT_EQ(config->subsamples().size(), 0u);
  EXPECT_EQ(config->encryption_scheme(), EncryptionScheme::kCbcs);
  EXPECT_TRUE(config->HasPattern());
  EXPECT_EQ(config->encryption_pattern()->crypt_byte_block(), 1u);
  EXPECT_EQ(config->encryption_pattern()->skip_byte_block(), 2u);

  // Now with multiple subsample entries.
  config = DecryptConfig::CreateCbcsConfig(kDefaultKeyId, kAlternateIV,
                                           {{1, 2}, {3, 4}, {5, 6}, {7, 8}},
                                           EncryptionPattern(1, 0));
  EXPECT_EQ(config->key_id(), kDefaultKeyId);
  EXPECT_EQ(config->iv(), kAlternateIV);
  EXPECT_EQ(config->subsamples().size(), 4u);
  EXPECT_EQ(config->subsamples()[0].clear_bytes, 1u);
  EXPECT_EQ(config->subsamples()[0].cypher_bytes, 2u);
  EXPECT_EQ(config->subsamples()[3].clear_bytes, 7u);
  EXPECT_EQ(config->subsamples()[3].cypher_bytes, 8u);
  EXPECT_EQ(config->encryption_scheme(), EncryptionScheme::kCbcs);
  EXPECT_TRUE(config->HasPattern());
  EXPECT_EQ(config->encryption_pattern()->crypt_byte_block(), 1u);
  EXPECT_EQ(config->encryption_pattern()->skip_byte_block(), 0u);

  // Now without pattern.
  config = DecryptConfig::CreateCbcsConfig(kAlternateKeyId, kDefaultIV,
                                           {{1, 2}}, base::nullopt);
  EXPECT_EQ(config->key_id(), kAlternateKeyId);
  EXPECT_EQ(config->iv(), kDefaultIV);
  EXPECT_EQ(config->subsamples().size(), 1u);
  EXPECT_EQ(config->subsamples()[0].clear_bytes, 1u);
  EXPECT_EQ(config->subsamples()[0].cypher_bytes, 2u);
  EXPECT_EQ(config->encryption_scheme(), EncryptionScheme::kCbcs);
  EXPECT_FALSE(config->HasPattern());
}

TEST(DecryptConfigTest, Matches) {
  auto config1 = DecryptConfig::CreateCencConfig(kDefaultKeyId, kDefaultIV, {});
  EXPECT_TRUE(config1->Matches(*config1));

  auto config2 = DecryptConfig::CreateCbcsConfig(kDefaultKeyId, kDefaultIV, {},
                                                 EncryptionPattern(1, 2));
  EXPECT_TRUE(config2->Matches(*config2));

  EXPECT_FALSE(config1->Matches(*config2));
  EXPECT_FALSE(config2->Matches(*config1));
}

TEST(DecryptConfigTest, CencMatches) {
  auto config1 = DecryptConfig::CreateCencConfig(kDefaultKeyId, kDefaultIV, {});
  EXPECT_TRUE(config1->Matches(*config1));

  // Different key_id.
  auto config2 =
      DecryptConfig::CreateCencConfig(kAlternateKeyId, kDefaultIV, {});
  EXPECT_FALSE(config1->Matches(*config2));
  EXPECT_FALSE(config2->Matches(*config1));

  // Different IV.
  auto config3 =
      DecryptConfig::CreateCencConfig(kDefaultKeyId, kAlternateIV, {});
  EXPECT_FALSE(config1->Matches(*config3));
  EXPECT_FALSE(config2->Matches(*config3));
  EXPECT_FALSE(config3->Matches(*config1));
  EXPECT_FALSE(config3->Matches(*config2));

  // Different subsamples.
  auto config4 = DecryptConfig::CreateCencConfig(
      kDefaultKeyId, kDefaultIV, {{1, 2}, {3, 4}, {5, 6}, {7, 8}});
  EXPECT_FALSE(config1->Matches(*config4));
  EXPECT_FALSE(config2->Matches(*config4));
  EXPECT_FALSE(config3->Matches(*config4));
  EXPECT_FALSE(config4->Matches(*config1));
  EXPECT_FALSE(config4->Matches(*config2));
  EXPECT_FALSE(config4->Matches(*config3));
}

TEST(DecryptConfigTest, CbcsMatches) {
  auto config1 = DecryptConfig::CreateCbcsConfig(kDefaultKeyId, kDefaultIV, {},
                                                 EncryptionPattern(1, 2));
  EXPECT_TRUE(config1->Matches(*config1));

  // Different key_id.
  auto config2 = DecryptConfig::CreateCbcsConfig(kAlternateKeyId, kDefaultIV,
                                                 {}, EncryptionPattern(1, 2));
  EXPECT_FALSE(config1->Matches(*config2));
  EXPECT_FALSE(config2->Matches(*config1));

  // Different IV.
  auto config3 = DecryptConfig::CreateCbcsConfig(kDefaultKeyId, kAlternateIV,
                                                 {}, EncryptionPattern(1, 2));
  EXPECT_FALSE(config1->Matches(*config3));
  EXPECT_FALSE(config2->Matches(*config3));
  EXPECT_FALSE(config3->Matches(*config1));
  EXPECT_FALSE(config3->Matches(*config2));

  // Different subsamples.
  auto config4 = DecryptConfig::CreateCbcsConfig(kDefaultKeyId, kDefaultIV,
                                                 {{1, 2}, {3, 4}, {5, 6}},
                                                 EncryptionPattern(1, 2));
  EXPECT_FALSE(config1->Matches(*config4));
  EXPECT_FALSE(config2->Matches(*config4));
  EXPECT_FALSE(config3->Matches(*config4));
  EXPECT_FALSE(config4->Matches(*config1));
  EXPECT_FALSE(config4->Matches(*config2));
  EXPECT_FALSE(config4->Matches(*config3));

  // Different pattern.
  auto config5 = DecryptConfig::CreateCbcsConfig(kDefaultKeyId, kDefaultIV, {},
                                                 EncryptionPattern(5, 6));
  EXPECT_FALSE(config1->Matches(*config5));
  EXPECT_FALSE(config2->Matches(*config5));
  EXPECT_FALSE(config3->Matches(*config5));
  EXPECT_FALSE(config4->Matches(*config5));
  EXPECT_FALSE(config5->Matches(*config1));
  EXPECT_FALSE(config5->Matches(*config2));
  EXPECT_FALSE(config5->Matches(*config3));
  EXPECT_FALSE(config5->Matches(*config4));

  // Without pattern.
  auto config6 = DecryptConfig::CreateCbcsConfig(kDefaultKeyId, kDefaultIV, {},
                                                 base::nullopt);
  EXPECT_FALSE(config1->Matches(*config6));
  EXPECT_FALSE(config5->Matches(*config6));
  EXPECT_FALSE(config6->Matches(*config1));
  EXPECT_FALSE(config6->Matches(*config5));
}

TEST(DecryptConfigTest, Output) {
  std::ostringstream stream;

  // Simple 'cenc' config.
  stream << *DecryptConfig::CreateCencConfig(kDefaultKeyId, kDefaultIV, {});

  // Try with subsamples.
  stream << *DecryptConfig::CreateCencConfig(kAlternateKeyId, kAlternateIV,
                                             {{1, 2}, {3, 4}, {5, 6}});

  // Simple 'cbcs' config.
  stream << *DecryptConfig::CreateCbcsConfig(kDefaultKeyId, kDefaultIV, {},
                                             base::nullopt);

  // 'cbcs' config with subsamples and pattern.
  stream << *DecryptConfig::CreateCbcsConfig(kAlternateKeyId, kAlternateIV,
                                             {{1, 2}, {3, 4}, {5, 6}, {7, 8}},
                                             EncryptionPattern(1, 2));
}

}  // namespace media
