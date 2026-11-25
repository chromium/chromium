// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bookmarks/ui_bundled/home/bookmarks_coordinator.h"

#import <UIKit/UIKit.h>

#import "base/memory/raw_ptr.h"
#import "base/test/scoped_feature_list.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/browser/bookmark_node.h"
#import "components/send_tab_to_self/features.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_ios_unit_test_support.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/home/bookmarks_home_view_controller.h"
#import "ios/chrome/browser/reminder_notifications/coordinator/reminder_notifications_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

// Test fixture for BookmarksCoordinator.
class BookmarksCoordinatorTest : public BookmarkIOSUnitTestSupport {
 protected:
  BookmarksCoordinatorTest() {
    // The "Set a reminder" UI is only available with this feature.
    feature_list_.InitAndEnableFeature(send_tab_to_self::kIOSTabReminders);
  }

  void SetUp() override {
    BookmarkIOSUnitTestSupport::SetUp();

    bookmark_ = AddBookmark(bookmark_model_->mobile_node(), u"foo");

    base_view_controller_ = [[UIViewController alloc] init];
    [scoped_key_window_.Get() setRootViewController:base_view_controller_];

    // Set up SceneCommands mock. Because SceneCommands conforms to
    // SettingsCommands, that needs to be mocked and dispatched as well.
    mock_snackbar_handler_ = OCMProtocolMock(@protocol(SnackbarCommands));
    mock_scene_handler_ = OCMProtocolMock(@protocol(SceneCommands));
    mock_settings_handler_ = OCMProtocolMock(@protocol(SettingsCommands));
    CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
    [dispatcher startDispatchingToTarget:mock_snackbar_handler_
                             forProtocol:@protocol(SnackbarCommands)];
    [dispatcher startDispatchingToTarget:mock_scene_handler_
                             forProtocol:@protocol(SceneCommands)];
    [dispatcher startDispatchingToTarget:mock_settings_handler_
                             forProtocol:@protocol(SettingsCommands)];

    // Make BookmarksCoordinator create `mock_reminder_coordinator_` instead of
    // a real ReminderNotificationsCoordinator, to observe when it is started
    // and stopped.
    mock_reminder_coordinator_class_ =
        OCMClassMock([ReminderNotificationsCoordinator class]);
    mock_reminder_coordinator_ = mock_reminder_coordinator_class_;
    OCMStub([mock_reminder_coordinator_class_ alloc])
        .andReturn(mock_reminder_coordinator_class_);
    OCMStub([mock_reminder_coordinator_
                initWithBaseViewController:[OCMArg any]
                                   browser:browser_.get()])
        .andReturn(mock_reminder_coordinator_class_);

    coordinator_ =
        [[BookmarksCoordinator alloc] initWithBrowser:browser_.get()];
    coordinator_.baseViewController = base_view_controller_;
  }

  void TearDown() override {
    // BookmarksCoordinator must be stopped before `browser_` is destroyed.
    [coordinator_ stop];
    coordinator_ = nil;
    [mock_reminder_coordinator_class_ stopMocking];
    bookmark_ = nullptr;
    BookmarkIOSUnitTestSupport::TearDown();
  }

  // Asks `coordinator_` to show the "Set a reminder" UI for `node`, as the
  // bookmark browser does when the user picks "Set a reminder" in the context
  // menu of a bookmark.
  void ShowSetTabReminderUI(const bookmarks::BookmarkNode* node) {
    [static_cast<id<BookmarksHomeViewControllerDelegate>>(coordinator_)
                bookmarkHomeViewController:nil
        wantsToShowSetTabReminderUIForNode:node];
  }

  // Presents the bookmark browser, asks `coordinator_` to show the "Set a
  // reminder" UI for `bookmark_` and checks that the UI is started.
  void PresentBookmarksAndShowReminderUI() {
    [coordinator_ presentBookmarks];
    OCMExpect([mock_reminder_coordinator_ start]);
    ShowSetTabReminderUI(bookmark_);
    EXPECT_OCMOCK_VERIFY(mock_reminder_coordinator_class_);
  }

  base::test::ScopedFeatureList feature_list_;
  ScopedKeyWindow scoped_key_window_;
  UIViewController* base_view_controller_;
  id mock_snackbar_handler_;
  id mock_scene_handler_;
  id mock_settings_handler_;
  // Class mock of ReminderNotificationsCoordinator, and the same object typed
  // as an instance of the mocked class.
  id mock_reminder_coordinator_class_;
  ReminderNotificationsCoordinator* mock_reminder_coordinator_;
  // Bookmark the "Set a reminder" UI is shown for.
  raw_ptr<const bookmarks::BookmarkNode> bookmark_ = nullptr;
  BookmarksCoordinator* coordinator_;
};

// Tests that the "Set a reminder" UI is stopped when the bookmark browser is
// dismissed programmatically while it is shown, and that it can be shown again
// the next time the bookmark browser is presented (crbug.com/446105496).
TEST_F(BookmarksCoordinatorTest, StopsReminderUIWhenBookmarksAreDismissed) {
  PresentBookmarksAndShowReminderUI();

  // The "Set a reminder" UI is not dismissed by the user here: the bookmark
  // browser it is presented on is dismissed by its owner.
  OCMExpect([mock_reminder_coordinator_ stop]);
  [coordinator_ dismissBookmarkModalControllerAnimated:NO];
  EXPECT_OCMOCK_VERIFY(mock_reminder_coordinator_class_);

  PresentBookmarksAndShowReminderUI();
}

// Tests that the "Set a reminder" UI is stopped when the coordinator is
// stopped while it is shown.
TEST_F(BookmarksCoordinatorTest, StopsReminderUIWhenStopped) {
  PresentBookmarksAndShowReminderUI();

  OCMExpect([mock_reminder_coordinator_ stop]);
  [coordinator_ stop];
  EXPECT_OCMOCK_VERIFY(mock_reminder_coordinator_class_);
}

// Tests that asking for the "Set a reminder" UI while it is already shown
// stops the previous one instead of crashing.
TEST_F(BookmarksCoordinatorTest, StopsPreviousReminderUIWhenShownAgain) {
  PresentBookmarksAndShowReminderUI();

  OCMExpect([mock_reminder_coordinator_ stop]);
  OCMExpect([mock_reminder_coordinator_ start]);
  ShowSetTabReminderUI(bookmark_);
  EXPECT_OCMOCK_VERIFY(mock_reminder_coordinator_class_);
}
