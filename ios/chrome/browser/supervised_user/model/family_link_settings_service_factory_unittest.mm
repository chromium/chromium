// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/supervised_user/model/family_link_settings_service_factory.h"

#import "components/supervised_user/core/browser/family_link_settings_service.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

namespace supervised_user {
namespace {
// Test fixture for testing FamilyLinkSettingsServiceFactory class.
class FamilyLinkSettingsServiceFactoryTest : public PlatformTest {
 protected:
  FamilyLinkSettingsServiceFactoryTest()
      : profile_(TestProfileIOS::Builder().Build()) {}

  // ProfileIOS needs thread.
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<ProfileIOS> profile_;
};

// Tests that FamilyLinkSettingsServiceFactory creates
// FamilyLinkSettingsService.
TEST_F(FamilyLinkSettingsServiceFactoryTest, CreateService) {
  supervised_user::FamilyLinkSettingsService* service =
      FamilyLinkSettingsServiceFactory::GetForProfile(profile_.get());
  ASSERT_TRUE(service);
}
}  // namespace
}  // namespace supervised_user
