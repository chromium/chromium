// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/test/task_environment.h"
#import "ios/web_view/internal/passwords/cwv_password_internal.h"

#import "base/strings/sys_string_conversions.h"
#import "components/password_manager/core/browser/password_ui_utils.h"

#import "ios/web_view/internal/passwords/cwv_weak_check_utils_internal.h"

#import "services/network/test/test_shared_url_loader_factory.h"

#import "testing/gtest/include/gtest/gtest.h"

#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

namespace ios_web_view {

class CWVWeakCheckUtilsTest : public PlatformTest {
 public:
  CWVWeakCheckUtilsTest() {
    weakPassword_ = @"weak";
    strongPassword_ = @"600613Longpasswordthatwouldbeconsideredstrong600613";
  }
  NSString* weakPassword_;
  NSString* strongPassword_;
};

// Tests that weak passwords are identified as weak
TEST_F(CWVWeakCheckUtilsTest, WeakPassword) {
  BOOL isWeak = [CWVWeakCheckUtils isPasswordWeak:weakPassword_];
  EXPECT_EQ(YES, isWeak);
}

// Tests that strong passwords aren't identified as weak
TEST_F(CWVWeakCheckUtilsTest, StrongPassword) {
  BOOL isWeak = [CWVWeakCheckUtils isPasswordWeak:strongPassword_];
  EXPECT_EQ(NO, isWeak);
}

}  // namespace ios_web_view
