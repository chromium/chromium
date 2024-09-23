// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sharing_message/model/ios_sharing_message_bridge_factory.h"

#import "base/test/scoped_feature_list.h"
#import "components/send_tab_to_self/features.h"
#import "components/sharing_message/sharing_message_bridge.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

// Test fixture for testing IOSSharingMessageBridgeFactory class.
class IOSSharingMessageBridgeFactoryTest : public PlatformTest {
 protected:
  IOSSharingMessageBridgeFactoryTest() {
    scoped_feature_list_.InitAndEnableFeature(
        send_tab_to_self::kSendTabToSelfIOSPushNotifications);
    profile_ = TestProfileIOS::Builder().Build();
  }

  // ProfileIOS needs thread.
  base::test::ScopedFeatureList scoped_feature_list_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
};

// Tests that IOSSharingMessageBridgeFactory creates
// SharingMessageBridge.
TEST_F(IOSSharingMessageBridgeFactoryTest, CreateService) {
  SharingMessageBridge* service =
      IOSSharingMessageBridgeFactory::GetForProfile(profile_.get());
  ASSERT_TRUE(service);
}
