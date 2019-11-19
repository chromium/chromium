// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"

#import <Foundation/Foundation.h>

#include <memory>

#include "base/bind.h"
#include "components/search_engines/template_url_service.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state_manager.h"
#include "ios/chrome/browser/main/test_browser.h"
#include "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#include "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/authentication_service_fake.h"
#include "ios/chrome/browser/sync/sync_setup_service.h"
#include "ios/chrome/browser/sync/sync_setup_service_factory.h"
#include "ios/chrome/test/ios_chrome_scoped_testing_chrome_browser_state_manager.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

NSString* const kSpdyProxyEnabled = @"SpdyProxyEnabled";

using testing::ReturnRef;

class SettingsNavigationControllerTest : public PlatformTest {
 protected:
  SettingsNavigationControllerTest()
      : scoped_browser_state_manager_(
            std::make_unique<TestChromeBrowserStateManager>(base::FilePath())) {
    TestChromeBrowserState::Builder test_cbs_builder;
    test_cbs_builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        base::BindRepeating(
            &AuthenticationServiceFake::CreateAuthenticationService));
    test_cbs_builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());
    chrome_browser_state_ = test_cbs_builder.Build();
    WebStateList* web_state_list = nullptr;
    browser_ = std::make_unique<TestBrowser>(chrome_browser_state_.get(),
                                             web_state_list);

    mockDelegate_ = [OCMockObject
        niceMockForProtocol:@protocol(SettingsNavigationControllerDelegate)];

    TemplateURLService* template_url_service =
        ios::TemplateURLServiceFactory::GetForBrowserState(
            chrome_browser_state_.get());
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
  IOSChromeScopedTestingChromeBrowserStateManager scoped_browser_state_manager_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  std::unique_ptr<Browser> browser_;
  id mockDelegate_;
  NSString* initialValueForSpdyProxyEnabled_;
};

// When navigation stack has more than one view controller,
// -popViewControllerAnimated: successfully removes the top view controller.
TEST_F(SettingsNavigationControllerTest, PopController) {
  @autoreleasepool {
    SettingsNavigationController* settingsController =
        [SettingsNavigationController
            mainSettingsControllerForBrowser:browser_.get()
                                    delegate:nil];
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
}

// When the navigation stack has only one view controller,
// -popViewControllerAnimated: returns false.
TEST_F(SettingsNavigationControllerTest, DontPopRootController) {
  @autoreleasepool {
    SettingsNavigationController* settingsController =
        [SettingsNavigationController
            mainSettingsControllerForBrowser:browser_.get()
                                    delegate:nil];
    EXPECT_EQ(1U, [[settingsController viewControllers] count]);

    EXPECT_FALSE([settingsController popViewControllerAnimated:NO]);
    [settingsController cleanUpSettings];
  }
}

// When the settings navigation stack has more than one view controller, calling
// -popViewControllerOrCloseSettingsAnimated: pops the top view controller to
// reveal the view controller underneath.
TEST_F(SettingsNavigationControllerTest,
       PopWhenNavigationStackSizeIsGreaterThanOne) {
  @autoreleasepool {
    SettingsNavigationController* settingsController =
        [SettingsNavigationController
            mainSettingsControllerForBrowser:browser_.get()
                                    delegate:mockDelegate_];
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
}

// When the settings navigation stack only has one view controller, calling
// -popViewControllerOrCloseSettingsAnimated: calls -closeSettings on the
// delegate.
TEST_F(SettingsNavigationControllerTest,
       CloseSettingsWhenNavigationStackSizeIsOne) {
  @autoreleasepool {
    SettingsNavigationController* settingsController =
        [SettingsNavigationController
            mainSettingsControllerForBrowser:browser_.get()
                                    delegate:mockDelegate_];
    EXPECT_EQ(1U, [[settingsController viewControllers] count]);
    [[mockDelegate_ expect] closeSettings];
    [settingsController popViewControllerOrCloseSettingsAnimated:NO];
    EXPECT_OCMOCK_VERIFY(mockDelegate_);
    [settingsController cleanUpSettings];
  }
}

}  // namespace
