// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/home/bookmarks_home_view_controller.h"

#import "base/test/metrics/user_action_tester.h"
#import "base/test/scoped_feature_list.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/common/bookmark_features.h"
#import "ios/chrome/browser/bookmarks/bookmark_ios_unit_test_support.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_model.h"
#import "ios/chrome/browser/ui/bookmarks/home/bookmarks_home_mediator.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class BookmarksHomeViewControllerTest
    : public BookmarkIOSUnitTestSupport,
      public testing::WithParamInterface<bool> {
 protected:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatureState(
        bookmarks::kEnableBookmarksAccountStorage, IsAccountStorageEnabled());
    BookmarkIOSUnitTestSupport::SetUp();
  }

  bool IsAccountStorageEnabled() const { return GetParam(); }
};

TEST_P(BookmarksHomeViewControllerTest,
       TableViewPopulatedAfterBookmarkModelLoaded) {
  @autoreleasepool {
    id mockSnackbarCommandHandler =
        OCMProtocolMock(@protocol(SnackbarCommands));

    // Set up ApplicationCommands mock. Because ApplicationCommands conforms
    // to ApplicationSettingsCommands, that needs to be mocked and dispatched
    // as well.
    id mockApplicationCommandHandler =
        OCMProtocolMock(@protocol(ApplicationCommands));
    id mockApplicationSettingsCommandHandler =
        OCMProtocolMock(@protocol(ApplicationSettingsCommands));

    CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
    [dispatcher startDispatchingToTarget:mockSnackbarCommandHandler
                             forProtocol:@protocol(SnackbarCommands)];
    [dispatcher startDispatchingToTarget:mockApplicationCommandHandler
                             forProtocol:@protocol(ApplicationCommands)];
    [dispatcher
        startDispatchingToTarget:mockApplicationSettingsCommandHandler
                     forProtocol:@protocol(ApplicationSettingsCommands)];

    BookmarksHomeViewController* controller =
        [[BookmarksHomeViewController alloc] initWithBrowser:browser_.get()];
    controller.applicationCommandsHandler = mockApplicationCommandHandler;
    controller.snackbarCommandsHandler = mockSnackbarCommandHandler;

    const bookmarks::BookmarkNode* mobileNode =
        local_or_syncable_bookmark_model_->mobile_node();
    AddBookmark(mobileNode, u"foo");
    controller.displayedFolderNode = mobileNode;
    // sections: Bookmarks, root profile, root account, message.
    EXPECT_EQ(4, [controller numberOfSectionsInTableView:controller.tableView]);
    EXPECT_EQ(1, [controller tableView:controller.tableView
                     numberOfRowsInSection:
                         [controller.tableViewModel
                             sectionForSectionIdentifier:
                                 BookmarksHomeSectionIdentifierBookmarks]]);
    EXPECT_EQ(0, [controller tableView:controller.tableView
                     numberOfRowsInSection:
                         [controller.tableViewModel
                             sectionForSectionIdentifier:
                                 BookmarksHomeSectionIdentifierRootProfile]]);
    EXPECT_EQ(0, [controller tableView:controller.tableView
                     numberOfRowsInSection:
                         [controller.tableViewModel
                             sectionForSectionIdentifier:
                                 BookmarksHomeSectionIdentifierRootAccount]]);
    EXPECT_EQ(0, [controller tableView:controller.tableView
                     numberOfRowsInSection:
                         [controller.tableViewModel
                             sectionForSectionIdentifier:
                                 BookmarksHomeSectionIdentifierMessages]]);
  }
}

