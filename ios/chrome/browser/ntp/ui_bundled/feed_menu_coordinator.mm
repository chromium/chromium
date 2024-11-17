// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/ui_bundled/feed_menu_coordinator.h"

#import "components/feed/core/v2/public/ios/pref_names.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/coordinator/alert/action_sheet_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "ui/strings/grit/ui_strings.h"

namespace {
// The size of the icons in the management context menu.
const CGFloat kManagementContextMenuIconSize = 18;
}  // namespace

@implementation FeedMenuCoordinator

#pragma mark - ChromeCoordinator

- (void)start {
  [self.browser->GetCommandDispatcher()
      startDispatchingToTarget:self
                   forProtocol:@protocol(FeedMenuCommands)];
}

- (void)stop {
  [self.browser->GetCommandDispatcher()
      stopDispatchingForProtocol:@protocol(FeedMenuCommands)];
  self.delegate = nil;
}

#pragma mark - FeedMenuCommands

- (void)configureManagementMenu:(UIButton*)button {
  NSMutableArray<UIAction*>* feedActions = [NSMutableArray array];

  FeedMenuItemType toggle = [self isFeedExpanded] ? FeedMenuItemType::kTurnOff
                                                  : FeedMenuItemType::kTurnOn;
  [feedActions addObject:[self createMenuAction:toggle]];

  if ([self isSignedIn]) {
    if (IsWebChannelsEnabled()) {
      // Following feed is available.
      [feedActions addObject:[self createMenuAction:FeedMenuItemType::kManage]];
    } else {
      [feedActions
          addObject:[self createMenuAction:FeedMenuItemType::kManageActivity]];
      [feedActions
          addObject:[self createMenuAction:FeedMenuItemType::kManageFollowing]];
    }
  }

  [feedActions addObject:[self createMenuAction:FeedMenuItemType::kLearnMore]];

  button.menu = [UIMenu menuWithTitle:@"" children:feedActions];
}

#pragma mark - Private methods

// True if the feed is currently expanded. When not expanded, the feed header
// is shown, but not any of the feed content.
- (bool)isFeedExpanded {
  if (IsHomeCustomizationEnabled()) {
    return YES;
  }
  PrefService* prefService =
      ProfileIOS::FromBrowserState(self.browser->GetProfile())->GetPrefs();
  return prefService->GetBoolean(feed::prefs::kArticlesListVisible);
}

// True if the user is signed in.
- (bool)isSignedIn {
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForProfile(self.browser->GetProfile());
  return authService->HasPrimaryIdentity(signin::ConsentLevel::kSignin);
}

// Returns the message id to use for the title for an item of the given `type`.
- (int)titleMessageIdForMenuAction:(FeedMenuItemType)type {
  switch (type) {
    case FeedMenuItemType::kTurnOff:
      return IDS_IOS_DISCOVER_FEED_MENU_TURN_OFF_ITEM;
    case FeedMenuItemType::kTurnOn:
      return IDS_IOS_DISCOVER_FEED_MENU_TURN_ON_ITEM;
    case FeedMenuItemType::kManage:
      return IDS_IOS_DISCOVER_FEED_MENU_MANAGE_ITEM;
    case FeedMenuItemType::kManageActivity:
      return IDS_IOS_DISCOVER_FEED_MENU_MANAGE_ACTIVITY_ITEM;
    case FeedMenuItemType::kManageFollowing:
      return IDS_IOS_DISCOVER_FEED_MENU_MANAGE_INTERESTS_ITEM;
    case FeedMenuItemType::kLearnMore:
      return IDS_IOS_DISCOVER_FEED_MENU_LEARN_MORE_ITEM;
  }
}

// Returns an image icon for the context menu item.
- (UIImage*)iconForMenuAction:(FeedMenuItemType)type {
  switch (type) {
    case FeedMenuItemType::kTurnOff:
      return DefaultSymbolWithPointSize(kMinusInCircleSymbol,
                                        kManagementContextMenuIconSize);
    case FeedMenuItemType::kTurnOn:
      return DefaultSymbolWithPointSize(kPlusInCircleSymbol,
                                        kManagementContextMenuIconSize);
    case FeedMenuItemType::kManage:
      return DefaultSymbolWithPointSize(kSliderHorizontalSymbol,
                                        kManagementContextMenuIconSize);
    case FeedMenuItemType::kManageActivity:
      return DefaultSymbolWithPointSize(kSliderHorizontalSymbol,
                                        kManagementContextMenuIconSize);
    case FeedMenuItemType::kManageFollowing:
      return DefaultSymbolWithPointSize(kSliderHorizontalSymbol,
                                        kManagementContextMenuIconSize);
    case FeedMenuItemType::kLearnMore:
      return DefaultSymbolWithPointSize(kInfoCircleSymbol,
                                        kManagementContextMenuIconSize);
  }
}

// Creates an action to be added to the management menu.
- (UIAction*)createMenuAction:(FeedMenuItemType)type {
  __weak __typeof(self) weakSelf = self;
  UIAction* menuAction =
      [UIAction actionWithTitle:l10n_util::GetNSString(
                                    [self titleMessageIdForMenuAction:type])
                          image:[self iconForMenuAction:type]
                     identifier:nil
                        handler:^(UIAction* action) {
                          [weakSelf.delegate didSelectFeedMenuItem:type];
                        }];
  return menuAction;
}

@end
