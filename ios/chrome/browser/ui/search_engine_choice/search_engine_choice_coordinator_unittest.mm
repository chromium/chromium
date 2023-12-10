// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_coordinator.h"

#import <UIKit/UIKit.h>
#import <memory>

#import "base/apple/foundation_util.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "components/search_engines/search_engine_choice_utils.h"
#import "components/search_engines/search_engines_pref_names.h"
#import "components/signin/public/base/signin_switches.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/favicon/favicon_service_factory.h"
#import "ios/chrome/browser/favicon/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/favicon/ios_chrome_large_icon_service_factory.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/promos_manager/mock_promos_manager.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/browser_view/browser_coordinator.h"
#import "ios/chrome/browser/ui/first_run/first_run_screen_delegate.h"
#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_table/search_engine_choice_table_view_controller.h"
#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_view_controller.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util_mac.h"

// Empty implementation of the FirstRunScreenDelegate used to check that the
// screen does get dismissed.
@interface FirstRunScreenTestDelegate : NSObject <FirstRunScreenDelegate>

@property(nonatomic, readonly) BOOL wasDismissed;

@end

@implementation FirstRunScreenTestDelegate

@synthesize wasDismissed = _wasDismissed;

- (void)screenWillFinishPresenting {
  _wasDismissed = YES;
}

- (void)skipAllScreens {
}

@end

// Empty implementation of the SearchEngineChoiceCoordinatorDelegate used to
// check that the screen does get dismissed.
@interface SearchEngineChoiceCoordinatorTestDelegate
    : NSObject <SearchEngineChoiceCoordinatorDelegate>

@property(nonatomic, readonly) BOOL wasDismissed;

@end

@implementation SearchEngineChoiceCoordinatorTestDelegate

@synthesize wasDismissed = _wasDismissed;

- (void)choiceScreenWillBeDismissed:
    (SearchEngineChoiceCoordinator*)coordinator {
  _wasDismissed = YES;
}

@end

