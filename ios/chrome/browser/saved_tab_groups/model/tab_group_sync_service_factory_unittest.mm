// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/saved_tab_groups/model/tab_group_sync_service_factory.h"

#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_sync_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "ui/base/device_form_factor.h"

namespace tab_groups {

class TabGroupSyncServiceFactoryTest : public PlatformTest {
 public:
  TabGroupSyncServiceFactoryTest() {
    TestProfileIOS::Builder builder;
    profile_ = std::move(builder).Build();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
};

// Tests that TabGroupSyncServiceFactory does not create TabGroupSyncService
// for TestProfileIOS.
TEST_F(TabGroupSyncServiceFactoryTest, NoServiceForTests) {
  TabGroupSyncService* service =
      TabGroupSyncServiceFactory::GetForProfile(profile_.get());
  EXPECT_FALSE(service);
}

}  // namespace tab_groups
