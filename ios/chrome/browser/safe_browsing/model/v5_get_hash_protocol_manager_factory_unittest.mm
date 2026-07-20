// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/v5_get_hash_protocol_manager_factory.h"

#import "components/safe_browsing/core/browser/db/v5_get_hash_protocol_manager.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

class V5GetHashProtocolManagerFactoryTest : public PlatformTest {
 protected:
  V5GetHashProtocolManagerFactoryTest()
      : profile_(TestProfileIOS::Builder().Build()) {}

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<ProfileIOS> profile_;
};

TEST_F(V5GetHashProtocolManagerFactoryTest, EnabledForRegularMode) {
  EXPECT_NE(nullptr,
            V5GetHashProtocolManagerFactory::GetForProfile(profile_.get()));
}

TEST_F(V5GetHashProtocolManagerFactoryTest, EnabledForIncognitoMode) {
  EXPECT_NE(nullptr, V5GetHashProtocolManagerFactory::GetForProfile(
                         profile_->GetOffTheRecordProfile()));
  EXPECT_NE(V5GetHashProtocolManagerFactory::GetForProfile(profile_.get()),
            V5GetHashProtocolManagerFactory::GetForProfile(
                profile_->GetOffTheRecordProfile()));
}
