// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/secure_hash.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <utility>

#include "base/types/fixed_array.h"
#include "crypto/sha2.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/sha.h"

class SecureHashTest : public testing::Test,
                       public testing::WithParamInterface<
                           std::pair<crypto::SecureHash::Algorithm, uint64_t>> {
 public:
  SecureHashTest()
      : algorithm_(GetParam().first), hash_length_(GetParam().second) {}

 protected:
  crypto::SecureHash::Algorithm algorithm_;
  const uint64_t hash_length_;
};

TEST_P(SecureHashTest, TestUpdateSHA256) {
  std::string input3;
  std::vector<uint8_t> expected_hash_of_input_3;

  switch (algorithm_) {
    case crypto::SecureHash::SHA256:
      // Example B.3 from FIPS 180-2: long message.
      input3 = std::string(500000, 'a');  // 'a' repeated half a million times
      expected_hash_of_input_3 = {
          0xcd, 0xc7, 0x6e, 0x5c, 0x99, 0x14, 0xfb, 0x92, 0x81, 0xa1, 0xc7,
          0xe2, 0x84, 0xd7, 0x3e, 0x67, 0xf1, 0x80, 0x9a, 0x48, 0xa4, 0x97,
          0x20, 0x0e, 0x04, 0x6d, 0x39, 0xcc, 0xc7, 0x11, 0x2c, 0xd0};
      break;
    case crypto::SecureHash::SHA512:
      // Example C.3 from FIPS 180-2: long message.
      input3 = std::string(500000, 'a');  // 'a' repeated half a million times
      expected_hash_of_input_3 = {
          0xe7, 0x18, 0x48, 0x3d, 0x0c, 0xe7, 0x69, 0x64, 0x4e, 0x2e, 0x42,
          0xc7, 0xbc, 0x15, 0xb4, 0x63, 0x8e, 0x1f, 0x98, 0xb1, 0x3b, 0x20,
          0x44, 0x28, 0x56, 0x32, 0xa8, 0x03, 0xaf, 0xa9, 0x73, 0xeb, 0xde,
          0x0f, 0xf2, 0x44, 0x87, 0x7e, 0xa6, 0x0a, 0x4c, 0xb0, 0x43, 0x2c,
          0xe5, 0x77, 0xc3, 0x1b, 0xeb, 0x00, 0x9c, 0x5c, 0x2c, 0x49, 0xaa,
          0x2e, 0x4e, 0xad, 0xb2, 0x17, 0xad, 0x8c, 0xc0, 0x9b};
      break;
  }

  base::FixedArray<uint8_t> output3(hash_length_);

  std::unique_ptr<crypto::SecureHash> ctx(
      crypto::SecureHash::Create(algorithm_));
  ctx->Update(input3.data(), input3.size());
  ctx->Update(input3.data(), input3.size());

  ctx->Finish(output3.data(), output3.size());
  for (size_t i = 0; i < hash_length_; i++)
    EXPECT_EQ(expected_hash_of_input_3[i], static_cast<int>(output3[i]));
}

