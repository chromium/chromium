// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/dom_distiller/model/distiller_service_factory.h"

#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

class DistillerServiceFactoryTest : public PlatformTest {
 protected:
  DistillerServiceFactoryTest() : profile_(TestProfileIOS::Builder().Build()) {}

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
};

// Checks that the same instance is returned for on-the-record and
// off-the-record profile.
TEST_F(DistillerServiceFactoryTest, ProfileRedirectedInIncognito) {
  DistillerService* on_the_record_service =
      DistillerServiceFactory::GetForProfile(profile_.get());
  DistillerService* off_the_record_service =
      DistillerServiceFactory::GetForProfile(
          profile_->GetOffTheRecordProfile());
  EXPECT_TRUE(on_the_record_service != nullptr);
  EXPECT_EQ(on_the_record_service, off_the_record_service);
}
