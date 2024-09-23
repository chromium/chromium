// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bookmarks/ui_bundled/home/bookmarks_home_view_controller.h"

#import "base/test/metrics/user_action_tester.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/common/bookmark_features.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_ios_unit_test_support.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/home/bookmarks_home_mediator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_model.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

using BookmarksHomeViewControllerTest = BookmarkIOSUnitTestSupport;

TEST_F(BookmarksHomeViewControllerTest,
       TableViewPopulatedAfterBookmarkModelLoaded) {
  @autoreleasepool {
    id mockSnackbarCommandHandler =
        OCMProtocolMock(@protocol(SnackbarCommands));

    // Set up ApplicationCommands mock. Because ApplicationCommands conforms
    // to SettingsCommands, that needs to be mocked and dispatched
    // as well.
    id mockApplicationCommandHandler =
        OCMProtocolMock(@protocol(ApplicationCommands));
    id mockSettingsCommandHandler =
        OCMProtocolMock(@protocol(SettingsCommands));

    CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
    [dispatcher startDispatchingToTarget:mockSnackbarCommandHandler
                             forProtocol:@protocol(SnackbarCommands)];
    [dispatcher startDispatchingToTarget:mockApplicationCommandHandler
                             forProtocol:@protocol(ApplicationCommands)];
    [dispatcher startDispatchingToTarget:mockSettingsCommandHandler
                             forProtocol:@protocol(SettingsCommands)];

    BookmarksHomeViewController* controller =
        [[BookmarksHomeViewController alloc] initWithBrowser:browser_.get()];
    controller.applicationCommandsHandler = mockApplicationCommandHandler;
    controller.snackbarCommandsHandler = mockSnackbarCommandHandler;

    const bookmarks::BookmarkNode* mobileNode = bookmark_model_->mobile_node();
    AddBookmark(mobileNode, u"foo");
    controller.displayedFolderNode = mobileNode;
    // sections: Bookmarks, root profile, root account, message, batch upload.
    EXPECT_EQ(5, [controller numberOfSectionsInTableView:controller.tableView]);
    EXPECT_EQ(1, [controller tableView:controller.tableView
                     numberOfRowsInSection:
                         [controller.tableViewModel
                             sectionForSectionIdentifier:
                                 BookmarksHomeSectionIdentifierBookmarks]]);
    EXPECT_EQ(
        0, [controller tableView:controller.tableView
               numberOfRowsInSection:
                   [controller.tableViewModel
                       sectionForSectionIdentifier:
                           BookmarksHomeSectionIdentifierRootLocalOrSyncable]]);
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
    EXPECT_EQ(0, [controller tableView:controller.tableView
                     numberOfRowsInSection:
                         [controller.tableViewModel
                             sectionForSectionIdentifier:
                                 BookmarksBatchUploadSectionIdentifier]]);
    [controller shutdown];
  }
}

TEST_F(BookmarksHomeViewControllerTest,
       TableViewPopulatedAfterBookmarkModelLoadedAtRootLevel) {
  @autoreleasepool {
    id mockSnackbarCommandHandler =
        OCMProtocolMock(@protocol(SnackbarCommands));

    // Set up ApplicationCommands mock. Because ApplicationCommands conforms
    // to SettingsCommands, that needs to be mocked and dispatched
    // as well.
    id mockApplicationCommandHandler =
        OCMProtocolMock(@protocol(ApplicationCommands));
    id mockSettingsCommandHandler =
        OCMProtocolMock(@protocol(SettingsCommands));

    CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
    [dispatcher startDispatchingToTarget:mockSnackbarCommandHandler
                             forProtocol:@protocol(SnackbarCommands)];
    [dispatcher startDispatchingToTarget:mockApplicationCommandHandler
                             forProtocol:@protocol(ApplicationCommands)];
    [dispatcher startDispatchingToTarget:mockSettingsCommandHandler
                             forProtocol:@protocol(SettingsCommands)];

    BookmarksHomeViewController* controller =
        [[BookmarksHomeViewController alloc] initWithBrowser:browser_.get()];
    controller.applicationCommandsHandler = mockApplicationCommandHandler;
    controller.snackbarCommandsHandler = mockSnackbarCommandHandler;

    const bookmarks::BookmarkNode* rootNode = bookmark_model_->root_node();
    const bookmarks::BookmarkNode* mobileNode = bookmark_model_->mobile_node();
    AddBookmark(mobileNode, u"foo");  // Ensure there are bookmarks
    controller.displayedFolderNode = rootNode;
    // sections: Promo, Bookmarks, root profile, root account, message, batch
    // upload.
    EXPECT_EQ(6, [controller numberOfSectionsInTableView:controller.tableView]);
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
    EXPECT_EQ(
        1, [controller tableView:controller.tableView
               numberOfRowsInSection:
                   [controller.tableViewModel
                       sectionForSectionIdentifier:
                           BookmarksHomeSectionIdentifierRootLocalOrSyncable]]);
    EXPECT_EQ(1, [controller tableView:controller.tableView
                     numberOfRowsInSection:
                         [controller.tableViewModel
                             sectionForSectionIdentifier:
                                 BookmarksHomeSectionIdentifierRootAccount]]);
    EXPECT_EQ(0, [controller tableView:controller.tableView
                     numberOfRowsInSection:
                         [controller.tableViewModel
                             sectionForSectionIdentifier:
                                 BookmarksHomeSectionIdentifierMessages]]);
    EXPECT_EQ(0, [controller tableView:controller.tableView
                     numberOfRowsInSection:
                         [controller.tableViewModel
                             sectionForSectionIdentifier:
                                 BookmarksBatchUploadSectionIdentifier]]);
    [controller shutdown];
  }
}

