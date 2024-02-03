// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/cells/shortcuts_mediator.h"

#import "components/feature_engagement/test/mock_tracker.h"
#import "components/reading_list/core/reading_list_model_impl.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/reading_list/model/reading_list_model_factory.h"
#import "ios/chrome/browser/reading_list/model/reading_list_test_utils.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_action_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_shortcut_tile_view.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_mediator_util.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_metrics_delegate.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {
std::unique_ptr<KeyedService> BuildFeatureEngagementMockTracker(
    web::BrowserState* browser_state) {
  return std::make_unique<feature_engagement::test::MockTracker>();
}
}  // namespace

@protocol ShortcutsMediatorDispatcher <BrowserCoordinatorCommands>
@end

// Testing Suite for ShortcutsMediator
class ShortcutsMediatorTest : public PlatformTest {
 public:
  ShortcutsMediatorTest() {
    TestChromeBrowserState::Builder test_cbs_builder;
    test_cbs_builder.AddTestingFactory(
        ReadingListModelFactory::GetInstance(),
        base::BindRepeating(&BuildReadingListModelWithFakeStorage,
                            std::vector<scoped_refptr<ReadingListEntry>>()));
    test_cbs_builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        base::BindRepeating(AuthenticationServiceFactory::GetDefaultFactory()));
    test_cbs_builder.AddTestingFactory(
        feature_engagement::TrackerFactory::GetInstance(),
        base::BindRepeating(&BuildFeatureEngagementMockTracker));
    chrome_browser_state_ = test_cbs_builder.Build();
    AuthenticationServiceFactory::CreateAndInitializeForBrowserState(
        chrome_browser_state_.get(),
        std::make_unique<FakeAuthenticationServiceDelegate>());

    dispatcher_ = OCMProtocolMock(@protocol(ShortcutsMediatorDispatcher));

    ReadingListModel* readingListModel =
        ReadingListModelFactory::GetForBrowserState(
            chrome_browser_state_.get());
    feature_engagement::Tracker* tracker =
        feature_engagement::TrackerFactory::GetForBrowserState(
            chrome_browser_state_.get());
    AuthenticationService* authentication_service =
        AuthenticationServiceFactory::GetForBrowserState(
            chrome_browser_state_.get());
    mediator_ = [[ShortcutsMediator alloc]
        initWithReadingListModel:readingListModel
        featureEngagementTracker:(feature_engagement::Tracker*)tracker
                     authService:authentication_service];

    mediator_.contentSuggestionsMetricsRecorder = metrics_recorder_;
    mediator_.dispatcher = dispatcher_;
    mediator_.delegate = OCMProtocolMock(@protocol(ShortcutsMediatorDelegate));
    mediator_.NTPMetricsDelegate =
        OCMProtocolMock(@protocol(NewTabPageMetricsDelegate));
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState local_state_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  ShortcutsMediator* mediator_;
  ContentSuggestionsMetricsRecorder* metrics_recorder_;
  id dispatcher_;
};

// Tests that the command is sent to the dispatcher when opening the Reading
// List.
TEST_F(ShortcutsMediatorTest, TestOpenReadingList) {
  OCMExpect([dispatcher_ showReadingList]);

  OCMExpect([mediator_.NTPMetricsDelegate shortcutTileOpened]);
  OCMExpect([mediator_.delegate
      logMagicStackEngagementForType:ContentSuggestionsModuleType::kShortcuts]);

  // Action.
  ContentSuggestionsMostVisitedActionItem* readingList =
      ReadingListActionItem();
  ContentSuggestionsShortcutTileView* shortcutView =
      [[ContentSuggestionsShortcutTileView alloc]
          initWithConfiguration:readingList];
  UIGestureRecognizer* recognizer = [[UIGestureRecognizer alloc] init];
  [shortcutView addGestureRecognizer:recognizer];
  [mediator_ shortcutsTapped:recognizer];

  // Test.
  EXPECT_OCMOCK_VERIFY(dispatcher_);
}

// Tests that the command is sent to the dispatcher when opening the What's new.
TEST_F(ShortcutsMediatorTest, TestOpenWhatsNew) {
  OCMExpect([dispatcher_ showWhatsNew]);

  OCMExpect([mediator_.NTPMetricsDelegate shortcutTileOpened]);
  OCMExpect([mediator_.delegate
      logMagicStackEngagementForType:ContentSuggestionsModuleType::kShortcuts]);

  // Action.
  ContentSuggestionsMostVisitedActionItem* whatsNew = WhatsNewActionItem();
  ContentSuggestionsShortcutTileView* shortcutView =
      [[ContentSuggestionsShortcutTileView alloc]
          initWithConfiguration:whatsNew];
  UIGestureRecognizer* recognizer = [[UIGestureRecognizer alloc] init];
  [shortcutView addGestureRecognizer:recognizer];
  [mediator_ shortcutsTapped:recognizer];
  // Test.
  EXPECT_OCMOCK_VERIFY(dispatcher_);
}
