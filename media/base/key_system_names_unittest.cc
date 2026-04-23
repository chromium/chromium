// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/key_system_names.h"

#include "media/cdm/clear_key_cdm_common.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(KeySystemNamesTest, IsClearKey) {
  EXPECT_TRUE(IsClearKey(kClearKeyKeySystem));
  EXPECT_FALSE(IsClearKey("org.w3.clearkeys"));
  EXPECT_FALSE(IsClearKey("org.w3.clearkey.something"));
  EXPECT_FALSE(IsClearKey(kExternalClearKeyKeySystem));
  EXPECT_FALSE(IsClearKey(""));
}

TEST(KeySystemNamesTest, IsSubKeySystemOf) {
  EXPECT_TRUE(IsSubKeySystemOf("a.b.c", "a.b"));
  EXPECT_TRUE(IsSubKeySystemOf("a.b.c", "a"));
  EXPECT_FALSE(IsSubKeySystemOf("a.b.c", "a.b.c"));
  EXPECT_FALSE(IsSubKeySystemOf("a.b", "a.b.c"));
  EXPECT_FALSE(IsSubKeySystemOf("a.bc", "a.b"));
  EXPECT_FALSE(IsSubKeySystemOf("a", "a"));
  EXPECT_FALSE(IsSubKeySystemOf("", "a"));
  EXPECT_FALSE(IsSubKeySystemOf("a", ""));
}

TEST(KeySystemNamesTest, IsExternalClearKey) {
  EXPECT_TRUE(IsExternalClearKey(kExternalClearKeyKeySystem));
  EXPECT_TRUE(IsExternalClearKey("org.chromium.externalclearkey.something"));
  EXPECT_FALSE(IsExternalClearKey("org.chromium.externalclearkeys"));
  EXPECT_FALSE(IsExternalClearKey(kClearKeyKeySystem));
  EXPECT_FALSE(IsExternalClearKey(""));
}

}  // namespace media
