// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/cells/shortcuts_mediator.h"

#import "base/time/default_clock.h"
#import "components/feature_engagement/test/mock_tracker.h"
#import "components/reading_list/core/fake_reading_list_model_storage.h"
#import "components/reading_list/core/reading_list_model_impl.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_actions_delegate.h"
#import "ios/chrome/browser/reading_list/model/reading_list_model_factory.h"
#import "ios/chrome/browser/reading_list/model/reading_list_test_utils.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/whats_new_commands.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_action_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_shortcut_tile_view.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

@protocol
    ShortcutsMediatorDispatcher <BrowserCoordinatorCommands, WhatsNewCommands>
@end

// Testing Suite for ShortcutsMediator
class ShortcutsMediatorTest : public PlatformTest {
 public:
  ShortcutsMediatorTest() {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetFactoryWithDelegate(
            std::make_unique<FakeAuthenticationServiceDelegate>()));
    profile_ = std::move(builder).Build();

    auto storage = std::make_unique<FakeReadingListModelStorage>();
    base::WeakPtr<FakeReadingListModelStorage> storage_ptr =
        storage->AsWeakPtr();
    reading_list_model_ = std::make_unique<ReadingListModelImpl>(
        std::move(storage), syncer::StorageType::kUnspecified,
        syncer::WipeModelUponSyncDisabledBehavior::kNever,
        base::DefaultClock::GetInstance());
    storage_ptr->TriggerLoadCompletion();

    dispatcher_ = OCMProtocolMock(@protocol(ShortcutsMediatorDispatcher));
    mock_delegate_ = OCMProtocolMock(@protocol(ShortcutsMediatorDelegate));
    mock_ntp_actions_delegate_ =
        OCMProtocolMock(@protocol(NewTabPageActionsDelegate));

    AuthenticationService* authentication_service =
        AuthenticationServiceFactory::GetForProfile(profile_.get());
    mediator_ = [[ShortcutsMediator alloc]
        initWithReadingListModel:reading_list_model_.get()
        featureEngagementTracker:&tracker_
                     authService:authentication_service];

    mediator_.contentSuggestionsMetricsRecorder = metrics_recorder_;
    mediator_.dispatcher = dispatcher_;
    mediator_.delegate = mock_delegate_;
    mediator_.NTPActionsDelegate = mock_ntp_actions_delegate_;
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  ShortcutsMediator* mediator_;
  ContentSuggestionsMetricsRecorder* metrics_recorder_;
  feature_engagement::test::MockTracker tracker_;
  std::unique_ptr<ReadingListModel> reading_list_model_;
  id dispatcher_;
  id mock_delegate_;
  id mock_ntp_actions_delegate_;
};

// Tests that the command is sent to the dispatcher when opening the Reading
// List.
TEST_F(ShortcutsMediatorTest, TestOpenReadingList) {
  OCMExpect([dispatcher_ showReadingList]);
  OCMExpect([mock_ntp_actions_delegate_ shortcutTileOpened]);
  OCMExpect([mock_delegate_
      logMagicStackEngagementForType:ContentSuggestionsModuleType::kShortcuts]);

  // Action.
  ContentSuggestionsMostVisitedActionItem* readingList =
      [[ContentSuggestionsMostVisitedActionItem alloc]
          initWithCollectionShortcutType:NTPCollectionShortcutTypeReadingList];
  ContentSuggestionsShortcutTileView* shortcutView =
      [[ContentSuggestionsShortcutTileView alloc]
          initWithConfiguration:readingList];
  UIGestureRecognizer* recognizer = [[UIGestureRecognizer alloc] init];
  [shortcutView addGestureRecognizer:recognizer];
  [mediator_ shortcutsTapped:recognizer];

  // Test.
  EXPECT_OCMOCK_VERIFY(dispatcher_);
  EXPECT_OCMOCK_VERIFY(mock_ntp_actions_delegate_);
  EXPECT_OCMOCK_VERIFY(mock_delegate_);
}

// Tests that the command is sent to the dispatcher when opening the What's new.
TEST_F(ShortcutsMediatorTest, TestOpenWhatsNew) {
  OCMExpect([dispatcher_ showWhatsNew]);
  OCMExpect([mock_ntp_actions_delegate_ shortcutTileOpened]);
  OCMExpect([mock_delegate_
      logMagicStackEngagementForType:ContentSuggestionsModuleType::kShortcuts]);

  // Action.
  ContentSuggestionsMostVisitedActionItem* whatsNew =
      [[ContentSuggestionsMostVisitedActionItem alloc]
          initWithCollectionShortcutType:NTPCollectionShortcutTypeWhatsNew];
  ContentSuggestionsShortcutTileView* shortcutView =
      [[ContentSuggestionsShortcutTileView alloc]
          initWithConfiguration:whatsNew];
  UIGestureRecognizer* recognizer = [[UIGestureRecognizer alloc] init];
  [shortcutView addGestureRecognizer:recognizer];
  [mediator_ shortcutsTapped:recognizer];
  // Test.
  EXPECT_OCMOCK_VERIFY(dispatcher_);
  EXPECT_OCMOCK_VERIFY(mock_ntp_actions_delegate_);
  EXPECT_OCMOCK_VERIFY(mock_delegate_);
}
