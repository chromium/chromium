// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/pinned_tabs/pinned_tabs_view_controller.h"

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/transitions/legacy_grid_transition_layout.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_item.h"
#import "ios/web/public/web_state_id.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/device_form_factor.h"

using PinnedTabsViewControllerTest = PlatformTest;

// Tests that when there is no selected item, there is no transition layout
// created.
TEST_F(PinnedTabsViewControllerTest, NoSelection_NoTransitionLayout) {
  // The Pinned Tabs feature is not available on iPad.
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    return;
  }

  PinnedTabsViewController* view_controller =
      [[PinnedTabsViewController alloc] init];
  web::WebStateID identifier_a = web::WebStateID::NewUnique();
  web::WebStateID identifier_b = web::WebStateID::NewUnique();
  NSArray* items = @[
    [[TabSwitcherItem alloc] initWithIdentifier:identifier_a],
    [[TabSwitcherItem alloc] initWithIdentifier:identifier_b],
  ];
  [view_controller populateItems:items selectedItemID:web::WebStateID()];

  EXPECT_NSEQ(view_controller.transitionLayout, nil);
}

// Tests that when there is a selected item, there is a transition layout
// created.
TEST_F(PinnedTabsViewControllerTest, Selection_TransitionLayout) {
  // The Pinned Tabs feature is not available on iPad.
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    return;
  }

  PinnedTabsViewController* view_controller =
      [[PinnedTabsViewController alloc] init];
  web::WebStateID identifier_a = web::WebStateID::NewUnique();
  web::WebStateID identifier_b = web::WebStateID::NewUnique();
  NSArray* items = @[
    [[TabSwitcherItem alloc] initWithIdentifier:identifier_a],
    [[TabSwitcherItem alloc] initWithIdentifier:identifier_b],
  ];
  [view_controller populateItems:items selectedItemID:identifier_a];

  EXPECT_NSNE(view_controller.transitionLayout, nil);
}

// Tests that when the selected item is unselected, there is no transition
// layout created.
TEST_F(PinnedTabsViewControllerTest, Unselection_NoTransitionLayout) {
  // The Pinned Tabs feature is not available on iPad.
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    return;
  }

  PinnedTabsViewController* view_controller =
      [[PinnedTabsViewController alloc] init];
  web::WebStateID identifier_a = web::WebStateID::NewUnique();
  web::WebStateID identifier_b = web::WebStateID::NewUnique();
  NSArray* items = @[
    [[TabSwitcherItem alloc] initWithIdentifier:identifier_a],
    [[TabSwitcherItem alloc] initWithIdentifier:identifier_b],
  ];
  [view_controller populateItems:items selectedItemID:identifier_a];

  // Unselect.
  [view_controller selectItemWithID:web::WebStateID()];

  EXPECT_NSEQ(view_controller.transitionLayout, nil);
}
