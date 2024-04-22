// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/coordinator/alert/alert_coordinator.h"

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/strings/grit/ui_strings.h"

@interface AlertCoordinator () {
  // Variable backing a property from Subclassing category.
  UIAlertController* _alertController;
}

// Redefined to readwrite.
@property(nonatomic, readwrite, getter=isVisible) BOOL visible;

// Called when the alert is dismissed to perform cleanup.
- (void)alertDismissed;

@end

@implementation AlertCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                     title:(NSString*)title
                                   message:(NSString*)message {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    [self commonInitWithTitle:title message:message];
  }
  return self;
}

- (void)commonInitWithTitle:(NSString*)title message:(NSString*)message {
  _title = title;
  _message = message;
}

#pragma mark - Public Methods.

- (void)addItemWithTitle:(NSString*)title
                  action:(ProceduralBlock)actionBlock
                   style:(UIAlertActionStyle)style {
  [self addItemWithTitle:title action:actionBlock style:style enabled:YES];
}

- (void)addItemWithTitle:(NSString*)title
                  action:(ProceduralBlock)actionBlock
                   style:(UIAlertActionStyle)style
                 enabled:(BOOL)enabled {
  [self addItemWithTitle:title
                  action:actionBlock
                   style:style
               preferred:NO
                 enabled:YES];
}

- (void)addItemWithTitle:(NSString*)title
                  action:(ProceduralBlock)actionBlock
                   style:(UIAlertActionStyle)style
               preferred:(BOOL)preferred
                 enabled:(BOOL)enabled {
  CHECK(!self.visible);

  if (style == UIAlertActionStyleCancel) {
    CHECK(!self.cancelButtonAdded);
    _cancelButtonAdded = YES;
  }

  __weak AlertCoordinator* weakSelf = self;

  UIAlertAction* alertAction =
      [UIAlertAction actionWithTitle:title
                               style:style
                             handler:^(UIAlertAction*) {
                               [weakSelf alertDismissed];
                               if (actionBlock) {
                                 actionBlock();
                               }
                             }];

  alertAction.accessibilityIdentifier =
      [title stringByAppendingString:@"AlertAction"];
  alertAction.enabled = enabled;

  [self.alertController addAction:alertAction];
  if (preferred) {
    DCHECK(![self.alertController preferredAction]);
    [self.alertController setPreferredAction:alertAction];
  }
}

- (void)start {
  // Check that the view is still visible on screen, otherwise just return and
  // don't show the context menu.
  if (![self.baseViewController.view window] &&
      ![self.baseViewController.view isKindOfClass:[UIWindow class]]) {
    return;
  }

  // Display at least one button to let the user dismiss the alert.
  if ([self.alertController actions].count == 0) {
    [self addItemWithTitle:l10n_util::GetNSString(IDS_APP_OK)
                    action:nil
                     style:UIAlertActionStyleDefault];
  }

  [self.baseViewController presentViewController:self.alertController
                                        animated:YES
                                      completion:nil];
  self.visible = YES;
}

- (void)stop {
  ProceduralBlock noInteractionAction = _noInteractionAction;
  _noInteractionAction = nil;
  [[_alertController presentingViewController]
      dismissViewControllerAnimated:NO
                         completion:nil];
  [self alertDismissed];
  if (noInteractionAction) {
    // This callback might deallocate `self`. Nothing should be done after
    // calling `noInteractionAction()`.
    noInteractionAction();
  }
}

#pragma mark - Property Implementation.

- (UIAlertController*)alertController {
  if (!_alertController) {
    UIAlertController* alert = [self alertControllerWithTitle:_title
                                                      message:_message];

    if (alert) {
      _alertController = alert;
    }
  }
  return _alertController;
}

- (NSString*)message {
  return _message;
}

#pragma mark - Private Methods.

- (void)alertDismissed {
  self.visible = NO;
  _cancelButtonAdded = NO;
  _alertController = nil;
  _noInteractionAction = nil;
}

- (UIAlertController*)alertControllerWithTitle:(NSString*)title
                                       message:(NSString*)message {
  return
      [UIAlertController alertControllerWithTitle:title
                                          message:message
                                   preferredStyle:UIAlertControllerStyleAlert];
}

@end
