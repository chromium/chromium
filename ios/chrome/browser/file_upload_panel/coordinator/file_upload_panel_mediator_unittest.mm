// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/file_upload_panel/coordinator/file_upload_panel_mediator.h"

#import "base/test/task_environment.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/file_upload_panel_commands.h"
#import "ios/chrome/browser/web/model/choose_file/choose_file_controller.h"
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

// Test suite for FileUploadPanelMediator.
class FileUploadPanelMediatorTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();
    web_state_ = std::make_unique<web::FakeWebState>();
    ChooseFileEvent event =
        ChooseFileEvent::Builder().SetWebState(web_state_.get()).Build();
    controller_ = std::make_unique<FakeChooseFileController>(event);
    handler_ = OCMProtocolMock(@protocol(FileUploadPanelCommands));
    if (@available(iOS 18.4, *)) {
      mediator_ = [[FileUploadPanelMediator alloc]
          initWithChooseFileController:controller_.get()];
      mediator_.fileUploadPanelHandler = handler_;
    }
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<web::FakeWebState> web_state_;
  std::unique_ptr<FakeChooseFileController> controller_;
  id<FileUploadPanelCommands> handler_;
  API_AVAILABLE(ios(18.4)) FileUploadPanelMediator* mediator_;
};

// Tests that destroying the controller calls the handler to hide the panel.
TEST_F(FileUploadPanelMediatorTest, ControllerDestroyed) {
  if (@available(iOS 18.4, *)) {
    OCMExpect([handler_ hideFileUploadPanel]);
    controller_.reset();
    EXPECT_OCMOCK_VERIFY(handler_);
  }
}

// Tests that disconnecting the mediator submits the selection if it has not
// been submitted yet.
TEST_F(FileUploadPanelMediatorTest, DisconnectSubmitsSelection) {
  if (@available(iOS 18.4, *)) {
    EXPECT_FALSE(controller_->HasSubmittedSelection());
    [mediator_ disconnect];
    EXPECT_TRUE(controller_->HasSubmittedSelection());
  }
}
