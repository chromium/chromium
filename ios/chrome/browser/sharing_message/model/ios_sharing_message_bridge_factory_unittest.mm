// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sharing_message/model/ios_sharing_message_bridge_factory.h"

#import "components/sharing_message/sharing_message_bridge.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

// Test fixture for testing IOSSharingMessageBridgeFactory class.
class IOSSharingMessageBridgeFactoryTest : public PlatformTest {
 protected:
  IOSSharingMessageBridgeFactoryTest()
      : browser_state_(TestChromeBrowserState::Builder().Build()) {}

  // ChromeBrowserState needs thread.
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
};

// Tests that IOSSharingMessageBridgeFactory creates
// SharingMessageBridge.
TEST_F(IOSSharingMessageBridgeFactoryTest, CreateService) {
  SharingMessageBridge* service =
      IOSSharingMessageBridgeFactory::GetForBrowserState(browser_state_.get());
  ASSERT_TRUE(service);
}
