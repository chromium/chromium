// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/hashed_extension_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

TEST(HashedExtensionIdTest, Basic) {
  const std::string kExtensionId = "abcdefghijklmnopabcdefghijklmnop";
  const std::string kExpectedHash = "ACD66AF886BA7B085B41B4382BC39D1855BC18FE";

  EXPECT_EQ(kExpectedHash, HashedExtensionId(kExtensionId).value());
}

}  // namespace extensions
