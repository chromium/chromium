// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/security/ssl_status.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace web {

using SSLStatusTest = PlatformTest;

// Tests the Equals() method of the SSLStatus class.
TEST_F(SSLStatusTest, SSLStatusEqualityTest) {
  SSLStatus status;
  EXPECT_EQ(SECURITY_STYLE_UNKNOWN, status.security_style);
  EXPECT_EQ(0u, status.cert_status);
  EXPECT_EQ(SSLStatus::NORMAL_CONTENT, status.content_status);

  // Verify that the Equals() method returns false if two SSLStatus objects
  // have different ContentStatusFlags.
  SSLStatus other_status;
  other_status.content_status =
      SSLStatus::ContentStatusFlags::DISPLAYED_INSECURE_CONTENT;
  EXPECT_FALSE(status.Equals(other_status));
  EXPECT_FALSE(other_status.Equals(status));

  // Verify a copied SSLStatus Equals() the original.
  SSLStatus copied_status = status;
  EXPECT_TRUE(status.Equals(copied_status));
  EXPECT_TRUE(copied_status.Equals(status));
}
}  // namespace web