TEST_P(BookmarksHomeViewControllerTest,
       TableViewPopulatedAfterBookmarkModelLoadedAtRootLevel) {
  @autoreleasepool {
    id mockSnackbarCommandHandler =
        OCMProtocolMock(@protocol(SnackbarCommands));

    // Set up ApplicationCommands mock. Because ApplicationCommands conforms
    // to ApplicationSettingsCommands, that needs to be mocked and dispatched
    // as well.
    id mockApplicationCommandHandler =
        OCMProtocolMock(@protocol(ApplicationCommands));
    id mockApplicationSettingsCommandHandler =
        OCMProtocolMock(@protocol(ApplicationSettingsCommands));

    CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
    [dispatcher startDispatchingToTarget:mockSnackbarCommandHandler
                             forProtocol:@protocol(SnackbarCommands)];
    [dispatcher startDispatchingToTarget:mockApplicationCommandHandler
                             forProtocol:@protocol(ApplicationCommands)];
    [dispatcher
        startDispatchingToTarget:mockApplicationSettingsCommandHandler
                     forProtocol:@protocol(ApplicationSettingsCommands)];

    BookmarksHomeViewController* controller =
        [[BookmarksHomeViewController alloc] initWithBrowser:browser_.get()];
    controller.applicationCommandsHandler = mockApplicationCommandHandler;
    controller.snackbarCommandsHandler = mockSnackbarCommandHandler;

    const bookmarks::BookmarkNode* rootNode =
        local_or_syncable_bookmark_model_->root_node();
    const bookmarks::BookmarkNode* mobileNode =
        local_or_syncable_bookmark_model_->mobile_node();
    AddBookmark(mobileNode, u"foo");  // Ensure there are bookmarks
    controller.displayedFolderNode = rootNode;
    // sections: Promo, Bookmarks, root profile, root account, message.
    EXPECT_EQ(5, [controller numberOfSectionsInTableView:controller.tableView]);
    EXPECT_EQ(1, [controller tableView:controller.tableView
                     numberOfRowsInSection:
                         [controller.tableViewModel
                             sectionForSectionIdentifier:
                                 BookmarksHomeSectionIdentifierPromo]]);
    EXPECT_EQ(0, [controller tableView:controller.tableView
                     numberOfRowsInSection:
                         [controller.tableViewModel
                             sectionForSectionIdentifier:
                                 BookmarksHomeSectionIdentifierBookmarks]]);
    EXPECT_EQ(1, [controller tableView:controller.tableView
                     numberOfRowsInSection:
                         [controller.tableViewModel
                             sectionForSectionIdentifier:
                                 BookmarksHomeSectionIdentifierRootProfile]]);
    EXPECT_EQ(0, [controller tableView:controller.tableView
                     numberOfRowsInSection:
                         [controller.tableViewModel
                             sectionForSectionIdentifier:
                                 BookmarksHomeSectionIdentifierRootAccount]]);
    EXPECT_EQ(0, [controller tableView:controller.tableView
                     numberOfRowsInSection:
                         [controller.tableViewModel
                             sectionForSectionIdentifier:
                                 BookmarksHomeSectionIdentifierMessages]]);
  }
}

// Checks that metrics are correctly reported.
TEST_P(BookmarksHomeViewControllerTest, Metrics) {
  @autoreleasepool {
    id mockSnackbarCommandHandler =
        OCMProtocolMock(@protocol(SnackbarCommands));

    // Set up ApplicationCommands mock. Because ApplicationCommands conforms
    // to ApplicationSettingsCommands, that needs to be mocked and dispatched
    // as well.
    id mockApplicationCommandHandler =
        OCMProtocolMock(@protocol(ApplicationCommands));
    id mockApplicationSettingsCommandHandler =
        OCMProtocolMock(@protocol(ApplicationSettingsCommands));

    CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
    [dispatcher startDispatchingToTarget:mockSnackbarCommandHandler
                             forProtocol:@protocol(SnackbarCommands)];
    [dispatcher startDispatchingToTarget:mockApplicationCommandHandler
                             forProtocol:@protocol(ApplicationCommands)];
    [dispatcher
        startDispatchingToTarget:mockApplicationSettingsCommandHandler
                     forProtocol:@protocol(ApplicationSettingsCommands)];

    BookmarksHomeViewController* controller =
        [[BookmarksHomeViewController alloc] initWithBrowser:browser_.get()];
    controller.applicationCommandsHandler = mockApplicationCommandHandler;
    controller.snackbarCommandsHandler = mockSnackbarCommandHandler;

    controller.displayedFolderNode =
        local_or_syncable_bookmark_model_->mobile_node();
    base::UserActionTester user_action_tester;
    std::string user_action = "MobileKeyCommandClose";
    ASSERT_EQ(user_action_tester.GetActionCount(user_action), 0);

    [controller keyCommand_close];

    EXPECT_EQ(user_action_tester.GetActionCount(user_action), 1);
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         BookmarksHomeViewControllerTest,
                         ::testing::Bool());

}  // namespace
