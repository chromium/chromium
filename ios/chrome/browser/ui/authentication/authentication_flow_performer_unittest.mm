// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/authentication_flow_performer.h"

#import <objc/runtime.h>

#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
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

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    profile_ = std::move(builder).Build();
    AuthenticationServiceFactory::CreateAndInitializeForProfile(
        profile_.get(), std::make_unique<FakeAuthenticationServiceDelegate>());
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    fake_identity_ = [FakeSystemIdentity fakeIdentity1];

    NSArray<Protocol*>* command_protocols = @[
      @protocol(ApplicationCommands), @protocol(BrowserCommands),
      @protocol(SettingsCommands), @protocol(SnackbarCommands)
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
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<Browser> browser_;
  AuthenticationFlowPerformer* authentication_flow_performer_ = nil;
  id<AuthenticationFlowPerformerDelegate>
      authentication_flow_performer_delegate_mock_ = nil;
  FakeSystemIdentity* fake_identity_ = nil;
  ProtocolFake* fake_command_endpoint_ = nil;
};

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