class SearchEngineChoiceCoordinatorTest : public PlatformTest {
 protected:
  SearchEngineChoiceCoordinatorTest() {
    feature_list_.InitWithFeatures(
        {switches::kSearchEngineChoiceFre, switches::kSearchEngineChoice}, {});
    TestChromeBrowserState::Builder test_cbs_builder;
    test_cbs_builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());
    test_cbs_builder.AddTestingFactory(
        ios::FaviconServiceFactory::GetInstance(),
        ios::FaviconServiceFactory::GetDefaultFactory());
    test_cbs_builder.AddTestingFactory(
        IOSChromeLargeIconServiceFactory::GetInstance(),
        IOSChromeLargeIconServiceFactory::GetDefaultFactory());
    test_cbs_builder.AddTestingFactory(
        IOSChromeFaviconLoaderFactory::GetInstance(),
        IOSChromeFaviconLoaderFactory::GetDefaultFactory());
    test_cbs_builder.AddTestingFactory(
        ios::HistoryServiceFactory::GetInstance(),
        ios::HistoryServiceFactory::GetDefaultFactory());
    browser_state_ = test_cbs_builder.Build();
    browser_ = std::make_unique<TestBrowser>(browser_state_.get());
  }

  ~SearchEngineChoiceCoordinatorTest() override {
    [coordinator_ stop];
    coordinator_ = nil;
    feature_list_.Reset();
  }

  void CreateAndStartCoordinator() {
    base_view_controller_ = [[UIViewController alloc] init];
    [scoped_key_window_.Get() setRootViewController:base_view_controller_];
    coordinator_delegate_ =
        [[SearchEngineChoiceCoordinatorTestDelegate alloc] init];
    coordinator_ = [[SearchEngineChoiceCoordinator alloc]
        initWithBaseViewController:base_view_controller_
                           browser:browser_.get()];
    coordinator_.delegate = coordinator_delegate_;
    [coordinator_ start];

    search_engine_choice_view_controller_ =
        base::apple::ObjCCastStrict<SearchEngineChoiceViewController>(
            base_view_controller_.presentedViewController);
    table_view_controller_ =
        search_engine_choice_view_controller_.childViewControllers[0];
  }

  void CreateAndStartCoordinatorForFre() {
    root_view_controller_ = [[UIViewController alloc] init];
    scoped_key_window_.Get().rootViewController = root_view_controller_;
    base_navigation_controller_ = [[UINavigationController alloc] init];
    first_run_delegate_ = [[FirstRunScreenTestDelegate alloc] init];
    coordinator_ = [[SearchEngineChoiceCoordinator alloc]
        initForFirstRunWithBaseNavigationController:base_navigation_controller_
                                            browser:browser_.get()
                                   firstRunDelegate:first_run_delegate_];
    [coordinator_ start];

    // `base_navigation_controller_` needs to be presented to make sure the view
    // is loaded.
    __block bool presentation_finished = NO;
    [root_view_controller_ presentViewController:base_navigation_controller_
                                        animated:NO
                                      completion:^{
                                        presentation_finished = YES;
                                      }];
    EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
        base::test::ios::kWaitForUIElementTimeout, ^bool {
          return presentation_finished;
        }));

    search_engine_choice_view_controller_ =
        base::apple::ObjCCastStrict<SearchEngineChoiceViewController>(
            base_navigation_controller_.childViewControllers[0]);
    table_view_controller_ =
        search_engine_choice_view_controller_.childViewControllers[0];
  }

  // Select the first visible search engine and set it as the default.
  void SelectAndSetAsDefault() {
    [table_view_controller_.delegate selectSearchEngineAtRow:0];
    [search_engine_choice_view_controller_.actionDelegate didTapPrimaryButton];
  }

  void ExpectChoiceScreenWasDisplayedRecorded() {
    histogram_tester_.ExpectBucketCount(
        search_engines::kSearchEngineChoiceScreenEventsHistogram,
        search_engines::SearchEngineChoiceScreenEvents::
            kChoiceScreenWasDisplayed,
        1);
  }

  void ExpectFreChoiceScreenWasDisplayedRecorded() {
    histogram_tester_.ExpectBucketCount(
        search_engines::kSearchEngineChoiceScreenEventsHistogram,
        search_engines::SearchEngineChoiceScreenEvents::
            kFreChoiceScreenWasDisplayed,
        1);
  }

  void ExpectDefaultWasSetRecorded() {
    histogram_tester_.ExpectBucketCount(
        search_engines::kSearchEngineChoiceScreenEventsHistogram,
        search_engines::SearchEngineChoiceScreenEvents::kDefaultWasSet, 1);
  }

  void ExpectFreDefaultWasSetRecorded() {
    histogram_tester_.ExpectBucketCount(
        search_engines::kSearchEngineChoiceScreenEventsHistogram,
        search_engines::SearchEngineChoiceScreenEvents::kFreDefaultWasSet, 1);
  }

  void ExpectLearnMoreWasDisplayedRecorded() {
    histogram_tester_.ExpectBucketCount(
        search_engines::kSearchEngineChoiceScreenEventsHistogram,
        search_engines::SearchEngineChoiceScreenEvents::kLearnMoreWasDisplayed,
        1);
  }

  void ExpectFreLearnMoreWasDisplayedRecorded() {
    histogram_tester_.ExpectBucketCount(
        search_engines::kSearchEngineChoiceScreenEventsHistogram,
        search_engines::SearchEngineChoiceScreenEvents::
            kFreLearnMoreWasDisplayed,
        1);
  }

  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<Browser> browser_;
  UIViewController* root_view_controller_ = nil;
  UIViewController* base_view_controller_ = nil;
  UINavigationController* base_navigation_controller_ = nil;
  SearchEngineChoiceViewController* search_engine_choice_view_controller_ = nil;
  SearchEngineChoiceTableViewController* table_view_controller_ = nil;
  ScopedKeyWindow scoped_key_window_;
  SearchEngineChoiceCoordinator* coordinator_;
  base::HistogramTester histogram_tester_;
  FirstRunScreenTestDelegate* first_run_delegate_ = nil;
  SearchEngineChoiceCoordinatorTestDelegate* coordinator_delegate_ = nil;
};

// Checks that the correct metrics are recorded and that the screen gets
// dismissed.
TEST_F(SearchEngineChoiceCoordinatorTest, TestHistograms) {
  CreateAndStartCoordinator();
  ExpectChoiceScreenWasDisplayedRecorded();

  SelectAndSetAsDefault();
  ASSERT_TRUE(coordinator_delegate_.wasDismissed);
  ExpectDefaultWasSetRecorded();
}

// Checks that the correct metrics are recorded when the Learn More screen is
// displayed.
TEST_F(SearchEngineChoiceCoordinatorTest, TestLearnMoreRecorded) {
  CreateAndStartCoordinator();

  [search_engine_choice_view_controller_.actionDelegate showLearnMore];
  ExpectLearnMoreWasDisplayedRecorded();
}

// Checks that the correct metrics are recorded for the FRE and that the screen
// is dismissed.
TEST_F(SearchEngineChoiceCoordinatorTest, TestFREHistograms) {
  CreateAndStartCoordinatorForFre();
  ExpectFreChoiceScreenWasDisplayedRecorded();

  SelectAndSetAsDefault();
  ASSERT_TRUE(first_run_delegate_.wasDismissed);
  ExpectFreDefaultWasSetRecorded();
}

// Checks that the correct metrics are recorded in the FRE when the Learn More
// screen is displayed.
TEST_F(SearchEngineChoiceCoordinatorTest, TestFRELearnMoreRecorded) {
  CreateAndStartCoordinatorForFre();

  [search_engine_choice_view_controller_.actionDelegate showLearnMore];
  ExpectFreLearnMoreWasDisplayedRecorded();
}
