// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/coordinator/alert/action_sheet_coordinator.h"

#import "ui/base/l10n/l10n_util.h"
#import "ui/strings/grit/ui_strings.h"

namespace {

enum class AnchorMode {
  VIEW,
  BAR_BUTTON_ITEM,
};

}  // namespace

@interface ActionSheetCoordinator () {
  // The anchor mode for the popover alert.
  AnchorMode _anchorMode;

  // Rectangle for the popover alert. Only used when `_anchorMode` is VIEW.
  CGRect _rect;
  // View for the popovert alert. Only used when `_anchorMode` is VIEW.
  __weak UIView* _view;

  // Bar button item for the popover alert.  Only used when `_anchorMode` is
  // BAR_BUTTON_ITEM.
  UIBarButtonItem* _barButtonItem;
}

@end

@implementation ActionSheetCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                     title:(NSString*)title
                                   message:(NSString*)message
                                      rect:(CGRect)rect
                                      view:(UIView*)view {
  self = [super initWithBaseViewController:viewController
                                   browser:browser
                                     title:title
                                   message:message];
  if (self) {
    _anchorMode = AnchorMode::VIEW;
    _rect = rect;
    _view = view;
    _popoverArrowDirection = UIPopoverArrowDirectionAny;
    _alertStyle = UIAlertControllerStyleActionSheet;
  }
  return self;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                     title:(NSString*)title
                                   message:(NSString*)message
                             barButtonItem:(UIBarButtonItem*)barButtonItem {
  self = [super initWithBaseViewController:viewController
                                   browser:browser
                                     title:title
                                   message:message];
  if (self) {
    _anchorMode = AnchorMode::BAR_BUTTON_ITEM;
    _barButtonItem = barButtonItem;
    _popoverArrowDirection = UIPopoverArrowDirectionAny;
    _alertStyle = UIAlertControllerStyleActionSheet;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  if (!self.cancelButtonAdded) {
    [self addItemWithTitle:l10n_util::GetNSString(IDS_APP_CANCEL)
                    action:nil
                     style:UIAlertActionStyleCancel];
  }
  [super start];
}

#pragma mark - AlertCoordinator

- (UIAlertController*)alertControllerWithTitle:(NSString*)title
                                       message:(NSString*)message {
  UIAlertController* alert =
      [UIAlertController alertControllerWithTitle:title
                                          message:message
                                   preferredStyle:_alertStyle];
  alert.popoverPresentationController.permittedArrowDirections =
      _popoverArrowDirection;

  switch (_anchorMode) {
    case AnchorMode::VIEW:
      alert.popoverPresentationController.sourceView = _view;
      alert.popoverPresentationController.sourceRect = _rect;
      break;
    case AnchorMode::BAR_BUTTON_ITEM:
      alert.popoverPresentationController.barButtonItem = _barButtonItem;
      break;
  }

  return alert;
}

#pragma mark - Public Methods

- (void)updateAttributedText {
  // Use setValue to access unexposed attributed strings for title and message.
  if (self.attributedTitle) {
    [self.alertController setValue:_attributedTitle forKey:@"attributedTitle"];
  }
  if (self.attributedMessage) {
    [self.alertController setValue:_attributedMessage
                            forKey:@"attributedMessage"];
  }
}

@end
