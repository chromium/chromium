// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_mediator.h"

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_mode_holder.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_toolbars_configuration.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/test/fake_tab_grid_toolbars_mediator.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/gtest_support.h"

using RecentTabsMediatorTest = PlatformTest;

// Tests the disabled configuration of the toolbar buttons.
TEST_F(RecentTabsMediatorTest, disabledConfiguration) {
  FakeTabGridToolbarsMediator* fake_toolbars_mediator =
      [[FakeTabGridToolbarsMediator alloc] init];
  TabGridModeHolder* mode_holder = [[TabGridModeHolder alloc] init];

  RecentTabsMediator* mediator =
      [[RecentTabsMediator alloc] initWithSessionSyncService:nullptr
                                             identityManager:nullptr
                                              restoreService:nullptr
                                               faviconLoader:nil
                                                 syncService:nullptr
                                                 browserList:nullptr
                                                  sceneState:nil
                                            disabledByPolicy:YES
                                           engagementTracker:nullptr
                                                  modeHolder:mode_holder];
  mediator.toolbarsMutator = fake_toolbars_mediator;

  [mediator currentlySelectedGrid:YES];

  EXPECT_EQ(TabGridPageRemoteTabs, fake_toolbars_mediator.configuration.page);

  EXPECT_FALSE(fake_toolbars_mediator.configuration.selectAllButton);
  EXPECT_FALSE(fake_toolbars_mediator.configuration.doneButton);
  EXPECT_EQ(0u, fake_toolbars_mediator.configuration.selectedItemsCount);
  EXPECT_FALSE(fake_toolbars_mediator.configuration.closeSelectedTabsButton);
  EXPECT_FALSE(fake_toolbars_mediator.configuration.shareButton);
  EXPECT_FALSE(fake_toolbars_mediator.configuration.addToButton);

  EXPECT_FALSE(fake_toolbars_mediator.configuration.closeAllButton);
  EXPECT_FALSE(fake_toolbars_mediator.configuration.newTabButton);
  EXPECT_FALSE(fake_toolbars_mediator.configuration.searchButton);
  EXPECT_FALSE(fake_toolbars_mediator.configuration.selectTabsButton);
  EXPECT_FALSE(fake_toolbars_mediator.configuration.undoButton);
  EXPECT_FALSE(fake_toolbars_mediator.configuration.deselectAllButton);
  EXPECT_FALSE(fake_toolbars_mediator.configuration.cancelSearchButton);
}
