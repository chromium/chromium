// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_group_confirmation_coordinator.h"

#import "base/notreached.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/coordinator/alert/action_sheet_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
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
  // TODO(crbug.com/329627336) Add `title` and `message` depending on
  // `actionType`.
  _actionSheetCoordinator = [[ActionSheetCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                           title:nil
                         message:nil
                            rect:_sourceView.bounds
                            view:_sourceView];

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

@end
