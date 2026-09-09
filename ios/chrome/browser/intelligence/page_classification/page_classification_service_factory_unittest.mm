// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/page_classification/page_classification_service_factory.h"

#import "ios/chrome/browser/intelligence/page_classification/page_classification_service.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

class PageClassificationServiceFactoryTest : public PlatformTest {
 public:
  PageClassificationServiceFactoryTest()
      : profile_(TestProfileIOS::Builder().Build()) {}

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
};

// Tests that factory creates a non-null instance for a standard profile.
TEST_F(PageClassificationServiceFactoryTest, TestCreateInstance) {
  PageClassificationService* service =
      PageClassificationServiceFactory::GetForProfile(profile_.get());
  EXPECT_NE(nullptr, service);
}

// Tests that factory returns null for an OffTheRecord profile.
TEST_F(PageClassificationServiceFactoryTest, TestNullForOffTheRecordProfile) {
  ProfileIOS* otr_profile = profile_->GetOffTheRecordProfile();
  PageClassificationService* service =
      PageClassificationServiceFactory::GetForProfile(otr_profile);
  EXPECT_EQ(nullptr, service);
}
