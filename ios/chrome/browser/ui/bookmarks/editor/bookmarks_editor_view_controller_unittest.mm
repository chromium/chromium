// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/editor/bookmarks_editor_view_controller.h"
#import "ios/chrome/browser/ui/bookmarks/editor/bookmarks_editor_mediator.h"

#import "base/test/metrics/user_action_tester.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_ios_unit_test_support.h"
#import "testing/platform_test.h"

namespace {

class BookmarksEditorViewControllerTest : public BookmarkIOSUnitTestSupport {
 protected:
  void SetUp() override {
    BookmarkIOSUnitTestSupport::SetUp();
    _controller =
        [[BookmarksEditorViewController alloc] initWithName:@"name"
                                                        URL:@"https://a.b.com"
                                                 folderName:@"folder"];
  }

  BookmarksEditorViewController* _controller;
};

// Checks that the view controller can become the first responder. This is
// needed to correctly register key commands.
TEST_F(BookmarksEditorViewControllerTest, CanBecomeFirstResponder) {
  EXPECT_TRUE(_controller.canBecomeFirstResponder);
}

// Checks that key commands are registered.
TEST_F(BookmarksEditorViewControllerTest, KeyCommands) {
  EXPECT_GT(_controller.keyCommands.count, 0u);
  EXPECT_GT(_controller.keyCommands.count, 0u);
}

// Checks that metrics are correctly reported.
TEST_F(BookmarksEditorViewControllerTest, Metrics) {
  base::UserActionTester user_action_tester;
  std::string user_action = "MobileKeyCommandClose";
  ASSERT_EQ(user_action_tester.GetActionCount(user_action), 0);

  [_controller keyCommand_close];

  EXPECT_EQ(user_action_tester.GetActionCount(user_action), 1);
}

// Regression test for See crbug.com/1429435
// Checks sync can safely occurs before the view is loaded.
TEST_F(BookmarksEditorViewControllerTest, CanSyncBeforeLoad) {
  const bookmarks::BookmarkNode* mobile_node =
      local_or_syncable_bookmark_model_->mobile_node();
  const bookmarks::BookmarkNode* bookmark = AddBookmark(mobile_node, u"foo");
  BookmarksEditorMediator* mediator = [[BookmarksEditorMediator alloc]
      initWithLocalOrSyncableBookmarkModel:local_or_syncable_bookmark_model_
                      accountBookmarkModel:account_bookmark_model_
                              bookmarkNode:bookmark
                                     prefs:nullptr
                               syncService:nullptr
                              browserState:nullptr];
  _controller.mutator = mediator;
  [_controller updateSync];
  [mediator disconnect];
}

}  // namespace
