// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_notification/model/content_notification_service_factory.h"

#import "base/test/task_environment.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

class ContentNotificationServiceFactoryTest : public PlatformTest {
 public:
  ContentNotificationServiceFactoryTest()
      : profile_(TestProfileIOS::Builder().Build()) {}

  ProfileIOS* profile() { return profile_.get(); }

  ProfileIOS* otr_profile() { return profile_->GetOffTheRecordProfile(); }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
};

// Tests that the factory returns a non-null instance for regular BrowserStates.
TEST_F(ContentNotificationServiceFactoryTest, CreateInstance) {
  ContentNotificationService* const service =
      ContentNotificationServiceFactory::GetForProfile(profile());
  EXPECT_NE(service, nullptr);
}

// Tests that the factory returns a null instance for off-the-record
// BrowserStates.
TEST_F(ContentNotificationServiceFactoryTest, CreateOTRInstance) {
  ContentNotificationService* const service =
      ContentNotificationServiceFactory::GetForProfile(otr_profile());
  EXPECT_EQ(service, nullptr);
}
