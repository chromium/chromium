// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/content_hash_tree.h"

#include <memory>

#include "crypto/secure_hash.h"
#include "crypto/sha2.h"
#include "testing/gtest/include/gtest/gtest.h"

using crypto::kSHA256Length;
using crypto::SecureHash;

// Helper to return a fake sha256 signature based on a seed.
static std::string FakeSignatureWithSeed(int seed) {
  std::string input;
  for (int i = 0; i < seed * 3; i++) {
    input.push_back(static_cast<char>(((i + 19) * seed) % 256));
  }
  return crypto::SHA256HashString(input);
}

namespace extensions {

TEST(ContentHashTreeTest, HashTreeBasics) {
  std::vector<std::string> nodes;
  // Empty array.
  EXPECT_EQ(std::string(), ComputeTreeHashRoot(nodes, 16));

  // One node.
  std::string node1 = FakeSignatureWithSeed(1);
  nodes.push_back(node1);
  EXPECT_EQ(node1, ComputeTreeHashRoot(nodes, 16));

  // Two nodes.
  std::string node2 = FakeSignatureWithSeed(2);
  nodes.push_back(node2);

  std::string expected(kSHA256Length, 0);
  std::unique_ptr<SecureHash> hash(SecureHash::Create(SecureHash::SHA256));
  hash->Update(node1.data(), node1.size());
  hash->Update(node2.data(), node2.size());
  hash->Finish(std::data(expected), expected.size());
  EXPECT_EQ(expected, ComputeTreeHashRoot(nodes, 16));
}

TEST(ContentHashTreeTest, HashTreeMultipleLevels) {
  std::vector<std::string> nodes;
  for (int i = 0; i < 3; i++) {
    std::string node;
    nodes.push_back(FakeSignatureWithSeed(i));
  }

  // First try a test where our branch factor is >= 3, so we expect the result
  // to be the hash of all 3 concatenated together. E.g the expected top hash
  // should be 4 in the following diagram:
  //   4
  // 1 2 3
  std::string expected =
      crypto::SHA256HashString(nodes[0] + nodes[1] + nodes[2]);
  EXPECT_EQ(expected, ComputeTreeHashRoot(nodes, 4));

  // Now try making the branch factor be 2, so that we
  // should get the following:
  //   6
  //  4 5
  // 1 2 3
  // where 4 is the hash of 1 and 2, 5 is the hash of 3, and 6 is the
  // hash of 4 and 5.
  std::string hash_of_first_2 = crypto::SHA256HashString(nodes[0] + nodes[1]);
  std::string hash_of_third = crypto::SHA256HashString(nodes[2]);
  expected = crypto::SHA256HashString(hash_of_first_2 + hash_of_third);
  EXPECT_EQ(expected, ComputeTreeHashRoot(nodes, 2));
}

}  // namespace extensions
