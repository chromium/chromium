// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/photos/model/photos_service_factory.h"

#import "base/test/task_environment.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "testing/platform_test.h"

class PhotosServiceFactoryTest : public PlatformTest {
 protected:
  PhotosServiceFactoryTest() : profile_(TestProfileIOS::Builder().Build()) {}

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
};

// Checks that the same instance is returned for on-the-record and
// off-the-record profile.
TEST_F(PhotosServiceFactoryTest, ProfileRedirectedInIncognito) {
  PhotosService* on_the_record_service =
      PhotosServiceFactory::GetForProfile(profile_.get());
  PhotosService* off_the_record_service =
      PhotosServiceFactory::GetForProfile(profile_->GetOffTheRecordProfile());
  EXPECT_TRUE(on_the_record_service != nullptr);
  EXPECT_EQ(on_the_record_service, off_the_record_service);
}
