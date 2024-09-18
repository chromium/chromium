// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/safe_browsing_client_factory.h"

#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

class SafeBrowsingClientFactoryTest : public PlatformTest {
 protected:
  SafeBrowsingClientFactoryTest()
      : profile_(TestProfileIOS::Builder().Build()) {}

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<ProfileIOS> profile_;
};

// Checks that different instances are returned for recording and off the record
// profiles.
TEST_F(SafeBrowsingClientFactoryTest, DifferentClientInstances) {
  SafeBrowsingClient* recording_client =
      SafeBrowsingClientFactory::GetForProfile(profile_.get());
  SafeBrowsingClient* off_the_record_client =
      SafeBrowsingClientFactory::GetForProfile(
          profile_->GetOffTheRecordProfile());
  EXPECT_TRUE(recording_client);
  EXPECT_TRUE(off_the_record_client);
  EXPECT_NE(recording_client, off_the_record_client);
}
