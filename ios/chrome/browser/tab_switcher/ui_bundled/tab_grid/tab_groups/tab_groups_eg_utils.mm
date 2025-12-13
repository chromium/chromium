// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_groups_eg_utils.h"

#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_groups_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"

using base::test::ios::kWaitForUIElementTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;
using chrome_test_util::ContextMenuItemWithAccessibilityLabelId;
using chrome_test_util::CreateTabGroupTextField;
using chrome_test_util::TabGridCellAtIndex;
using chrome_test_util::TabGridGroupCellAtIndex;
using chrome_test_util::TabGroupCreationView;

namespace {

// Returns the matcher for the sub menu button `Add Tab to New Group`.
id<GREYMatcher> NewTabGroupButton() {
  return ContextMenuItemWithAccessibilityLabelId(
      IDS_IOS_CONTENT_CONTEXT_ADDTABTONEWTABGROUP_SUBMENU);
}

// Opens the tab group creation view using the long press context menu for the
// tab at `index`.
void OpenTabGroupCreationViewUsingLongPressForCellAtIndex(int index,
                                                          bool first_group) {
  base::PlatformThread::Sleep(base::Seconds(1));
  [[EarlGrey selectElementWithMatcher:TabGridCellAtIndex(index)]
      performAction:grey_longPressWithDuration(base::Seconds(1))];
  base::PlatformThread::Sleep(base::Seconds(1));
  if (first_group) {
    [[EarlGrey selectElementWithMatcher:
                   grey_text(l10n_util::GetPluralNSStringF(
                       IDS_IOS_CONTENT_CONTEXT_ADDTABTONEWTABGROUP, 1))]
        performAction:grey_tap()];
  } else {
    [[EarlGrey selectElementWithMatcher:
                   chrome_test_util::ContextMenuItemWithAccessibilityLabel(
                       l10n_util::GetPluralNSStringF(
                           IDS_IOS_CONTENT_CONTEXT_ADDTABTOTABGROUP, 1))]
        performAction:grey_tap()];
    [[EarlGrey selectElementWithMatcher:NewTabGroupButton()]
        performAction:grey_tap()];
  }

  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:TabGroupCreationView()];
}

// Sets the tab group name in the tab group creation view.
void SetTabGroupCreationName(NSString* group_name) {
  [[EarlGrey selectElementWithMatcher:CreateTabGroupTextField()]
      performAction:grey_tap()];
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:group_name flags:0];
}

}  // namespace

namespace chrome_test_util {

// Creates a tab group named `group_name` with the tab item at `index`.
// Set `first_group` to false when creating a subsequent group, as the context
// menu labels are different and EarlGrey needs to know exactly which labels to
// select.
void CreateTabGroupAtIndex(int index, NSString* group_name, bool first_group) {
  // Open the creation view.
  OpenTabGroupCreationViewUsingLongPressForCellAtIndex(index, first_group);
  SetTabGroupCreationName(group_name);

  // Validate the creation.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:CreateTabGroupCreateButton()];
  [[EarlGrey selectElementWithMatcher:CreateTabGroupCreateButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:TabGroupCreationView()];
}

void OpenTabGroupAtIndex(int group_cell_index) {
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:TabGridGroupCellAtIndex(
                                                          group_cell_index)];
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellAtIndex(group_cell_index)]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(
                                              kTabGroupViewTitleIdentifier)];
}

// Long presses a tab group cell.
void LongPressTabGroupCellAtIndex(unsigned int index) {
  // Make sure the cell has appeared. Otherwise, long pressing can be flaky.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:TabGridGroupCellAtIndex(index)];
  // It happens that on certain bots, the grey_longPress action doesn't return
  // an error for EarlGrey, but the context menu doesn't open accordingly.
  // Long press for a pre-determined duration to force the context menu to open.
  [[EarlGrey selectElementWithMatcher:TabGridGroupCellAtIndex(index)]
      performAction:grey_longPressWithDuration(base::Seconds(1))];
}

}  // namespace chrome_test_util
