// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_group_confirmation_coordinator.h"

#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/coordinator/alert/action_sheet_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_groups_commands.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@implementation TabGroupConfirmationCoordinator {
  // The action type that a tab group is going to take.
  TabGroupActionType _actionType;
  // The source view where the confirmation dialog anchors to.
  UIView* _sourceView;
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

#pragma mark - ChromeCoordinator

- (void)start {
  CHECK(self.action);
  _actionSheetCoordinator = [[ActionSheetCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                           title:[self sheetTitle]
                         message:[self sheetMessage]
                            rect:_sourceView.bounds
                            view:_sourceView];
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
                                       [weakSelf dismissActionSheetCoordinator];
                                     }
                                      style:UIAlertActionStyleCancel];
  [_actionSheetCoordinator start];
}

- (void)stop {
  [self dismissActionSheetCoordinator];
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
      // TODO(crbug.com/329631586): Implement the confirmation for ungrouping a
      // tab group.
      break;
    case TabGroupActionType::kDeleteTabGroup:
      return l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_DELETEGROUP);
  }
  NOTREACHED();
}

// Returns a string used in the action sheet as a title.
- (NSString*)sheetTitle {
  switch (_actionType) {
    case TabGroupActionType::kUngroupTabGroup:
      // TODO(crbug.com/329631586): Implement the confirmation for ungrouping a
      // tab group.
    case TabGroupActionType::kDeleteTabGroup:
      return l10n_util::GetNSString(
          IDS_IOS_TAB_GROUP_CONFIRMATION_DELETE_TITLE);
  }
  NOTREACHED();
}

// Returns a string used in the action sheet as a message.
- (NSString*)sheetMessage {
  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState());
  id<SystemIdentity> identity =
      authenticationService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);

  switch (_actionType) {
    case TabGroupActionType::kUngroupTabGroup:
      // TODO(crbug.com/329631586): Implement the confirmation for ungrouping a
      // tab group.
    case TabGroupActionType::kDeleteTabGroup:
      if (identity) {
        return l10n_util::GetNSStringF(
            IDS_IOS_TAB_GROUP_CONFIRMATION_DELETE_MESSAGE_WITH_EMAIL,
            base::SysNSStringToUTF16(identity.userEmail));
      } else {
        return l10n_util::GetNSString(
            IDS_IOS_TAB_GROUP_CONFIRMATION_DELETE_MESSAGE_WITHOUT_EMAIL);
      }
  }
  NOTREACHED();
}

@end
