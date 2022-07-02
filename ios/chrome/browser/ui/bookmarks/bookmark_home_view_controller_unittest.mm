// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bookmark_home_view_controller.h"

#include "components/bookmarks/browser/bookmark_model.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#include "ios/chrome/browser/ui/bookmarks/bookmark_ios_unittest.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/snackbar_commands.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

using BookmarkHomeViewControllerTest = BookmarkIOSUnitTest;

TEST_F(BookmarkHomeViewControllerTest,
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

    BookmarkHomeViewController* controller =
        [[BookmarkHomeViewController alloc] initWithBrowser:browser_.get()];
    controller.applicationCommandsHandler = mockApplicationCommandHandler;
    controller.snackbarCommandsHandler = mockSnackbarCommandHandler;

    [controller setRootNode:bookmark_model_->mobile_node()];
    // Two sections: Messages and Bookmarks.
    EXPECT_EQ(2, [controller numberOfSectionsInTableView:controller.tableView]);
  }
}

}  // namespace
