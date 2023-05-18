// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_details/password_details_coordinator.h"

#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "components/password_manager/core/browser/ui/affiliated_group.h"
#import "ios/chrome/browser/credential_provider_promo/features.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/credential_provider_promo_commands.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_handler.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for testing the PasswordDetailsCoordinatorTest class.
class PasswordDetailsCoordinatorTest : public PlatformTest {
 protected:
  PasswordDetailsCoordinatorTest()
      : browser_state_(TestChromeBrowserState::Builder().Build()),
        browser_(std::make_unique<TestBrowser>(browser_state_.get())) {
    UINavigationController* navigation_controller =
        [[UINavigationController alloc] init];
    const password_manager::AffiliatedGroup& affiliateGroup =
        password_manager::AffiliatedGroup();
    coordinator_ = [[PasswordDetailsCoordinator alloc]
        initWithBaseNavigationController:navigation_controller
                                 browser:browser_.get()
                         affiliatedGroup:affiliateGroup
                            reauthModule:nil
                                 context:DetailsContext::kPasswordSettings];
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<Browser> browser_;

  PasswordDetailsCoordinator* coordinator_;
};

#pragma mark - Tests

// Tests that OnPasswordCopied will dispatch `CredentialProviderPromoCommands`.
TEST_F(PasswordDetailsCoordinatorTest, OnPasswordCopiedTest) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kCredentialProviderExtensionPromo,
      {{"enable_promo_on_password_copied", "true"}});

  // Register the command handler for `CredentialProviderPromoCommands`
  id credential_provider_promo_commands_handler_mock =
      OCMStrictProtocolMock(@protocol(CredentialProviderPromoCommands));
  [browser_.get()->GetCommandDispatcher()
      startDispatchingToTarget:credential_provider_promo_commands_handler_mock
                   forProtocol:@protocol(CredentialProviderPromoCommands)];

  // Expect the call with correct trigger type.
  [[credential_provider_promo_commands_handler_mock expect]
      showCredentialProviderPromoWithTrigger:CredentialProviderPromoTrigger::
                                                 PasswordCopied];

  // Call the tested function.
  ASSERT_TRUE(
      [coordinator_ conformsToProtocol:@protocol(PasswordDetailsHandler)]);
  [(id<PasswordDetailsHandler>)coordinator_ onPasswordCopiedByUser];

  // Verify.
  [credential_provider_promo_commands_handler_mock verify];
}

// Tests that OnPasswordCopied will not dispatch
// `CredentialProviderPromoCommands`.
TEST_F(PasswordDetailsCoordinatorTest,
       OnPasswordCopiedTestCredentialProviderPromoDisabled) {
  base::test::ScopedFeatureList feature_list;
  // Enable another arm that will not lead to dispatching the
  // CredentialProviderPromoCommands.
  feature_list.InitAndEnableFeatureWithParameters(
      kCredentialProviderExtensionPromo,
      {{"enable_promo_on_password_saved", "true"}});

  // Register the command handler for `CredentialProviderPromoCommands`
  // Use `OCMStrictProtocolMock` and do not expect any so that an exception will
  // be raised when there is any invocation.
  id credential_provider_promo_commands_handler_mock =
      OCMStrictProtocolMock(@protocol(CredentialProviderPromoCommands));
  [browser_.get()->GetCommandDispatcher()
      startDispatchingToTarget:credential_provider_promo_commands_handler_mock
                   forProtocol:@protocol(CredentialProviderPromoCommands)];

  // Call the tested function.
  ASSERT_TRUE(
      [coordinator_ conformsToProtocol:@protocol(PasswordDetailsHandler)]);
  [(id<PasswordDetailsHandler>)coordinator_ onPasswordCopiedByUser];
}
