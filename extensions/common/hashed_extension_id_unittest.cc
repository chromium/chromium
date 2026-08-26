// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/hashed_extension_id.h"

#include "base/test/scoped_feature_list.h"
#include "extensions/common/extension_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

TEST(HashedExtensionIdTest, Basic) {
  const std::string kExtensionId = "abcdefghijklmnopabcdefghijklmnop";
  const std::string kExpectedSha1Hash =
      "ACD66AF886BA7B085B41B4382BC39D1855BC18FE";
  const std::string kExpectedSha256Hash =
      "DF43A9994ADFFA1484525827764397252D90EF618363E5D381A158AC38B11A8A";

  // When feature is disabled (default), expect SHA-1 hash for value().
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(
        extensions_features::kUseSha256ForExtensionHashes);
    HashedExtensionId hashed_id(kExtensionId);
    EXPECT_EQ(kExpectedSha1Hash, hashed_id.value());
    EXPECT_EQ(kExpectedSha1Hash, hashed_id.value_sha1());
    EXPECT_EQ(kExpectedSha256Hash, hashed_id.value_sha256());
  }

  // When feature is enabled, expect SHA-256 hash for value().
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(
        extensions_features::kUseSha256ForExtensionHashes);
    HashedExtensionId hashed_id(kExtensionId);
    EXPECT_EQ(kExpectedSha256Hash, hashed_id.value());
    EXPECT_EQ(kExpectedSha1Hash, hashed_id.value_sha1());
    EXPECT_EQ(kExpectedSha256Hash, hashed_id.value_sha256());
  }
}

}  // namespace extensions
