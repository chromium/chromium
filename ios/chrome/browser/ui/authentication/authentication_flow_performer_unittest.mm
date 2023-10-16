// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/authentication_flow_performer.h"

#import <objc/runtime.h>

#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_commands.h"
#import "ios/chrome/browser/shared/public/commands/browsing_data_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/signin/fake_system_identity_manager.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/testing/protocol_fake.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/gtest_support.h"
#import "third_party/ocmock/ocmock_extensions.h"

namespace {

class AuthenticationFlowPerformerTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    browser_state_ = builder.Build();
    AuthenticationServiceFactory::CreateAndInitializeForBrowserState(
        browser_state_.get(),
        std::make_unique<FakeAuthenticationServiceDelegate>());
    browser_ = std::make_unique<TestBrowser>(browser_state_.get());
    fake_identity_ = [FakeSystemIdentity fakeIdentity1];

    NSArray<Protocol*>* command_protocols = @[
      @protocol(ApplicationCommands), @protocol(BrowserCommands),
      @protocol(BrowsingDataCommands), @protocol(ApplicationSettingsCommands),
      @protocol(SnackbarCommands)
    ];
    fake_command_endpoint_ =
        [[ProtocolFake alloc] initWithProtocols:command_protocols];
    for (Protocol* protocol in command_protocols) {
      [browser_->GetCommandDispatcher()
          startDispatchingToTarget:fake_command_endpoint_
                       forProtocol:protocol];
    }

    FakeSystemIdentityManager* fake_system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    fake_system_identity_manager->AddIdentity(fake_identity_);

    authentication_flow_performer_delegate_mock_ =
        OCMStrictProtocolMock(@protocol(AuthenticationFlowPerformerDelegate));
    authentication_flow_performer_ = [[AuthenticationFlowPerformer alloc]
        initWithDelegate:authentication_flow_performer_delegate_mock_];
  }

  void TearDown() override {
    PlatformTest::TearDown();
    EXPECT_OCMOCK_VERIFY(authentication_flow_performer_delegate_mock_);
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<Browser> browser_;
  AuthenticationFlowPerformer* authentication_flow_performer_ = nil;
  id<AuthenticationFlowPerformerDelegate>
      authentication_flow_performer_delegate_mock_ = nil;
  FakeSystemIdentity* fake_identity_ = nil;
  ProtocolFake* fake_command_endpoint_ = nil;
};

// Tests interrupt call with `SigninCoordinatorInterrupt::UIShutdownNoDismiss`.
// The interrupt completion block is called synchronously by the interrupt
// method.
TEST_F(AuthenticationFlowPerformerTest, TestSimpleInterruptUIShutdown) {
  // Prompt for the merge case dialog.
  UIViewController* view_controller = [[UIViewController alloc] init];
  OCMExpect([authentication_flow_performer_delegate_mock_
      presentViewController:[OCMArg any]
                   animated:YES
                 completion:[OCMArg any]]);
  [authentication_flow_performer_ promptMergeCaseForIdentity:fake_identity_
                                                     browser:browser_.get()
                                              viewController:view_controller];
  // Interrupt flow performer with `UIShutdownNoDismiss`.
  __block BOOL completion_called = NO;
  [authentication_flow_performer_
      interruptWithAction:SigninCoordinatorInterrupt::UIShutdownNoDismiss
               completion:^() {
                 completion_called = YES;
               }];
  // Expect the interrupt completion to be called synchronously.
  EXPECT_TRUE(completion_called);
}

// Tests interrupt call with `SigninCoordinatorInterrupt::DismissWithAnimation`.
// The interrupt completion block is called when the view controller has been
// dismissed.
TEST_F(AuthenticationFlowPerformerTest,
       TestSimpleInterruptDismissWithAnimation) {
  // Prompt for the merge case dialog.
  UIViewController* view_controller = [[UIViewController alloc] init];
  OCMExpect([authentication_flow_performer_delegate_mock_
      presentViewController:[OCMArg any]
                   animated:YES
                 completion:[OCMArg any]]);
  [authentication_flow_performer_ promptMergeCaseForIdentity:fake_identity_
                                                     browser:browser_.get()
                                              viewController:view_controller];
  // Interrupt with animated dismiss.
  __block ProceduralBlock dismiss_completion_block = nil;
  OCMExpect([authentication_flow_performer_delegate_mock_
      dismissPresentingViewControllerAnimated:YES
                                   completion:[OCMArg checkWithBlock:^BOOL(
                                                          ProceduralBlock
                                                              block) {
                                     EXPECT_EQ(nil, dismiss_completion_block);
                                     dismiss_completion_block = block;
                                     return YES;
                                   }]]);
  __block BOOL completion_called = NO;
  [authentication_flow_performer_
      interruptWithAction:SigninCoordinatorInterrupt::DismissWithAnimation
               completion:^() {
                 completion_called = YES;
               }];
  EXPECT_FALSE(completion_called);
  // Expect the interrupt completion block to be called once the dialog has been
  // dismissed.
  dismiss_completion_block();
  EXPECT_TRUE(completion_called);
}

// Tests the AuthenticationFlowPerformer is interrupted and the interrupt
// completion is called.
TEST_F(AuthenticationFlowPerformerTest,
       TestSimpleInterruptWithoutDialogDisplayed) {
  __block BOOL completion_called = NO;
  [authentication_flow_performer_
      interruptWithAction:SigninCoordinatorInterrupt::DismissWithAnimation
               completion:^() {
                 completion_called = YES;
               }];
  EXPECT_TRUE(completion_called);
}

}  // namespace
