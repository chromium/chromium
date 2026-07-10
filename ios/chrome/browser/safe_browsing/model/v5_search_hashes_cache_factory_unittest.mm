// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/v5_search_hashes_cache_factory.h"

#import "components/safe_browsing/core/browser/db/v5_search_hashes_cache.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

class V5SearchHashesCacheFactoryTest : public PlatformTest {
 protected:
  V5SearchHashesCacheFactoryTest()
      : profile_(TestProfileIOS::Builder().Build()) {}

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<ProfileIOS> profile_;
};

// Checks that V5SearchHashesCacheFactory returns a non-null and distinct
// instance for an off-the-record profile.
TEST_F(V5SearchHashesCacheFactoryTest, EnabledForIncognitoMode) {
  EXPECT_TRUE(V5SearchHashesCacheFactory::GetForProfile(
      profile_->GetOffTheRecordProfile()));
  EXPECT_NE(V5SearchHashesCacheFactory::GetForProfile(profile_.get()),
            V5SearchHashesCacheFactory::GetForProfile(
                profile_->GetOffTheRecordProfile()));
}

// Checks that V5SearchHashesCacheFactory returns a non-null instance for a
// regular profile.
TEST_F(V5SearchHashesCacheFactoryTest, EnabledForRegularMode) {
  EXPECT_TRUE(V5SearchHashesCacheFactory::GetForProfile(profile_.get()));
}
