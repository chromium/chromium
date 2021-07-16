// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/menu/browser_action_factory.h"

#import "base/test/metrics/histogram_tester.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/sessions/test_session_service.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/menu/menu_action_type.h"
#import "ios/chrome/browser/ui/menu/menu_histograms.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "ui/base/test/ios/ui_image_test_utils.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
MenuScenario kTestMenuScenario = MenuScenario::kHistoryEntry;
}  // namespace

// Test fixture for the BrowserActionFactory.
class BrowserActionFactoryTest : public PlatformTest {
 protected:
  BrowserActionFactoryTest()
      : test_title_(@"SomeTitle"),
        test_browser_(std::make_unique<TestBrowser>()) {}

  void SetUp() override {
    mock_application_commands_handler_ =
        OCMStrictProtocolMock(@protocol(ApplicationCommands));
    [test_browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_application_commands_handler_
                     forProtocol:@protocol(ApplicationCommands)];

    mock_application_settings_commands_handler_ =
        OCMStrictProtocolMock(@protocol(ApplicationSettingsCommands));
    [test_browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_application_settings_commands_handler_
                     forProtocol:@protocol(ApplicationSettingsCommands)];
  }

  // Creates a blue square.
  UIImage* CreateMockImage() {
    return ui::test::uiimage_utils::UIImageWithSizeAndSolidColor(
        CGSizeMake(10, 10), [UIColor blueColor]);
  }

  base::test::TaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
  NSString* test_title_;
  std::unique_ptr<TestBrowser> test_browser_;
  id mock_application_commands_handler_;
  id mock_application_settings_commands_handler_;
};

// Tests that the Open in New Tab actions have the right titles and images.
TEST_F(BrowserActionFactoryTest, OpenInNewTabAction_URL) {
  if (@available(iOS 13.0, *)) {
    GURL testURL = GURL("https://example.com");

    // Using an action factory with a browser should return expected action.
    BrowserActionFactory* factory =
        [[BrowserActionFactory alloc] initWithBrowser:test_browser_.get()
                                             scenario:kTestMenuScenario];

    UIImage* expectedImage = [UIImage imageNamed:@"open_in_new_tab"];
    NSString* expectedTitle =
        l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWTAB);

    UIAction* actionWithURL = [factory actionToOpenInNewTabWithURL:testURL
                                                        completion:nil];
    EXPECT_TRUE([expectedTitle isEqualToString:actionWithURL.title]);
    EXPECT_EQ(expectedImage, actionWithURL.image);

    UIAction* actionWithBlock = [factory actionToOpenInNewTabWithBlock:nil];
    EXPECT_TRUE([expectedTitle isEqualToString:actionWithBlock.title]);
    EXPECT_EQ(expectedImage, actionWithBlock.image);
  }
}

// Tests that the Open in New Incognito Tab actions have the right titles
// and images.
TEST_F(BrowserActionFactoryTest, OpenInNewIncognitoTabAction_URL) {
  if (@available(iOS 13.0, *)) {
    GURL testURL = GURL("https://example.com");

    // Using an action factory with a browser should return expected action.
    BrowserActionFactory* factory =
        [[BrowserActionFactory alloc] initWithBrowser:test_browser_.get()
                                             scenario:kTestMenuScenario];

    UIImage* expectedImage = [UIImage imageNamed:@"open_in_incognito"];
    NSString* expectedTitle =
        l10n_util::GetNSString(IDS_IOS_OPEN_IN_INCOGNITO_ACTION_TITLE);

    UIAction* actionWithURL =
        [factory actionToOpenInNewIncognitoTabWithURL:testURL completion:nil];
    EXPECT_TRUE([expectedTitle isEqualToString:actionWithURL.title]);
    EXPECT_EQ(expectedImage, actionWithURL.image);

    UIAction* actionWithBlock =
        [factory actionToOpenInNewIncognitoTabWithBlock:nil];
    EXPECT_TRUE([expectedTitle isEqualToString:actionWithBlock.title]);
    EXPECT_EQ(expectedImage, actionWithBlock.image);
  }
}

// Tests that the Open in New Window action has the right title and image.
TEST_F(BrowserActionFactoryTest, OpenInNewWindowAction) {
  if (@available(iOS 13.0, *)) {
    GURL testURL = GURL("https://example.com");

    // Using an action factory with a browser should return expected action.
    BrowserActionFactory* factory =
        [[BrowserActionFactory alloc] initWithBrowser:test_browser_.get()
                                             scenario:kTestMenuScenario];

    UIImage* expectedImage = [UIImage imageNamed:@"open_new_window"];
    NSString* expectedTitle =
        l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_OPENINNEWWINDOW);

    UIAction* action =
        [factory actionToOpenInNewWindowWithURL:testURL
                                 activityOrigin:WindowActivityToolsOrigin];

    EXPECT_TRUE([expectedTitle isEqualToString:action.title]);
    EXPECT_EQ(expectedImage, action.image);
  }
}

// Tests that the open image action has the right title and image.
TEST_F(BrowserActionFactoryTest, OpenImageAction) {
  if (@available(iOS 13.0, *)) {
    BrowserActionFactory* factory =
        [[BrowserActionFactory alloc] initWithBrowser:test_browser_.get()
                                             scenario:kTestMenuScenario];

    GURL testURL = GURL("https://example.com/logo.png");

    UIImage* expectedImage = [UIImage imageNamed:@"open"];
    NSString* expectedTitle =
        l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_OPENIMAGE);

    UIAction* action = [factory actionOpenImageWithURL:testURL
                                            completion:^{
                                            }];

    EXPECT_TRUE([expectedTitle isEqualToString:action.title]);
    EXPECT_EQ(expectedImage, action.image);
  }
}

// Tests that the open image in new tab action has the right title and image.
TEST_F(BrowserActionFactoryTest, OpenImageInNewTabAction) {
  if (@available(iOS 13.0, *)) {
    BrowserActionFactory* factory =
        [[BrowserActionFactory alloc] initWithBrowser:test_browser_.get()
                                             scenario:kTestMenuScenario];

    GURL testURL = GURL("https://example.com/logo.png");
    UrlLoadParams testParams = UrlLoadParams::InNewTab(testURL);

    UIImage* expectedImage = [UIImage imageNamed:@"open_image_in_new_tab"];
    NSString* expectedTitle =
        l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_OPENIMAGENEWTAB);

    UIAction* action =
        [factory actionOpenImageInNewTabWithUrlLoadParams:testParams
                                               completion:^{
                                               }];

    EXPECT_TRUE([expectedTitle isEqualToString:action.title]);
    EXPECT_EQ(expectedImage, action.image);
  }
}
