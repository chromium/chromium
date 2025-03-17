// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autocomplete/model/remote_suggestions_service_factory.h"

#import "base/test/task_environment.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "testing/platform_test.h"

class RemoteSuggestionsServiceFactoryTest : public PlatformTest {
 protected:
  RemoteSuggestionsServiceFactoryTest()
      : profile_(TestProfileIOS::Builder().Build()) {}

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
};

// Ensures that a service instance is created for the expected Profile types.
TEST_F(RemoteSuggestionsServiceFactoryTest, ServiceInstance) {
  EXPECT_TRUE(RemoteSuggestionsServiceFactory::GetForProfile(
      profile_.get(), /*create_if_necessary=*/true));
  EXPECT_TRUE(RemoteSuggestionsServiceFactory::GetForProfile(
      profile_->GetOffTheRecordProfile(), /*create_if_necessary=*/true));
}
