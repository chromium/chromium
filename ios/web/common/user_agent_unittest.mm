// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/common/user_agent.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

using UserAgentTest = PlatformTest;

// Tests conversions between UserAgentType values and their descriptions
TEST_F(UserAgentTest, UserAgentTypeDescription) {
  const std::string kMobileDescription("MOBILE");
  const std::string kDesktopDescription("DESKTOP");
  const std::string kNoneDescription("NONE");
  const std::string kInvalidDescription(
      "not returned by GetUserAgentTypeDescription()");
  EXPECT_EQ(kMobileDescription,
            GetUserAgentTypeDescription(UserAgentType::MOBILE));
  EXPECT_EQ(kDesktopDescription,
            GetUserAgentTypeDescription(UserAgentType::DESKTOP));
  EXPECT_EQ(kNoneDescription, GetUserAgentTypeDescription(UserAgentType::NONE));
  EXPECT_EQ(UserAgentType::MOBILE,
            GetUserAgentTypeWithDescription(kMobileDescription));
  EXPECT_EQ(UserAgentType::DESKTOP,
            GetUserAgentTypeWithDescription(kDesktopDescription));
  EXPECT_EQ(UserAgentType::NONE,
            GetUserAgentTypeWithDescription(kNoneDescription));
  EXPECT_EQ(UserAgentType::NONE,
            GetUserAgentTypeWithDescription(kInvalidDescription));
}

}  // namespace web
