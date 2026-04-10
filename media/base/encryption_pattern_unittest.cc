// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/encryption_pattern.h"

#include "base/test/scoped_feature_list.h"
#include "media/base/media_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(EncryptionPatternTest, CreateWithValidationEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kValidateEncryptionPatternSize);

  // Valid patterns (0-15)
  EXPECT_TRUE(EncryptionPattern::Create(0, 0).has_value());
  EXPECT_TRUE(EncryptionPattern::Create(1, 9).has_value());
  EXPECT_TRUE(EncryptionPattern::Create(15, 15).has_value());

  // Invalid patterns (> 15)
  EXPECT_FALSE(EncryptionPattern::Create(16, 0).has_value());
  EXPECT_FALSE(EncryptionPattern::Create(0, 16).has_value());
  EXPECT_FALSE(EncryptionPattern::Create(100, 100).has_value());
}

TEST(EncryptionPatternTest, CreateWithValidationDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kValidateEncryptionPatternSize);

  // Valid patterns (0-15)
  EXPECT_TRUE(EncryptionPattern::Create(0, 0).has_value());
  EXPECT_TRUE(EncryptionPattern::Create(1, 9).has_value());
  EXPECT_TRUE(EncryptionPattern::Create(15, 15).has_value());

  // Invalid patterns (> 15)
  EXPECT_TRUE(EncryptionPattern::Create(16, 0).has_value());
  EXPECT_TRUE(EncryptionPattern::Create(0, 16).has_value());
  EXPECT_TRUE(EncryptionPattern::Create(100, 100).has_value());
}

}  // namespace media
