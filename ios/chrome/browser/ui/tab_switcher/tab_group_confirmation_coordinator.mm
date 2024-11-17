// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_group_confirmation_coordinator.h"

#import "base/strings/sys_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/coordinator/alert/action_sheet_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/tab_group_confirmation_commands.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_group_action_type.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@implementation TabGroupConfirmationCoordinator {
  // The action type that a tab group is going to take.
  TabGroupActionType _actionType;
  // The source view where the confirmation dialog anchors to.
  UIView* _sourceView;
  // The source button item where the confirmation dialog anchors to.
  UIBarButtonItem* _sourceButtonItem;
  // The action sheet coordinator, if one is currently being shown.
  ActionSheetCoordinator* _actionSheetCoordinator;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                actionType:(TabGroupActionType)actionType
                                sourceView:(UIView*)sourceView {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _actionType = actionType;
    _sourceView = sourceView;
  }
  return self;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                actionType:(TabGroupActionType)actionType
                          sourceButtonItem:(UIBarButtonItem*)sourceButtonItem {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _actionType = actionType;
    _sourceButtonItem = sourceButtonItem;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  CHECK(self.action);

  if (_sourceView) {
    _actionSheetCoordinator = [[ActionSheetCoordinator alloc]
        initWithBaseViewController:self.baseViewController
                           browser:self.browser
                             title:[self sheetTitle]
                           message:[self sheetMessage]
                              rect:_sourceView.bounds
                              view:_sourceView];
  } else {
    CHECK(_sourceButtonItem);
    _actionSheetCoordinator = [[ActionSheetCoordinator alloc]
        initWithBaseViewController:self.baseViewController
                           browser:self.browser
                             title:[self sheetTitle]
                           message:[self sheetMessage]
                     barButtonItem:_sourceButtonItem];
  }

  _actionSheetCoordinator.popoverArrowDirection =
      UIPopoverArrowDirectionDown | UIPopoverArrowDirectionUp;

  __weak TabGroupConfirmationCoordinator* weakSelf = self;
  [_actionSheetCoordinator addItemWithTitle:[self itemTitle]
                                     action:^{
                                       [weakSelf handleAction];
                                     }
                                      style:UIAlertActionStyleDestructive];
  [_actionSheetCoordinator addItemWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                                     action:^{
                                       [weakSelf stop];
                                     }
                                      style:UIAlertActionStyleCancel];
  [_actionSheetCoordinator start];

  [self.browser->GetCommandDispatcher()
      startDispatchingToTarget:self
                   forProtocol:@protocol(TabGroupConfirmationCommands)];
}

- (void)stop {
  [self dismissActionSheetCoordinator];

  [self.browser->GetCommandDispatcher()
      stopDispatchingForProtocol:@protocol(TabGroupConfirmationCommands)];
}

#pragma mark - TabGroupConfirmationCommands

- (void)dismissTabGroupConfirmation {
  [self stop];
}

#pragma mark - Private

// Helper method to execute an `action`.
- (void)handleAction {
  if (self.action) {
    self.action();
  }
}

// Stops the action sheet coordinator currently showned and nullifies the
// instance.
- (void)dismissActionSheetCoordinator {
  [_actionSheetCoordinator stop];
  _actionSheetCoordinator = nil;
}

// Returns a string used in the context menu.
- (NSString*)itemTitle {
  switch (_actionType) {
    case TabGroupActionType::kUngroupTabGroup:
      return l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_UNGROUP);
    case TabGroupActionType::kDeleteTabGroup:
      return l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_DELETEGROUP);
  }
}

// Returns a string used in the action sheet as a title.
- (NSString*)sheetTitle {
  switch (_actionType) {
    case TabGroupActionType::kUngroupTabGroup:
      return l10n_util::GetNSString(
          IDS_IOS_TAB_GROUP_CONFIRMATION_UNGROUP_TITLE);
    case TabGroupActionType::kDeleteTabGroup:
      return l10n_util::GetNSString(
          IDS_IOS_TAB_GROUP_CONFIRMATION_DELETE_TITLE);
  }
}

// Returns a string used in the action sheet as a message.
- (NSString*)sheetMessage {
  ProfileIOS* profile = self.browser->GetProfile();

  // Show a user's email in the message if it's not incognito and a user is
  // signed in.
  NSString* userEmail = nil;
  if (!profile->IsOffTheRecord()) {
    AuthenticationService* authenticationService =
        AuthenticationServiceFactory::GetForProfile(profile);
    id<SystemIdentity> identity = authenticationService->GetPrimaryIdentity(
        signin::ConsentLevel::kSignin);
    userEmail = identity.userEmail;
  }

  switch (_actionType) {
    case TabGroupActionType::kUngroupTabGroup:
      if (userEmail) {
        return l10n_util::GetNSStringF(
            IDS_IOS_TAB_GROUP_CONFIRMATION_UNGROUP_MESSAGE_WITH_EMAIL,
            base::SysNSStringToUTF16(userEmail));
      } else {
        return l10n_util::GetNSString(
            IDS_IOS_TAB_GROUP_CONFIRMATION_UNGROUP_MESSAGE_WITHOUT_EMAIL);
      }
    case TabGroupActionType::kDeleteTabGroup:
      if (userEmail) {
        return l10n_util::GetNSStringF(
            IDS_IOS_TAB_GROUP_CONFIRMATION_DELETE_MESSAGE_WITH_EMAIL,
            base::SysNSStringToUTF16(userEmail));
      } else {
        return l10n_util::GetNSString(
            IDS_IOS_TAB_GROUP_CONFIRMATION_DELETE_MESSAGE_WITHOUT_EMAIL);
      }
  }
}

@end