TEST_P(SecureHashTest, TestClone) {
  std::string input1(10001, 'a');  // 'a' repeated 10001 times
  std::string input2(10001, 'd');  // 'd' repeated 10001 times

  std::vector<uint8_t> expected_hash_of_input_1;
  std::vector<uint8_t> expected_hash_of_input_1_and_2;

  switch (algorithm_) {
    case crypto::SecureHash::SHA256:
      expected_hash_of_input_1 = {
          0x0c, 0xab, 0x99, 0xa0, 0x58, 0x60, 0x0f, 0xfa, 0xad, 0x12, 0x92,
          0xd0, 0xc5, 0x3c, 0x05, 0x48, 0xeb, 0xaf, 0x88, 0xdd, 0x1d, 0x01,
          0x03, 0x03, 0x45, 0x70, 0x5f, 0x01, 0x8a, 0x81, 0x39, 0x09};
      expected_hash_of_input_1_and_2 = {
          0x4c, 0x8e, 0x26, 0x5a, 0xc3, 0x85, 0x1f, 0x1f, 0xa5, 0x04, 0x1c,
          0xc7, 0x88, 0x53, 0x1c, 0xc7, 0x80, 0x47, 0x15, 0xfb, 0x47, 0xff,
          0x72, 0xb1, 0x28, 0x37, 0xb0, 0x4d, 0x6e, 0x22, 0x2e, 0x4d};
      break;
    case crypto::SecureHash::SHA512:
      expected_hash_of_input_1 = {
          0xea, 0x03, 0xb2, 0x23, 0x32, 0x29, 0xc8, 0x87, 0x86, 0x33, 0xa3,
          0x70, 0xc7, 0xb2, 0x40, 0xea, 0xef, 0xd9, 0x55, 0xe2, 0xb3, 0x79,
          0xd6, 0xb3, 0x3f, 0x5e, 0xff, 0x89, 0xfd, 0x86, 0x7b, 0x10, 0xe2,
          0xc1, 0x3b, 0x2f, 0xf5, 0x29, 0x80, 0xa0, 0xb0, 0xf9, 0xcf, 0x47,
          0xa7, 0xff, 0x73, 0xac, 0xd2, 0x66, 0x9e, 0x53, 0x78, 0x9f, 0xc6,
          0x07, 0x7a, 0xb7, 0x09, 0x1f, 0xa4, 0x3b, 0x18, 0x00};
      expected_hash_of_input_1_and_2 = {
          0x41, 0x6d, 0x46, 0x8d, 0x8a, 0x84, 0x3d, 0xf9, 0x43, 0xac, 0xe6,
          0x4d, 0x5b, 0x60, 0xd7, 0x1a, 0xb1, 0xe6, 0x2d, 0xd3, 0xe6, 0x97,
          0xaf, 0x6f, 0x34, 0x97, 0x8f, 0x01, 0xd4, 0x15, 0x06, 0xfa, 0x69,
          0x48, 0x0e, 0x24, 0x0d, 0x98, 0x84, 0x76, 0xd2, 0x95, 0x4c, 0x16,
          0x02, 0xfd, 0x71, 0xd4, 0x25, 0xb3, 0x8f, 0xf2, 0x60, 0xa3, 0x0e,
          0xdb, 0xe9, 0x87, 0x32, 0xfc, 0xf3, 0x2d, 0x0a, 0x28};
      break;
  }

  base::FixedArray<uint8_t> output1(hash_length_);
  base::FixedArray<uint8_t> output2(hash_length_);
  base::FixedArray<uint8_t> output3(hash_length_);

  std::unique_ptr<crypto::SecureHash> ctx1(
      crypto::SecureHash::Create(algorithm_));
  ctx1->Update(input1.data(), input1.size());

  std::unique_ptr<crypto::SecureHash> ctx2(ctx1->Clone());
  std::unique_ptr<crypto::SecureHash> ctx3(ctx2->Clone());
  // At this point, ctx1, ctx2, and ctx3 are all equivalent and represent the
  // state after hashing input1.

  // Updating ctx1 and ctx2 with input2 should produce equivalent results.
  ctx1->Update(input2.data(), input2.size());
  ctx1->Finish(output1.data(), output1.size());

  ctx2->Update(input2.data(), input2.size());
  ctx2->Finish(output2.data(), output2.size());

  EXPECT_EQ(0, memcmp(output1.data(), output2.data(), hash_length_));
  EXPECT_EQ(0, memcmp(output1.data(), expected_hash_of_input_1_and_2.data(),
                      hash_length_));

  // Finish() ctx3, which should produce the hash of input1.
  ctx3->Finish(output3.data(), output3.size());
  EXPECT_EQ(
      0, memcmp(output3.data(), expected_hash_of_input_1.data(), hash_length_));
}

TEST_P(SecureHashTest, TestLength) {
  std::unique_ptr<crypto::SecureHash> ctx(
      crypto::SecureHash::Create(algorithm_));
  EXPECT_EQ(hash_length_, ctx->GetHashLength());
}

TEST_P(SecureHashTest, Equality) {
  std::string input1(10001, 'a');  // 'a' repeated 10001 times
  std::string input2(10001, 'd');  // 'd' repeated 10001 times

  base::FixedArray<uint8_t> output1(hash_length_);
  base::FixedArray<uint8_t> output2(hash_length_);

  // Call Update() twice on input1 and input2.
  std::unique_ptr<crypto::SecureHash> ctx1(
      crypto::SecureHash::Create(algorithm_));
  ctx1->Update(input1.data(), input1.size());
  ctx1->Update(input2.data(), input2.size());
  ctx1->Finish(output1.data(), output1.size());

  // Call Update() once one input1 + input2 (concatenation).
  std::unique_ptr<crypto::SecureHash> ctx2(
      crypto::SecureHash::Create(algorithm_));
  std::string input3 = input1 + input2;
  ctx2->Update(input3.data(), input3.size());
  ctx2->Finish(output2.data(), output2.size());

  // The hash should be the same.
  EXPECT_EQ(0, memcmp(output1.data(), output2.data(), hash_length_));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SecureHashTest,
    testing::Values(
        std::make_pair(crypto::SecureHash::SHA256, SHA256_DIGEST_LENGTH),
        std::make_pair(crypto::SecureHash::SHA512, SHA512_DIGEST_LENGTH)));
