// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/data_protection/model/data_protection_tab_helper_observer_bridge.h"

#import "ios/chrome/browser/enterprise/data_protection/model/data_protection_tab_helper.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

class DataProtectionTabHelperObserverBridgeTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    TestProfileIOS::Builder builder;
    profile_ = std::move(builder).Build();
    web_state_.SetBrowserState(profile_.get());
    DataProtectionTabHelper::CreateForWebState(&web_state_);
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  web::FakeWebState web_state_;
};

// Tests that the bridge correctly forwards the ScreenshotProtectionDidChange
// event to the Objective-C observer.
TEST_F(DataProtectionTabHelperObserverBridgeTest,
       ForwardsScreenshotProtectionDidChange) {
  id mock_observer =
      OCMStrictProtocolMock(@protocol(DataProtectionTabHelperObserving));
  DataProtectionTabHelper* helper =
      DataProtectionTabHelper::FromWebState(&web_state_);

  DataProtectionTabHelperObserverBridge bridge(mock_observer, helper);

  OCMExpect([mock_observer screenshotProtectionDidChangeForWebState:&web_state_
                                                        isProtected:YES]);

  // Trigger the observer directly using the bridge method to ensure it forwards
  bridge.ScreenshotProtectionDidChange(&web_state_, true);

  EXPECT_OCMOCK_VERIFY(mock_observer);
}

// Tests that the bridge handles the destruction of the helper gracefully.
TEST_F(DataProtectionTabHelperObserverBridgeTest,
       UnobservesOnHelperDestruction) {
  id mock_observer =
      OCMStrictProtocolMock(@protocol(DataProtectionTabHelperObserving));
  DataProtectionTabHelper* helper =
      DataProtectionTabHelper::FromWebState(&web_state_);

  auto bridge = std::make_unique<DataProtectionTabHelperObserverBridge>(
      mock_observer, helper);

  // Destroy the helper by removing it from the WebState.
  // This calls DataProtectionTabHelperDestroyed on observers.
  DataProtectionTabHelper::RemoveFromWebState(&web_state_);

  // Destroying the bridge should not crash or trigger a Use-After-Free
  // on the ScopedObservation since it should have reset during the
  // DataProtectionTabHelperDestroyed callback.
  bridge.reset();
}
