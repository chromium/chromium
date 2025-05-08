// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/photos/model/photos_service_factory.h"

#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

class PhotosServiceFactoryTest : public PlatformTest {
 protected:
  PhotosServiceFactoryTest() : profile_(TestProfileIOS::Builder().Build()) {}

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
};

// Tests that PhotosServiceFactory does not create PhotosService
// for TestProfileIOS.
TEST_F(PhotosServiceFactoryTest, NoServiceForTests) {
  PhotosService* service = PhotosServiceFactory::GetForProfile(profile_.get());
  EXPECT_FALSE(service);
}
