// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/coordinator/alert/alert_coordinator.h"

#import "ios/chrome/browser/main/browser.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/strings/grit/ui_strings.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface AlertCoordinator () {
  // Variable backing a property from Subclassing category.
  UIAlertController* _alertController;
}

// Redefined to readwrite.
@property(nonatomic, readwrite, getter=isVisible) BOOL visible;

// Cancel action passed using the public API.
// It will called from the overridden block stored in the `cancelAction`
// property.
@property(nonatomic, copy) ProceduralBlock rawCancelAction;

// Called when the alert is dismissed to perform cleanup.
- (void)alertDismissed;

@end

@implementation AlertCoordinator

@synthesize visible = _visible;
@synthesize cancelButtonAdded = _cancelButtonAdded;
@synthesize cancelAction = _cancelAction;
@synthesize startAction = _startAction;
@synthesize noInteractionAction = _noInteractionAction;
@synthesize rawCancelAction = _rawCancelAction;
@synthesize message = _message;
@synthesize title = _title;

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
  if (self.visible ||
      (style == UIAlertActionStyleCancel && self.cancelButtonAdded)) {
    return;
  }

  if (style == UIAlertActionStyleCancel) {
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
      [NSString stringWithFormat:@"%@%@", title, @"AlertAction"];
  alertAction.enabled = enabled;

  [self.alertController addAction:alertAction];
  if (preferred) {
    DCHECK(![self.alertController preferredAction]);
    [self.alertController setPreferredAction:alertAction];
  }
}

- (void)executeCancelHandler {
  self.noInteractionAction = nil;
  if (self.cancelAction) {
    self.cancelAction();
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

  // Call the start action before presenting the alert.
  if (self.startAction) {
    self.startAction();
  }

  [self.baseViewController presentViewController:self.alertController
                                        animated:YES
                                      completion:nil];
  self.visible = YES;
}

- (void)stop {
  if (_noInteractionAction) {
    _noInteractionAction();
    _noInteractionAction = nil;
  }
  [[_alertController presentingViewController]
      dismissViewControllerAnimated:NO
                         completion:nil];
  [self alertDismissed];
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

- (void)setCancelAction:(ProceduralBlock)cancelAction {
  __weak AlertCoordinator* weakSelf = self;

  self.rawCancelAction = cancelAction;

  _cancelAction = [^{
    AlertCoordinator* strongSelf = weakSelf;
    [strongSelf setNoInteractionAction:nil];
    if ([strongSelf rawCancelAction]) {
      [strongSelf rawCancelAction]();
    }
  } copy];
}

#pragma mark - Private Methods.

- (void)alertDismissed {
  self.visible = NO;
  _cancelButtonAdded = NO;
  _alertController = nil;
  _cancelAction = nil;
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
