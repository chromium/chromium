// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_group_confirmation_coordinator.h"

#import "base/strings/sys_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/coordinator/alert/action_sheet_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/tab_group_confirmation_commands.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_group_action_type.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@implementation TabGroupConfirmationCoordinator {
  // The action type that a tab group is going to take.
  TabGroupActionType _actionType;
  // The secondary action type that a tab group is going to take. Default value
  // is kNone.
  TabGroupActionType _secondaryActionType;
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
    _canCancel = YES;
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
    _canCancel = YES;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  CHECK(self.primaryAction);
  if ([self shouldHaveSecondaryAction]) {
    CHECK(self.secondaryAction);
  }

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

  _actionSheetCoordinator.alertStyle = _showAsAlert
                                           ? UIAlertControllerStyleAlert
                                           : UIAlertControllerStyleActionSheet;

  _actionSheetCoordinator.popoverArrowDirection =
      UIPopoverArrowDirectionDown | UIPopoverArrowDirectionUp;

  __weak TabGroupConfirmationCoordinator* weakSelf = self;
  [_actionSheetCoordinator addItemWithTitle:[self primaryItemTitle]
                                     action:^{
                                       [weakSelf handlePrimaryAction];
                                     }
                                      style:UIAlertActionStyleDestructive];
  if ([self shouldHaveSecondaryAction]) {
    UIAlertActionStyle secondaryStyle =
        self.canCancel ? UIAlertActionStyleDefault : UIAlertActionStyleCancel;
    [_actionSheetCoordinator addItemWithTitle:[self secondaryItemTitle]
                                       action:^{
                                         [weakSelf handleSecondaryAction];
                                       }
                                        style:secondaryStyle];
  }
  if (self.canCancel) {
    [_actionSheetCoordinator addItemWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                                       action:^{
                                         [weakSelf stop];
                                       }
                                        style:UIAlertActionStyleCancel];
  }
  [_actionSheetCoordinator start];
}

- (void)stop {
  [self dismissActionSheetCoordinator];
}

#pragma mark - TabGroupConfirmationCommands

- (void)dismissTabGroupConfirmation {
  [self stop];
}

#pragma mark - Private

// Helper methods to execute an `action`.
- (void)handlePrimaryAction {
  if (self.primaryAction) {
    self.primaryAction();
  }
}

- (void)handleSecondaryAction {
  if (self.secondaryAction) {
    self.secondaryAction();
  }
}

// Stops the action sheet coordinator currently showned and nullifies the
// instance.
- (void)dismissActionSheetCoordinator {
  if (self.dismissAction) {
    self.dismissAction();
  }
  [_actionSheetCoordinator stop];
  _actionSheetCoordinator = nil;
}

// Returns a string used in the first item of the context menu.
- (NSString*)primaryItemTitle {
  switch (_actionType) {
    case TabGroupActionType::kUngroupTabGroup:
      return l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_UNGROUP);
    case TabGroupActionType::kDeleteTabGroup:
      return l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_DELETEGROUP);
    case TabGroupActionType::kLeaveSharedTabGroup:
    case TabGroupActionType::kLeaveOrKeepSharedTabGroup:
      return l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_LEAVESHAREDGROUP);
    case TabGroupActionType::kDeleteSharedTabGroup:
    case TabGroupActionType::kDeleteOrKeepSharedTabGroup:
      return l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_DELETESHAREDGROUP);
  }
}

// Returns a string for the second item in the context menu.
- (NSString*)secondaryItemTitle {
  switch (_actionType) {
    case TabGroupActionType::kLeaveOrKeepSharedTabGroup:
    case TabGroupActionType::kDeleteOrKeepSharedTabGroup:
      return l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_KEEPSHAREDGROUP);
    case TabGroupActionType::kUngroupTabGroup:
    case TabGroupActionType::kDeleteTabGroup:
    case TabGroupActionType::kLeaveSharedTabGroup:
    case TabGroupActionType::kDeleteSharedTabGroup:
      NOTREACHED();
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
    case TabGroupActionType::kLeaveSharedTabGroup:
      return l10n_util::GetNSString(
          IDS_IOS_SHARED_TAB_GROUP_CONFIRMATION_LEAVE_TITLE);
    case TabGroupActionType::kDeleteSharedTabGroup:
      return l10n_util::GetNSString(
          IDS_IOS_SHARED_TAB_GROUP_CONFIRMATION_DELETE_TITLE);
    case TabGroupActionType::kDeleteOrKeepSharedTabGroup:
    case TabGroupActionType::kLeaveOrKeepSharedTabGroup:
      return l10n_util::GetNSString(IDS_IOS_TAB_GROUP_CONFIRMATION_KEEP_TITLE);
  }
}

// Returns a string used in the action sheet as a message.
- (NSString*)sheetMessage {
  // Show a user's email in the message if it's not incognito and a user is
  // signed in.
  NSString* userEmail = nil;
  if (!self.profile->IsOffTheRecord()) {
    AuthenticationService* authenticationService =
        AuthenticationServiceFactory::GetForProfile(self.profile);
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
    case TabGroupActionType::kLeaveSharedTabGroup: {
      CHECK(_tabGroupName);
      return l10n_util::GetNSStringF(
          IDS_IOS_SHARED_TAB_GROUP_CONFIRMATION_LEAVE_MESSAGE,
          base::SysNSStringToUTF16(_tabGroupName));
    }
    case TabGroupActionType::kDeleteSharedTabGroup: {
      CHECK(_tabGroupName);
      return l10n_util::GetNSStringF(
          IDS_IOS_SHARED_TAB_GROUP_CONFIRMATION_DELETE_MESSAGE,
          base::SysNSStringToUTF16(_tabGroupName));
    }
    case TabGroupActionType::kDeleteOrKeepSharedTabGroup:
      CHECK(_tabGroupName);
      return [NSString
          stringWithFormat:
              @"%@\n%@",
              l10n_util::GetNSString(
                  IDS_IOS_SHARED_TAB_GROUP_CONFIRMATION_KEEP_OR_DELETE_MESSAGE),
              l10n_util::GetNSStringF(
                  IDS_IOS_SHARED_TAB_GROUP_CONFIRMATION_KEEP_OR_DELETE_MESSAGE_EXPLANATION_PART,
                  base::SysNSStringToUTF16(_tabGroupName))];
    case TabGroupActionType::kLeaveOrKeepSharedTabGroup:
      return l10n_util::GetNSString(
          IDS_IOS_SHARED_TAB_GROUP_CONFIRMATION_KEEP_OR_LEAVE_MESSAGE);
  }
}

// Returns YES if the confirmation have two actions + Cancel instead of one
// action and Cancel only.
- (BOOL)shouldHaveSecondaryAction {
  return _actionType == TabGroupActionType::kDeleteOrKeepSharedTabGroup ||
         _actionType == TabGroupActionType::kLeaveOrKeepSharedTabGroup;
}

@end
