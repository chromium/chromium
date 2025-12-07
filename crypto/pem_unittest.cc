// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/pem.h"

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/string_view_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/pki/pem.h"

namespace {

base::FilePath GetTestDataPath(std::string_view filename) {
  return base::PathService::CheckedGet(base::DIR_SRC_TEST_DATA_ROOT)
      .AppendASCII("crypto")
      .AppendASCII("test")
      .AppendASCII("data")
      .AppendASCII(filename);
}

TEST(PemTest, SingleMessage) {
  std::optional<std::vector<uint8_t>> result =
      crypto::pem::SingleMessageFromFile(GetTestDataPath("one-message.pem"),
                                         "SINGLE MESSAGE");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(base::as_string_view(*result), "crypto test data single message");
}

TEST(PemTest, MultipleMessages) {
  std::vector<bssl::PEMToken> result = crypto::pem::MessagesFromFile(
      GetTestDataPath("three-messages.pem"), {"TYPE ONE"});
  ASSERT_EQ(result.size(), 2u);
  EXPECT_EQ(result[0].data, "crypto test data message one");
  EXPECT_EQ(result[1].data, "crypto test data message three");
}

void ExpectReadsFail(std::string_view filename) {
  std::vector<bssl::PEMToken> multi_result =
      crypto::pem::MessagesFromFile(GetTestDataPath(filename), {"BLOCK"});
  EXPECT_EQ(multi_result.size(), 0u);

  std::optional<std::vector<uint8_t>> single_result =
      crypto::pem::SingleMessageFromFile(GetTestDataPath(filename), "BLOCK");
  EXPECT_FALSE(single_result.has_value());
}

TEST(PemTest, NoMessages) {
  ExpectReadsFail("not-pem.txt");
}

TEST(PemTest, MissingFile) {
  ExpectReadsFail("does-not-exist.pem");
}

}  // namespace
