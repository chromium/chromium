// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"

#import <Foundation/Foundation.h>
#import <objc/runtime.h>

#import <memory>

#import "base/functional/bind.h"
#import "base/test/metrics/user_action_tester.h"
#import "components/password_manager/core/browser/password_manager_test_utils.h"
#import "components/password_manager/core/browser/password_store/test_password_store.h"
#import "components/search_engines/template_url_service.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/popup_menu_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/testing/protocol_fake.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

NSString* const kSpdyProxyEnabled = @"SpdyProxyEnabled";

using testing::ReturnRef;

class SettingsNavigationControllerTest : public PlatformTest {
 protected:
  SettingsNavigationControllerTest() {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        IOSChromeProfilePasswordStoreFactory::GetInstance(),
        base::BindRepeating(
            &password_manager::BuildPasswordStore<
                web::BrowserState, password_manager::TestPasswordStore>));
    profile_ = profile_manager_.AddProfileWithBuilder(std::move(builder));
    AuthenticationServiceFactory::CreateAndInitializeForProfile(
        profile_.get(), std::make_unique<FakeAuthenticationServiceDelegate>());
    browser_ = std::make_unique<TestBrowser>(profile_.get());

    NSArray<Protocol*>* command_protocols = @[
      @protocol(ApplicationCommands), @protocol(BrowserCommands),
      @protocol(SettingsCommands), @protocol(SnackbarCommands),
      @protocol(PopupMenuCommands)
    ];
    fake_command_endpoint_ =
        [[ProtocolFake alloc] initWithProtocols:command_protocols];
    for (Protocol* protocol in command_protocols) {
      [browser_->GetCommandDispatcher()
          startDispatchingToTarget:fake_command_endpoint_
                       forProtocol:protocol];
    }

    mockDelegate_ = [OCMockObject
        niceMockForProtocol:@protocol(SettingsNavigationControllerDelegate)];

    TemplateURLService* template_url_service =
        ios::TemplateURLServiceFactory::GetForProfile(profile_.get());
    template_url_service->Load();

    NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
    initialValueForSpdyProxyEnabled_ =
        [[defaults stringForKey:kSpdyProxyEnabled] copy];
    [defaults setObject:@"Disabled" forKey:kSpdyProxyEnabled];
  }

  ~SettingsNavigationControllerTest() override {
    if (initialValueForSpdyProxyEnabled_) {
      [[NSUserDefaults standardUserDefaults]
          setObject:initialValueForSpdyProxyEnabled_
             forKey:kSpdyProxyEnabled];
    } else {
      [[NSUserDefaults standardUserDefaults]
          removeObjectForKey:kSpdyProxyEnabled];
    }
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<TestProfileIOS> profile_;
  std::unique_ptr<Browser> browser_;
  id mockDelegate_;
  NSString* initialValueForSpdyProxyEnabled_;
  ProtocolFake* fake_command_endpoint_;
};

// When navigation stack has more than one view controller,
// -popViewControllerAnimated: successfully removes the top view controller.
TEST_F(SettingsNavigationControllerTest, PopController) {
  SettingsNavigationController* settingsController =
      [SettingsNavigationController
          mainSettingsControllerForBrowser:browser_.get()
                                  delegate:nil
                  hasDefaultBrowserBlueDot:NO];
  UIViewController* viewController =
      [[UIViewController alloc] initWithNibName:nil bundle:nil];
  [settingsController pushViewController:viewController animated:NO];
  EXPECT_EQ(2U, [[settingsController viewControllers] count]);

  UIViewController* poppedViewController =
      [settingsController popViewControllerAnimated:NO];
  EXPECT_NSEQ(viewController, poppedViewController);
  EXPECT_EQ(1U, [[settingsController viewControllers] count]);
  [settingsController cleanUpSettings];
}

// When the navigation stack has only one view controller,
// -popViewControllerAnimated: returns false.
TEST_F(SettingsNavigationControllerTest, DontPopRootController) {
  SettingsNavigationController* settingsController =
      [SettingsNavigationController
          mainSettingsControllerForBrowser:browser_.get()
                                  delegate:nil
                  hasDefaultBrowserBlueDot:NO];
  EXPECT_EQ(1U, [[settingsController viewControllers] count]);

  EXPECT_FALSE([settingsController popViewControllerAnimated:NO]);
  [settingsController cleanUpSettings];
}

// When the settings navigation stack has more than one view controller, calling
// -popViewControllerOrCloseSettingsAnimated: pops the top view controller to
// reveal the view controller underneath.
TEST_F(SettingsNavigationControllerTest,
       PopWhenNavigationStackSizeIsGreaterThanOne) {
  SettingsNavigationController* settingsController =
      [SettingsNavigationController
          mainSettingsControllerForBrowser:browser_.get()
                                  delegate:mockDelegate_
                  hasDefaultBrowserBlueDot:NO];
  UIViewController* viewController =
      [[UIViewController alloc] initWithNibName:nil bundle:nil];
  [settingsController pushViewController:viewController animated:NO];
  EXPECT_EQ(2U, [[settingsController viewControllers] count]);
  [[mockDelegate_ reject] closeSettings];
  [settingsController popViewControllerOrCloseSettingsAnimated:NO];
  EXPECT_EQ(1U, [[settingsController viewControllers] count]);
  EXPECT_OCMOCK_VERIFY(mockDelegate_);
  [settingsController cleanUpSettings];
}

// When the settings navigation stack only has one view controller, calling
// -popViewControllerOrCloseSettingsAnimated: calls -closeSettings on the
// delegate.
TEST_F(SettingsNavigationControllerTest,
       CloseSettingsWhenNavigationStackSizeIsOne) {
  base::UserActionTester user_action_tester;
  SettingsNavigationController* settingsController =
      [SettingsNavigationController
          mainSettingsControllerForBrowser:browser_.get()
                                  delegate:mockDelegate_
                  hasDefaultBrowserBlueDot:NO];
  EXPECT_EQ(1U, [[settingsController viewControllers] count]);
  [[mockDelegate_ expect] closeSettings];
  ASSERT_EQ(0, user_action_tester.GetActionCount("MobileSettingsClose"));
  [settingsController popViewControllerOrCloseSettingsAnimated:NO];
  EXPECT_EQ(1, user_action_tester.GetActionCount("MobileSettingsClose"));
  EXPECT_OCMOCK_VERIFY(mockDelegate_);
  [settingsController cleanUpSettings];
}

// Checks that metrics are correctly reported.
TEST_F(SettingsNavigationControllerTest, Metrics) {
  base::UserActionTester user_action_tester;
  SettingsNavigationController* settingsController =
      [SettingsNavigationController
          mainSettingsControllerForBrowser:browser_.get()
                                  delegate:mockDelegate_
                  hasDefaultBrowserBlueDot:NO];
  std::string user_action = "MobileKeyCommandClose";
  ASSERT_EQ(user_action_tester.GetActionCount(user_action), 0);

  [settingsController keyCommand_close];

  EXPECT_EQ(user_action_tester.GetActionCount(user_action), 1);
  [settingsController cleanUpSettings];
}

}  // namespace
