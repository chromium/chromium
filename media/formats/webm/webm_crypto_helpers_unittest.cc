// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/formats/webm/webm_crypto_helpers.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ElementsAre;

namespace {

const uint8_t kKeyId[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};

}  // namespace

namespace media {

TEST(WebMCryptoHelpersTest, EmptyData) {
  std::unique_ptr<DecryptConfig> decrypt_config;
  int data_offset;
  ASSERT_FALSE(WebMCreateDecryptConfig(nullptr, 0, kKeyId, sizeof(kKeyId),
                                       &decrypt_config, &data_offset));
}

TEST(WebMCryptoHelpersTest, ClearData) {
  const uint8_t kData[] = {0x00, 0x0d, 0x0a, 0x0d, 0x0a};
  std::unique_ptr<DecryptConfig> decrypt_config;
  int data_offset;
  ASSERT_TRUE(WebMCreateDecryptConfig(kData, sizeof(kData), kKeyId,
                                      sizeof(kKeyId), &decrypt_config,
                                      &data_offset));
  EXPECT_EQ(1, data_offset);
  EXPECT_FALSE(decrypt_config);
}

TEST(WebMCryptoHelpersTest, EncryptedButNotEnoughBytes) {
  const uint8_t kData[] = {0x01, 0x0d, 0x0a, 0x0d, 0x0a};
  std::unique_ptr<DecryptConfig> decrypt_config;
  int data_offset;
  ASSERT_FALSE(WebMCreateDecryptConfig(kData, sizeof(kData), kKeyId,
                                       sizeof(kKeyId), &decrypt_config,
                                       &data_offset));
}

TEST(WebMCryptoHelpersTest, EncryptedNotPartitioned) {
  const uint8_t kData[] = {
      // Encrypted
      0x01,
      // IV
      0x0d, 0x0a, 0x0d, 0x0a, 0x0d, 0x0a, 0x0d, 0x0a,
      // Data
      0x01, 0x02,
  };
  // Extracted from kData and zero extended to 16 bytes.
  const uint8_t kExpectedIv[] = {
      0x0d, 0x0a, 0x0d, 0x0a, 0x0d, 0x0a, 0x0d, 0x0a,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };
  std::unique_ptr<DecryptConfig> decrypt_config;
  int data_offset;
  ASSERT_TRUE(WebMCreateDecryptConfig(kData, sizeof(kData), kKeyId,
                                      sizeof(kKeyId), &decrypt_config,
                                      &data_offset));
  EXPECT_TRUE(decrypt_config);
  EXPECT_EQ(std::string(kKeyId, kKeyId + sizeof(kKeyId)),
            decrypt_config->key_id());
  EXPECT_EQ(std::string(kExpectedIv, kExpectedIv + sizeof(kExpectedIv)),
            decrypt_config->iv());
  EXPECT_TRUE(decrypt_config->subsamples().empty());
}

TEST(WebMCryptoHelpersTest, EncryptedPartitionedMissingNumPartitionField) {
  const uint8_t kData[] = {
      // Encrypted and Partitioned
      0x03,
      // IV
      0x0d, 0x0a, 0x0d, 0x0a, 0x0d, 0x0a, 0x0d, 0x0a,
  };
  std::unique_ptr<DecryptConfig> decrypt_config;
  int data_offset;
  ASSERT_FALSE(WebMCreateDecryptConfig(kData, sizeof(kData), kKeyId,
                                       sizeof(kKeyId), &decrypt_config,
                                       &data_offset));
}

TEST(WebMCryptoHelpersTest, EncryptedPartitionedNotEnoughBytesForOffsets) {
  const uint8_t kData[] = {
      // Encrypted and Partitioned
      0x03,
      // IV
      0x0d, 0x0a, 0x0d, 0x0a, 0x0d, 0x0a, 0x0d, 0x0a,
      // Num partitions = 2
      0x02,
      // Partition 0 @ offset 3
      0x00, 0x00, 0x00, 0x03,
  };
  std::unique_ptr<DecryptConfig> decrypt_config;
  int data_offset;
  ASSERT_FALSE(WebMCreateDecryptConfig(kData, sizeof(kData), kKeyId,
                                       sizeof(kKeyId), &decrypt_config,
                                       &data_offset));
}

TEST(WebMCryptoHelpersTest, EncryptedPartitionedNotEnoughBytesForData) {
  const uint8_t kData[] = {
      // Encrypted and Partitioned
      0x03,
      // IV
      0x0d, 0x0a, 0x0d, 0x0a, 0x0d, 0x0a, 0x0d, 0x0a,
      // Num partitions = 2
      0x02,
      // Partition 0 @ offset 3, partition 2 @ offset 5
      0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x05,
      // Should have more than 5 bytes of data
      0x00, 0x01, 0x02, 0x03,
  };
  std::unique_ptr<DecryptConfig> decrypt_config;
  int data_offset;
  ASSERT_FALSE(WebMCreateDecryptConfig(kData, sizeof(kData), kKeyId,
                                       sizeof(kKeyId), &decrypt_config,
                                       &data_offset));
}

TEST(WebMCryptoHelpersTest, EncryptedPartitionedNotEnoughBytesForData2) {
  const uint8_t kData[] = {
      // Encrypted and Partitioned
      0x03,
      // IV
      0x0d, 0x0a, 0x0d, 0x0a, 0x0d, 0x0a, 0x0d, 0x0a,
      // Num partitions = 2
      0x02,
      // Partition 0 @ offset 3, partition 1 @ offset 5
      0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x05,
      // Should have more than 5 bytes of data
      0x00, 0x01, 0x02, 0x03, 0x04,
  };
  std::unique_ptr<DecryptConfig> decrypt_config;
  int data_offset;
  ASSERT_FALSE(WebMCreateDecryptConfig(kData, sizeof(kData), kKeyId,
                                       sizeof(kKeyId), &decrypt_config,
                                       &data_offset));
}

TEST(WebMCryptoHelpersTest, EncryptedPartitionedDecreasingOffsets) {
  const uint8_t kData[] = {
      // Encrypted and Partitioned
      0x03,
      // IV
      0x0d, 0x0a, 0x0d, 0x0a, 0x0d, 0x0a, 0x0d, 0x0a,
      // Num partitions = 2
      0x02,
      // Partition 0 @ offset 3, partition 1 @ offset 2
      0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x02,
      // Should have more than 5 bytes of data
      0x00, 0x01, 0x02, 0x03, 0x04,
  };
  std::unique_ptr<DecryptConfig> decrypt_config;
  int data_offset;
  ASSERT_FALSE(WebMCreateDecryptConfig(kData, sizeof(kData), kKeyId,
                                       sizeof(kKeyId), &decrypt_config,
                                       &data_offset));
}

TEST(WebMCryptoHelpersTest, EncryptedPartitionedEvenNumberOfPartitions) {
  const uint8_t kData[] = {
      // Encrypted and Partitioned
      0x03,
      // IV
      0x0d, 0x0a, 0x0d, 0x0a, 0x0d, 0x0a, 0x0d, 0x0a,
      // Num partitions = 2
      0x02,
      // Partition 0 @ offset 3, partition 1 @ offset 5
      0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x05,
      // Should have more than 5 bytes of data
      0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
  };
  // Extracted from kData and zero extended to 16 bytes.
  const uint8_t kExpectedIv[] = {
      0x0d, 0x0a, 0x0d, 0x0a, 0x0d, 0x0a, 0x0d, 0x0a,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };
  std::unique_ptr<DecryptConfig> decrypt_config;
  int data_offset;
  ASSERT_TRUE(WebMCreateDecryptConfig(kData, sizeof(kData), kKeyId,
                                      sizeof(kKeyId), &decrypt_config,
                                      &data_offset));
  EXPECT_TRUE(decrypt_config);
  EXPECT_EQ(std::string(kKeyId, kKeyId + sizeof(kKeyId)),
            decrypt_config->key_id());
  EXPECT_EQ(std::string(kExpectedIv, kExpectedIv + sizeof(kExpectedIv)),
            decrypt_config->iv());
  EXPECT_THAT(decrypt_config->subsamples(),
              ElementsAre(SubsampleEntry(3, 2), SubsampleEntry(1, 0)));
  EXPECT_EQ(18, data_offset);
}

TEST(WebMCryptoHelpersTest, EncryptedPartitionedOddNumberOfPartitions) {
  const uint8_t kData[] = {
      // Encrypted and Partitioned
      0x03,
      // IV
      0x0d, 0x0a, 0x0d, 0x0a, 0x0d, 0x0a, 0x0d, 0x0a,
      // Num partitions = 1
      0x01,
      // Partition 0 @ offset 3,
      0x00, 0x00, 0x00, 0x03,
      // Should have more than 3 bytes of data
      0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
  };
  // Extracted from kData and zero extended to 16 bytes.
  const uint8_t kExpectedIv[] = {
      0x0d, 0x0a, 0x0d, 0x0a, 0x0d, 0x0a, 0x0d, 0x0a,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };
  std::unique_ptr<DecryptConfig> decrypt_config;
  int data_offset;
  ASSERT_TRUE(WebMCreateDecryptConfig(kData, sizeof(kData), kKeyId,
                                      sizeof(kKeyId), &decrypt_config,
                                      &data_offset));
  EXPECT_TRUE(decrypt_config);
  EXPECT_EQ(std::string(kKeyId, kKeyId + sizeof(kKeyId)),
            decrypt_config->key_id());
  EXPECT_EQ(std::string(kExpectedIv, kExpectedIv + sizeof(kExpectedIv)),
            decrypt_config->iv());
  EXPECT_THAT(decrypt_config->subsamples(), ElementsAre(SubsampleEntry(3, 3)));
  EXPECT_EQ(14, data_offset);
}

TEST(WebMCryptoHelpersTest, EncryptedPartitionedZeroNumberOfPartitions) {
  const uint8_t kData[] = {
      // Encrypted and Partitioned
      0x03,
      // IV
      0x0d, 0x0a, 0x0d, 0x0a, 0x0d, 0x0a, 0x0d, 0x0a,
      // Num partitions = 0
      0x00,
      // Some random data.
      0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
  };
  // Extracted from kData and zero extended to 16 bytes.
  const uint8_t kExpectedIv[] = {
      0x0d, 0x0a, 0x0d, 0x0a, 0x0d, 0x0a, 0x0d, 0x0a,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };
  std::unique_ptr<DecryptConfig> decrypt_config;
  int data_offset;
  ASSERT_TRUE(WebMCreateDecryptConfig(kData, sizeof(kData), kKeyId,
                                      sizeof(kKeyId), &decrypt_config,
                                      &data_offset));
  EXPECT_TRUE(decrypt_config);
  EXPECT_EQ(std::string(kKeyId, kKeyId + sizeof(kKeyId)),
            decrypt_config->key_id());
  EXPECT_EQ(std::string(kExpectedIv, kExpectedIv + sizeof(kExpectedIv)),
            decrypt_config->iv());
  EXPECT_THAT(decrypt_config->subsamples(), ElementsAre(SubsampleEntry(6, 0)));
  EXPECT_EQ(10, data_offset);
}

}  // namespace media
