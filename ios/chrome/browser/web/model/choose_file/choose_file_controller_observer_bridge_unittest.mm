// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/choose_file/choose_file_controller_observer_bridge.h"

#import "ios/chrome/browser/web/model/choose_file/choose_file_event.h"
#import "ios/chrome/browser/web/model/choose_file/fake_choose_file_controller.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMArg.h"
#import "third_party/ocmock/OCMock/OCMStubRecorder.h"
#import "third_party/ocmock/gtest_support.h"
#import "third_party/ocmock/ocmock_extensions.h"

// Test suite for ChooseFileControllerObserverBridge.
class ChooseFileControllerObserverBridgeTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();
    web_state_ = std::make_unique<web::FakeWebState>();
    ChooseFileEvent event =
        ChooseFileEvent::Builder().SetWebState(web_state_.get()).Build();
    controller_ = std::make_unique<FakeChooseFileController>(event);
    observer_ = OCMProtocolMock(@protocol(ChooseFileControllerObserving));
    bridge_ = std::make_unique<ChooseFileControllerObserverBridge>(observer_);
    controller_->AddObserver(bridge_.get());
  }

 protected:
  std::unique_ptr<web::FakeWebState> web_state_;
  std::unique_ptr<FakeChooseFileController> controller_;
  id<ChooseFileControllerObserving> observer_;
  std::unique_ptr<ChooseFileControllerObserverBridge> bridge_;
};

// Tests that destroying the controller calls the observer.
TEST_F(ChooseFileControllerObserverBridgeTest, ControllerDestroyed) {
  ChooseFileController* controller = controller_.get();
  OCMExpect([observer_ chooseFileControllerDestroyed:controller])
      .andDo(^(NSInvocation* invocation) {
        controller->RemoveObserver(bridge_.get());
      });

  controller_.reset();

  EXPECT_OCMOCK_VERIFY(observer_);
}
