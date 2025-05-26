// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_groups_constants.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"

NSString* const kCreateTabGroupViewIdentifier =
    @"kCreateTabGroupViewIdentifier";
NSString* const kCreateTabGroupTextFieldIdentifier =
    @"kCreateTabGroupTextFieldIdentifier";
NSString* const kCreateTabGroupTextFieldClearButtonIdentifier =
    @"kCreateTabGroupTextFieldClearButtonIdentifier";
NSString* const kCreateTabGroupCreateButtonIdentifier =
    @"kCreateTabGroupCreateButtonIdentifier";
NSString* const kCreateTabGroupCancelButtonIdentifier =
    @"kCreateTabGroupCancelButtonIdentifier";

NSString* const kTabGroupViewIdentifier = @"kTabGroupViewIdentifier";
NSString* const kTabGroupViewTitleIdentifier = @"kTabGroupViewTitleIdentifier";
NSString* const kTabGroupNewTabButtonIdentifier =
    @"kTabGroupNewTabButtonIdentifier";
NSString* const kTabGroupOverflowMenuButtonIdentifier =
    @"kTabGroupOverflowMenuButtonIdentifier";
NSString* const kTabGroupCloseButtonIdentifier =
    @"kTabGroupCloseButtonIdentifier";
NSString* const kTabGroupFacePileButtonIdentifier =
    @"kTabGroupFacePileButtonIdentifier";

UIColor* TabGroupViewButtonBackgroundColor() {
  return [[UIColor colorNamed:kGrey200Color] colorWithAlphaComponent:0.35];
}

NSString* const kTabGroupRecentActivityIdentifier =
    @"kTabGroupRecentActivityIdentifier";

NSString* const kTabGroupsPanelIdentifier = @"kTabGroupsPanelIdentifier";

NSString* const kTabGroupsPanelNotificationCellIdentifierPrefix =
    @"kTabGroupsPanelNotificationCellIdentifier";
NSString* const kTabGroupsPanelCellIdentifierPrefix =
    @"kTabGroupsPanelCellIdentifier";

NSString* const kTabGroupsPanelCloseNotificationIdentifier =
    @"kTabGroupsPanelCloseNotificationIdentifier";

NSString* const kSharedTabGroupUserEducationAccessibilityIdentifier =
    @"SharedTabGroupUserEducationAccessibilityIdentifier";
NSString* const kSharedTabGroupUserEducationShownOnceKey =
    @"SharedTabGroupUserEducationShownOnceKey";

NSString* const kRecentActivityLogCellIdentifierPrefix =
    @"kRecentActivityLogCellIdentifier";
