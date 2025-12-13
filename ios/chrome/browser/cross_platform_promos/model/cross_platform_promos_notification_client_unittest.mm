// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cross_platform_promos/model/cross_platform_promos_notification_client.h"

#import "base/test/task_environment.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "testing/platform_test.h"

// Test suite for the `CrossPlatformPromosNotificationClient`.
class CrossPlatformPromosNotificationClientTest : public PlatformTest {
 public:
  CrossPlatformPromosNotificationClientTest() {
    profile_ = TestProfileIOS::Builder().Build();
    client_ =
        std::make_unique<CrossPlatformPromosNotificationClient>(profile_.get());
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<CrossPlatformPromosNotificationClient> client_;
};

// Tests that the client can be instantiated.
TEST_F(CrossPlatformPromosNotificationClientTest, Instantiate) {
  EXPECT_TRUE(client_);
}
