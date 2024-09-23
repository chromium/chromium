// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sharing_message/model/ios_sharing_service_factory.h"

#import "base/test/scoped_feature_list.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/send_tab_to_self/features.h"
#import "components/sharing_message/pref_names.h"
#import "components/sharing_message/sharing_service.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/gcm/model/instance_id/ios_chrome_instance_id_profile_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/sync/model/device_info_sync_service_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

// Test fixture for testing IOSSharingServiceFactory class.
class IOSSharingServiceFactoryTest : public PlatformTest {
 protected:
  IOSSharingServiceFactoryTest() {
    scoped_feature_list_.InitAndEnableFeature(
        send_tab_to_self::kSendTabToSelfIOSPushNotifications);

    profile_ = TestProfileIOS::Builder().Build();
    profile_->GetSyncablePrefs()->SetDict(prefs::kSharingFCMRegistration,
                                          base::Value::Dict());
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
};

// Tests that IOSSharingServiceFactory creates
// SharingService.
TEST_F(IOSSharingServiceFactoryTest, CreateService) {
  SharingService* service =
      IOSSharingServiceFactory::GetForProfile(profile_.get());
  ASSERT_TRUE(service);
}
