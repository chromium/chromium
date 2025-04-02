// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/chrome_enterprise_url_lookup_service_factory.h"

#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

namespace safe_browsing {

class ChromeEnterpriseRealTimeUrlLookupServiceFactoryTest
    : public PlatformTest {
 protected:
  ChromeEnterpriseRealTimeUrlLookupServiceFactoryTest()
      : profile_(TestProfileIOS::Builder().Build()) {}

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<ProfileIOS> profile_;
};

// Checks that ChromeEnterpriseRealTimeUrlLookupServiceFactory returns a null
// for an off-the-record profile, but returns a non-null instance for a regular
// profile.
TEST_F(ChromeEnterpriseRealTimeUrlLookupServiceFactoryTest,
       OffTheRecordReturnsNull) {
  // The factory should return null for an off-the-record profile.
  EXPECT_FALSE(ChromeEnterpriseRealTimeUrlLookupServiceFactory::GetForProfile(
      profile_->GetOffTheRecordProfile()));

  // There should be a non-null instance for a regular profile.
  EXPECT_TRUE(ChromeEnterpriseRealTimeUrlLookupServiceFactory::GetForProfile(
      profile_.get()));
}

}  // namespace safe_browsing