// Checks that metrics are correctly reported.
TEST_F(BookmarksHomeViewControllerTest, Metrics) {
  @autoreleasepool {
    id mockSnackbarCommandHandler =
        OCMProtocolMock(@protocol(SnackbarCommands));

    // Set up ApplicationCommands mock. Because ApplicationCommands conforms
    // to SettingsCommands, that needs to be mocked and dispatched
    // as well.
    id mockApplicationCommandHandler =
        OCMProtocolMock(@protocol(ApplicationCommands));
    id mockSettingsCommandHandler =
        OCMProtocolMock(@protocol(SettingsCommands));

    CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
    [dispatcher startDispatchingToTarget:mockSnackbarCommandHandler
                             forProtocol:@protocol(SnackbarCommands)];
    [dispatcher startDispatchingToTarget:mockApplicationCommandHandler
                             forProtocol:@protocol(ApplicationCommands)];
    [dispatcher startDispatchingToTarget:mockSettingsCommandHandler
                             forProtocol:@protocol(SettingsCommands)];

    BookmarksHomeViewController* controller =
        [[BookmarksHomeViewController alloc] initWithBrowser:browser_.get()];
    controller.applicationCommandsHandler = mockApplicationCommandHandler;
    controller.snackbarCommandsHandler = mockSnackbarCommandHandler;

    controller.displayedFolderNode = bookmark_model_->mobile_node();
    base::UserActionTester user_action_tester;
    std::string user_action = "MobileKeyCommandClose";
    ASSERT_EQ(user_action_tester.GetActionCount(user_action), 0);

    [controller keyCommand_close];

    EXPECT_EQ(user_action_tester.GetActionCount(user_action), 1);
    [controller shutdown];
  }
}

TEST_F(BookmarksHomeViewControllerTest, CachedViewControllerStack) {
  @autoreleasepool {
    id mockSnackbarCommandHandler =
        OCMProtocolMock(@protocol(SnackbarCommands));

    // Set up ApplicationCommands mock. Because ApplicationCommands conforms
    // to SettingsCommands, that needs to be mocked and dispatched
    // as well.
    id mockApplicationCommandHandler =
        OCMProtocolMock(@protocol(ApplicationCommands));
    id mockSettingsCommandHandler =
        OCMProtocolMock(@protocol(SettingsCommands));

    CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
    [dispatcher startDispatchingToTarget:mockSnackbarCommandHandler
                             forProtocol:@protocol(SnackbarCommands)];
    [dispatcher startDispatchingToTarget:mockApplicationCommandHandler
                             forProtocol:@protocol(ApplicationCommands)];
    [dispatcher startDispatchingToTarget:mockSettingsCommandHandler
                             forProtocol:@protocol(SettingsCommands)];

    const bookmarks::BookmarkNode* mobileNode = bookmark_model_->mobile_node();
    const bookmarks::BookmarkNode* folder = AddFolder(mobileNode, u"foo");
    AddBookmark(folder, u"bar");

    BookmarksHomeViewController* controller =
        [[BookmarksHomeViewController alloc] initWithBrowser:browser_.get()];
    controller.applicationCommandsHandler = mockApplicationCommandHandler;
    controller.snackbarCommandsHandler = mockSnackbarCommandHandler;
    controller.displayedFolderNode = folder;

    // Closing should populate the cache.
    [controller keyCommand_close];

    controller.displayedFolderNode = bookmark_model_->root_node();

    NSArray<BookmarksHomeViewController*>* stack =
        [controller cachedViewControllerStack];
    ASSERT_EQ(3u, stack.count);
    EXPECT_EQ(folder, stack[2].displayedFolderNode);
    EXPECT_EQ(mobileNode, stack[1].displayedFolderNode);
    EXPECT_EQ(bookmark_model_->root_node(), stack[0].displayedFolderNode);

    [stack[0] shutdown];
    [stack[1] shutdown];
    [stack[2] shutdown];
  }
}

}  // namespace
