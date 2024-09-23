// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/common/user_agent.h"

#import "base/strings/stringprintf.h"
#import "base/system/sys_info.h"
#import "ios/web/common/features.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "ui/base/device_form_factor.h"

namespace {
const char kDesktopUserAgentWithProduct[] =
    "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) "
    "AppleWebKit/605.1.15 (KHTML, like Gecko) desktop_product_name "
    "Version/11.1.1 "
    "Safari/605.1.15";

const char kDesktopUserAgent[] =
    "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) "
    "AppleWebKit/605.1.15 (KHTML, like Gecko) "
    "Version/11.1.1 "
    "Safari/605.1.15";
}  // namespace

namespace web {

using UserAgentTest = PlatformTest;

// Tests conversions between UserAgentType values and their descriptions
TEST_F(UserAgentTest, UserAgentTypeDescription) {
  const std::string kMobileDescription("MOBILE");
  const std::string kDesktopDescription("DESKTOP");
  const std::string kAutomaticDescription("AUTOMATIC");
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
  EXPECT_EQ(kAutomaticDescription,
            GetUserAgentTypeDescription(UserAgentType::AUTOMATIC));
  EXPECT_EQ(UserAgentType::AUTOMATIC,
            GetUserAgentTypeWithDescription(kAutomaticDescription));
}

// Tests the mobile user agent returned for a specific product.
TEST_F(UserAgentTest, MobileUserAgentForProduct) {
  std::string product = "my_product_name";

  std::string platform;
  std::string cpu;
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    platform = "iPad";
    cpu = "OS";
  } else {
    platform = "iPhone";
    cpu = "iPhone OS";
  }

  std::string os_version;
  int32_t os_major_version = 0;
  int32_t os_minor_version = 0;
  int32_t os_bugfix_version = 0;
  base::SysInfo::OperatingSystemVersionNumbers(
      &os_major_version, &os_minor_version, &os_bugfix_version);
  if (base::FeatureList::IsEnabled(web::features::kUserAgentBugFixVersion)) {
    base::StringAppendF(&os_version, "%d_%d_%d", os_major_version,
                        os_minor_version, os_bugfix_version);
  } else {
    base::StringAppendF(&os_version, "%d_%d", os_major_version,
                        os_minor_version);
  }

  std::string expected_user_agent;
  base::StringAppendF(
      &expected_user_agent,
      "Mozilla/5.0 (%s; CPU %s %s like Mac OS X) AppleWebKit/605.1.15 (KHTML, "
      "like Gecko) %s Mobile/15E148 Safari/604.1",
      platform.c_str(), cpu.c_str(), os_version.c_str(), product.c_str());

  std::string result = BuildMobileUserAgent(product);

  EXPECT_EQ(expected_user_agent, result);
}

// Tests the desktop user agent, checking that the product isn't taken into
// account when it is empty.
TEST_F(UserAgentTest, DesktopUserAgentForProduct) {
  EXPECT_EQ(kDesktopUserAgent, BuildDesktopUserAgent(""));
}

// Tests the desktop user agent for a specific product name.
TEST_F(UserAgentTest, DesktopUserAgentWithProduct) {
  EXPECT_EQ(kDesktopUserAgentWithProduct,
            BuildDesktopUserAgent("desktop_product_name"));
}

}  // namespace web
