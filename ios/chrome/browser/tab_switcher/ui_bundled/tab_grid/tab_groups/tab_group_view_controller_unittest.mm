// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_group_view_controller.h"

#import <memory>
#import <set>

#import "base/memory/weak_ptr.h"
#import "components/tab_groups/tab_group_color.h"
#import "components/tab_groups/tab_group_id.h"
#import "components/tab_groups/tab_group_visual_data.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/public/commands/tab_groups_commands.h"
#import "ios/web/public/web_state_id.h"
#import "testing/platform_test.h"

// Fake TabGroupsCommands handler recording the tab group edition requests.
@interface FakeTabGroupEditionHandler : NSObject <TabGroupsCommands>

// Number of times the edition of a tab group has been requested.
@property(nonatomic, readonly) int editionRequestCount;

// Returns the group passed to the last edition request, or nullptr if it is no
// longer valid.
- (const TabGroup*)lastEditedGroup;

@end

@implementation FakeTabGroupEditionHandler {
  base::WeakPtr<const TabGroup> _lastEditedGroup;
}

- (const TabGroup*)lastEditedGroup {
  return _lastEditedGroup.get();
}

#pragma mark - TabGroupsCommands

- (void)showTabGroup:(const TabGroup*)tabGroup {
}

- (void)hideTabGroup {
}

- (void)showTabGroupCreationForTabs:
    (const std::set<web::WebStateID>&)identifiers {
}

- (void)showTabGroupCreationWithoutTabs {
}

- (void)hideTabGroupCreationAnimated:(BOOL)animated {
}

- (void)showTabGroupEditionForGroup:(base::WeakPtr<const TabGroup>)tabGroup {
  _editionRequestCount++;
  _lastEditedGroup = tabGroup;
}

- (void)showActiveTab {
}

- (void)showTabGroupConfirmationForAction:(TabGroupActionType)actionType
                                    group:
                                        (base::WeakPtr<const TabGroup>)tabGroup
                               sourceView:(UIView*)sourceView {
}

- (void)startLeaveOrDeleteSharedGroup:(base::WeakPtr<const TabGroup>)group
                            forAction:(TabGroupActionType)actionType
                           sourceView:(UIView*)sourceView {
}

- (void)showTabGridTabGroupSnackbarAfterClosingGroups:
    (int)numberOfClosedGroups {
}

- (void)showRecentActivityForGroup:(base::WeakPtr<const TabGroup>)tabGroup {
}

- (void)showManageForGroup:(base::WeakPtr<const TabGroup>)tabGroup {
}

- (void)showShareForGroup:(base::WeakPtr<const TabGroup>)tabGroup {
}

@end

namespace {

class TabGroupViewControllerTest : public PlatformTest {
 protected:
  TabGroupViewControllerTest()
      : tab_group_(std::make_unique<TabGroup>(
            tab_groups::TabGroupId::GenerateNew(),
            tab_groups::TabGroupVisualData(
                u"Group",
                tab_groups::TabGroupColorId::kGrey))),
        handler_([[FakeTabGroupEditionHandler alloc] init]),
        view_controller_([[TabGroupViewController alloc]
            initWithHandler:handler_
                  incognito:NO
                   tabGroup:tab_group_.get()]) {}

  std::unique_ptr<TabGroup> tab_group_;
  FakeTabGroupEditionHandler* handler_;
  TabGroupViewController* view_controller_;
};

// Tests that tapping the title of the group requests its edition.
TEST_F(TabGroupViewControllerTest, TapTitleRequestsEdition) {
  ASSERT_EQ(0, handler_.editionRequestCount);
  ASSERT_EQ(nullptr, [handler_ lastEditedGroup]);

  [view_controller_ tabGroupHeaderDidTapTitle:nil];

  EXPECT_EQ(1, handler_.editionRequestCount);
  EXPECT_EQ(tab_group_.get(), [handler_ lastEditedGroup]);
}

// Tests that tapping the title after the group has been destroyed, while the
// view controller is still alive (e.g. during its dismissal animation), doesn't
// access the destroyed group.
TEST_F(TabGroupViewControllerTest, TapTitleAfterGroupDestruction) {
  tab_group_.reset();
  ASSERT_EQ(0, handler_.editionRequestCount);
  ASSERT_EQ(nullptr, [handler_ lastEditedGroup]);

  [view_controller_ tabGroupHeaderDidTapTitle:nil];

  EXPECT_EQ(1, handler_.editionRequestCount);
  EXPECT_EQ(nullptr, [handler_ lastEditedGroup]);
}

}  // namespace
