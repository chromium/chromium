// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/feed_menu_coordinator.h"

#import "components/feed/core/v2/public/ios/pref_names.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/ntp/features.h"
#import "ios/chrome/browser/shared/coordinator/alert/action_sheet_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "ui/strings/grit/ui_strings.h"

@implementation FeedMenuCoordinator {
  // The action sheet coordinator that will display the menu.
  ActionSheetCoordinator* _actionSheetCoordinator;
}

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
  [self stopActionSheetCoordinator];
}

#pragma mark - FeedMenuCommands

- (void)openFeedMenuFromButton:(UIButton*)button {
  [self stopActionSheetCoordinator];

  _actionSheetCoordinator = [[ActionSheetCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                           title:nil
                         message:nil
                            rect:button.frame
                            view:button.superview];

  // Add all the appropriate menu items.
  FeedMenuItemType toggle = [self isFeedExpanded] ? FeedMenuItemType::kTurnOff
                                                  : FeedMenuItemType::kTurnOn;
  [self addItem:toggle];
  if ([self isSignedIn]) {
    if (IsWebChannelsEnabled()) {
      // Following feed is available.
      [self addItem:FeedMenuItemType::kManage];
    } else {
      [self addItem:FeedMenuItemType::kManageActivity];
      [self addItem:FeedMenuItemType::kManageInterests];
    }
  }
  [self addItem:FeedMenuItemType::kLearnMore];
  [self addItem:FeedMenuItemType::kCancel];

  [_actionSheetCoordinator start];
}

#pragma mark - Private methods

// True if the feed is currently expanded. When not expanded, the feed header
// is shown, but not any of the feed content.
- (bool)isFeedExpanded {
  PrefService* prefService =
      ChromeBrowserState::FromBrowserState(self.browser->GetBrowserState())
          ->GetPrefs();
  return prefService->GetBoolean(feed::prefs::kArticlesListVisible);
}

// True if the user is signed in.
- (bool)isSignedIn {
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState());
  return authService->HasPrimaryIdentity(signin::ConsentLevel::kSignin);
}

// Returns the action style for an item of the given `type`.
- (UIAlertActionStyle)actionStyleForItem:(FeedMenuItemType)type {
  switch (type) {
    case FeedMenuItemType::kCancel:
      return UIAlertActionStyleCancel;
    case FeedMenuItemType::kTurnOff:
      return UIAlertActionStyleDestructive;
    default:
      return UIAlertActionStyleDefault;
  }
}

// Returns the message id to use for the title for an item of the given `type`.
- (int)titleMessageIdForItem:(FeedMenuItemType)type {
  switch (type) {
    case FeedMenuItemType::kCancel:
      return IDS_APP_CANCEL;
    case FeedMenuItemType::kTurnOff:
      return IDS_IOS_DISCOVER_FEED_MENU_TURN_OFF_ITEM;
    case FeedMenuItemType::kTurnOn:
      return IDS_IOS_DISCOVER_FEED_MENU_TURN_ON_ITEM;
    case FeedMenuItemType::kManage:
      return IDS_IOS_DISCOVER_FEED_MENU_MANAGE_ITEM;
    case FeedMenuItemType::kManageActivity:
      return IDS_IOS_DISCOVER_FEED_MENU_MANAGE_ACTIVITY_ITEM;
    case FeedMenuItemType::kManageInterests:
      return IDS_IOS_DISCOVER_FEED_MENU_MANAGE_INTERESTS_ITEM;
    case FeedMenuItemType::kLearnMore:
      return IDS_IOS_DISCOVER_FEED_MENU_LEARN_MORE_ITEM;
  }
}

// Adds an item of the given `type` to the action sheet.
- (void)addItem:(FeedMenuItemType)type {
  __weak __typeof(self) weakSelf = self;
  NSString* title = l10n_util::GetNSString([self titleMessageIdForItem:type]);
  [_actionSheetCoordinator
      addItemWithTitle:title
                action:^{
                  [weakSelf.delegate didSelectFeedMenuItem:type];
                  [weakSelf stopActionSheetCoordinator];
                }
                 style:[self actionStyleForItem:type]];
}

// Stops and clears the ActionSheetCoordinator instance.
- (void)stopActionSheetCoordinator {
  [_actionSheetCoordinator stop];
  _actionSheetCoordinator = nil;
}

@end
