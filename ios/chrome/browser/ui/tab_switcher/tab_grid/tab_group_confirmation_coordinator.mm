// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_group_confirmation_coordinator.h"

#import "base/strings/sys_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/coordinator/alert/action_sheet_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
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
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(deviceOrientationDidChange)
             name:UIDeviceOrientationDidChangeNotification
           object:nil];

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

- (void)deviceOrientationDidChange {
  [self dismissActionSheetCoordinator];
}

// Stops the action sheet coordinator currently showned and nullifies the
// instance.
- (void)dismissActionSheetCoordinator {
  [[NSNotificationCenter defaultCenter] removeObserver:self];

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
  ChromeBrowserState* browserState = self.browser->GetBrowserState();

  // Show a user's email in the message if it's not incognito and a user is
  // signed in.
  NSString* userEmail = nil;
  if (!browserState->IsOffTheRecord()) {
    AuthenticationService* authenticationService =
        AuthenticationServiceFactory::GetForBrowserState(browserState);
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
