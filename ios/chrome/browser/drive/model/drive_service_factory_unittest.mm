// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive/model/drive_service_factory.h"

#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

class DriveServiceFactoryTest : public PlatformTest {
 protected:
  DriveServiceFactoryTest() { profile_ = TestProfileIOS::Builder().Build(); }

  base::test::ScopedFeatureList scoped_feature_list_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
};

// Checks that the same instance is returned for on-the-record and
// off-the-record profiles.
TEST_F(DriveServiceFactoryTest, ProfileRedirectedInIncognito) {
  drive::DriveService* on_the_record_service =
      drive::DriveServiceFactory::GetForProfile(profile_.get());
  drive::DriveService* off_the_record_service =
      drive::DriveServiceFactory::GetForProfile(
          profile_->GetOffTheRecordProfile());
  EXPECT_TRUE(on_the_record_service != nullptr);
  EXPECT_EQ(on_the_record_service, off_the_record_service);
}
