// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/credential_provider/favicon_file_key.h"

#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

using FaviconFileKeyTest = PlatformTest;

// Tests that IsValidFaviconFileKey correctly validates favicon key hashes.
TEST_F(FaviconFileKeyTest, IsValidFaviconFileKey) {
  // Valid 64-character hex key.
  NSString* valid_key =
      @"BD7639C34EA3480A8AAD704306C8870161761506AD948AC6FA037B83CFF22D37";
  EXPECT_TRUE(IsValidFaviconFileKey(valid_key));
  EXPECT_TRUE(IsValidFaviconFileKey([valid_key lowercaseString]));

  // Nil or empty.
  EXPECT_FALSE(IsValidFaviconFileKey(nil));
  EXPECT_FALSE(IsValidFaviconFileKey(@""));

  // Length mismatch.
  EXPECT_FALSE(IsValidFaviconFileKey(@"ABCD"));
  EXPECT_FALSE(IsValidFaviconFileKey([valid_key stringByAppendingString:@"A"]));

  // Non-hex character.
  EXPECT_FALSE(IsValidFaviconFileKey(
      @"ZZ7639C34EA3480A8AAD704306C8870161761506AD948AC6FA037B83CFF22D37"));
}

}  // namespace
