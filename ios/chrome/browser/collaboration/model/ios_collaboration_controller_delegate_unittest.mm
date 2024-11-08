// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/collaboration/model/ios_collaboration_controller_delegate.h"

#import "base/check.h"
#import "base/test/mock_callback.h"
#import "ios/chrome/browser/collaboration/model/ios_collaboration_flow_configuration.h"
#import "ios/chrome/browser/share_kit/model/test_share_kit_service.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/web_state_list_builder_from_description.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/test/fakes/fake_ui_view_controller.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

namespace collaboration {

// Test fixture for the iOS collaboration controller delegate.
class IOSCollaborationControllerDelegateTest : public PlatformTest {
 protected:
  IOSCollaborationControllerDelegateTest() {
    // Create a tabGroup from the `WebStateListBuilderFromDescription`.
    EXPECT_TRUE(builder_.BuildWebStateListFromDescription("| a b [ 0 c ] d e"));
    tab_group_ = builder_.GetTabGroupForIdentifier('0');

    // Init the delegate parameters.
    command_dispatcher_ = [[CommandDispatcher alloc] init];
    id mock_application_commands_handler_ =
        OCMStrictProtocolMock(@protocol(ApplicationCommands));
    [command_dispatcher_
        startDispatchingToTarget:mock_application_commands_handler_
                     forProtocol:@protocol(ApplicationCommands)];
    share_kit_service_ = std::make_unique<TestShareKitService>();
    base_view_controller_ = [[FakeUIViewController alloc] init];
  }

  // Init the delegate for a share flow.
  void InitShareFlowDelegate() {
    delegate_ = std::make_unique<IOSCollaborationControllerDelegate>(
        std::make_unique<CollaborationFlowConfigurationShare>(
            share_kit_service_.get(), command_dispatcher_,
            tab_group_->GetWeakPtr(), base_view_controller_));
  }

  std::unique_ptr<IOSCollaborationControllerDelegate> delegate_;
  FakeWebStateListDelegate web_state_list_delegate_;
  WebStateList web_state_list_{&web_state_list_delegate_};
  WebStateListBuilderFromDescription builder_{&web_state_list_};
  CommandDispatcher* command_dispatcher_;
  UIViewController* base_view_controller_;
  raw_ptr<const TabGroup> tab_group_;
  std::unique_ptr<ShareKitService> share_kit_service_;
};

// Tests `ShowShareDialog` with a valid tabGroup.
TEST_F(IOSCollaborationControllerDelegateTest, ShowShareDialogValid) {
  InitShareFlowDelegate();
  base::MockCallback<CollaborationControllerDelegate::ResultCallback>
      completion_callback;
  EXPECT_CALL(completion_callback, Run(true));
  delegate_->ShowShareDialog(completion_callback.Get());
  EXPECT_TRUE(base_view_controller_.presentedViewController);
}

// Tests `ShowShareDialog` with an invalid tabGroup.
TEST_F(IOSCollaborationControllerDelegateTest, ShowShareDialogInValid) {
  InitShareFlowDelegate();

  // Delete the tabGroup.
  web_state_list_.DeleteGroup(tab_group_);

  base::MockCallback<CollaborationControllerDelegate::ResultCallback>
      completion_callback;
  EXPECT_CALL(completion_callback, Run(false));
  delegate_->ShowShareDialog(completion_callback.Get());
  EXPECT_FALSE(base_view_controller_.presentedViewController);
}

}  // namespace collaboration
