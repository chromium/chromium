// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/public/cwv_identity.h"

#import <Foundation/Foundation.h>

#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"

namespace ios_web_view {

using CWVIdentityTest = PlatformTest;

// Tests CWVIdentity initialization.
TEST_F(CWVIdentityTest, Initialization) {
  NSString* email = @"john.doe@chromium.org";
  NSString* full_name = @"John Doe";
  NSString* gaia_id = @"123456789";

  CWVIdentity* identity = [[CWVIdentity alloc] initWithEmail:email
                                                    fullName:full_name
                                                      gaiaID:gaia_id];
  EXPECT_NSEQ(email, identity.email);
  EXPECT_NSEQ(full_name, identity.fullName);
  EXPECT_NSEQ(gaia_id, identity.gaiaID);
}

}  // namespace ios_web_view
