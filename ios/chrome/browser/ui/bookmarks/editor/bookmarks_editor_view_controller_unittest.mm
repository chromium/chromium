// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/editor/bookmarks_editor_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "base/test/metrics/user_action_tester.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "ios/chrome/browser/bookmarks/bookmark_ios_unit_test_support.h"
#import "testing/platform_test.h"

namespace {

using BookmarksEditorViewControllerTest = BookmarkIOSUnitTestSupport;

// Checks that the view controller can become the first responder. This is
// needed to correctly register key commands.
TEST_F(BookmarksEditorViewControllerTest, CanBecomeFirstResponder) {
  const bookmarks::BookmarkNode* bookmark =
      AddBookmark(bookmark_model_->mobile_node(), @"Some Bookmark");
  BookmarksEditorViewController* controller =
      [[BookmarksEditorViewController alloc] initWithBookmark:bookmark
                                                      browser:browser_.get()];

  EXPECT_TRUE(controller.canBecomeFirstResponder);
}

// Checks that key commands are registered.
TEST_F(BookmarksEditorViewControllerTest, KeyCommands) {
  const bookmarks::BookmarkNode* bookmark =
      AddBookmark(bookmark_model_->mobile_node(), @"Some Bookmark");
  BookmarksEditorViewController* controller =
      [[BookmarksEditorViewController alloc] initWithBookmark:bookmark
                                                      browser:browser_.get()];

  EXPECT_GT(controller.keyCommands.count, 0u);
}

// Checks that metrics are correctly reported.
TEST_F(BookmarksEditorViewControllerTest, Metrics) {
  const bookmarks::BookmarkNode* bookmark =
      AddBookmark(bookmark_model_->mobile_node(), @"Some Bookmark");
  BookmarksEditorViewController* controller =
      [[BookmarksEditorViewController alloc] initWithBookmark:bookmark
                                                      browser:browser_.get()];
  base::UserActionTester user_action_tester;
  std::string user_action = "MobileKeyCommandClose";
  ASSERT_EQ(user_action_tester.GetActionCount(user_action), 0);

  [controller keyCommand_close];

  EXPECT_EQ(user_action_tester.GetActionCount(user_action), 1);
}

}  // namespace
