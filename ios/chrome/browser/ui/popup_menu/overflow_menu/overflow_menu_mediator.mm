// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_mediator.h"

#include "ios/chrome/browser/policy/browser_policy_connector_ios.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_swift.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

typedef void (^Handler)(void);

OverflowMenuAction* CreateOverflowMenuAction(int nameID,
                                             NSString* imageName,
                                             Handler handler) {
  NSString* name = l10n_util::GetNSString(nameID);
  return [[OverflowMenuAction alloc] initWithName:name
                                        imageName:imageName
                               enterpriseDisabled:NO
                                          handler:handler];
}

OverflowMenuDestination* CreateOverflowMenuDestination(int nameID,
                                                       NSString* imageName,
                                                       Handler handler) {
  NSString* name = l10n_util::GetNSString(nameID);
  return [[OverflowMenuDestination alloc] initWithName:name
                                             imageName:imageName
                                    enterpriseDisabled:NO
                                               handler:handler];
}

}  // namespace

@interface OverflowMenuMediator ()

@property(nonatomic, strong) OverflowMenuDestination* bookmarksDestination;
@property(nonatomic, strong) OverflowMenuDestination* downloadsDestination;
@property(nonatomic, strong) OverflowMenuDestination* historyDestination;
@property(nonatomic, strong) OverflowMenuDestination* passwordsDestination;
@property(nonatomic, strong) OverflowMenuDestination* readingListDestination;
@property(nonatomic, strong) OverflowMenuDestination* recentTabsDestination;
@property(nonatomic, strong) OverflowMenuDestination* settingsDestination;
@property(nonatomic, strong) OverflowMenuDestination* siteInfoDestination;

@property(nonatomic, strong) OverflowMenuAction* reloadAction;
@property(nonatomic, strong) OverflowMenuAction* openIncognitoTabAction;
@property(nonatomic, strong) OverflowMenuAction* bookmarkAction;

@end

@implementation OverflowMenuMediator

@synthesize overflowMenuModel = _overflowMenuModel;

- (OverflowMenuModel*)overflowMenuModel {
  if (!_overflowMenuModel) {
    _overflowMenuModel = [self createModel];
  }
  return _overflowMenuModel;
}

- (OverflowMenuModel*)createModel {
  self.bookmarksDestination = CreateOverflowMenuDestination(
      IDS_IOS_TOOLS_MENU_BOOKMARKS, @"overflow_menu_destination_bookmarks",
      ^{
      });
  self.downloadsDestination = CreateOverflowMenuDestination(
      IDS_IOS_TOOLS_MENU_DOWNLOADS, @"overflow_menu_destination_downloads",
      ^{
      });
  self.historyDestination = CreateOverflowMenuDestination(
      IDS_IOS_TOOLS_MENU_HISTORY, @"overflow_menu_destination_history",
      ^{
      });
  self.passwordsDestination = CreateOverflowMenuDestination(
      IDS_IOS_TOOLS_MENU_PASSWORDS, @"overflow_menu_destination_passwords",
      ^{
      });
  self.readingListDestination =
      CreateOverflowMenuDestination(IDS_IOS_TOOLS_MENU_READING_LIST,
                                    @"overflow_menu_destination_reading_list",
                                    ^{
                                    });
  self.recentTabsDestination = CreateOverflowMenuDestination(
      IDS_IOS_TOOLS_MENU_RECENT_TABS, @"overflow_menu_destination_recent_tabs",
      ^{
      });
  self.settingsDestination = CreateOverflowMenuDestination(
      IDS_IOS_TOOLS_MENU_SETTINGS, @"overflow_menu_destination_settings",
      ^{
      });
  self.siteInfoDestination =
      CreateOverflowMenuDestination(IDS_IOS_TOOLS_MENU_SITE_INFORMATION,
                                    @"overflow_menu_destination_site_info",
                                    ^{
                                    });

  self.reloadAction = CreateOverflowMenuAction(IDS_IOS_TOOLS_MENU_RELOAD,
                                               @"overflow_menu_action_reload",
                                               ^{
                                               });
  self.openIncognitoTabAction = CreateOverflowMenuAction(
      IDS_IOS_TOOLS_MENU_NEW_INCOGNITO_TAB, @"overflow_menu_action_incognito",
      ^{
      });

  OverflowMenuActionGroup* appActionsGroup =
      [[OverflowMenuActionGroup alloc] initWithGroupName:@"app_actions"
                                                 actions:@[
                                                   self.reloadAction,
                                                   self.openIncognitoTabAction,
                                                 ]];

  self.bookmarkAction = CreateOverflowMenuAction(
      IDS_IOS_TOOLS_MENU_ADD_TO_BOOKMARKS, @"overflow_menu_action_bookmark",
      ^{
      });

  OverflowMenuActionGroup* pageActionsGroup =
      [[OverflowMenuActionGroup alloc] initWithGroupName:@"page_actions"
                                                 actions:@[
                                                   self.bookmarkAction,
                                                 ]];

  return [[OverflowMenuModel alloc] initWithDestinations:@[
    self.bookmarksDestination,
    self.historyDestination,
    self.readingListDestination,
    self.passwordsDestination,
    self.downloadsDestination,
    self.recentTabsDestination,
    self.siteInfoDestination,
    self.settingsDestination,
  ]
                                            actionGroups:@[
                                              appActionsGroup,
                                              pageActionsGroup,
                                            ]];
}
@end
