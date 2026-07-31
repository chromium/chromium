// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/ui/actor_overlay_view_controller.h"

#import "base/check.h"
#import "ios/chrome/browser/intelligence/actor/ui/actor_overlay_scrim_view.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ui/base/device_form_factor.h"

@implementation ActorOverlayViewController {
  // The layout guide center for the browser.
  __weak LayoutGuideCenter* _browserLayoutGuideCenter;
  // The base color of the overlay UI components.
  UIColor* _overlayColor;
}

- (instancetype)initWithBrowserLayoutGuideCenter:
                    (LayoutGuideCenter*)browserCenter
                                    overlayColor:(UIColor*)overlayColor {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    CHECK(browserCenter);
    _browserLayoutGuideCenter = browserCenter;
    _overlayColor = overlayColor;
  }
  return self;
}

#pragma mark - UIViewController

- (void)loadView {
  self.view = [[ActorOverlayScrimView alloc] initWithScrimColor:_overlayColor];
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (!parent) {
    return;
  }
  UIView* view = self.view;
  UIView* superview = view.superview;
  if (!superview) {
    return;
  }

  NSLayoutYAxisAnchor* topAnchor = superview.topAnchor;
  // On iPad (tablet form factor), the UX explicitly anchors the overlay to
  // the bottom of the primary toolbar.
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    UILayoutGuide* primaryToolbarGuide =
        [_browserLayoutGuideCenter makeLayoutGuideNamed:kPrimaryToolbarGuide];
    if (primaryToolbarGuide) {
      [superview addLayoutGuide:primaryToolbarGuide];
      topAnchor = primaryToolbarGuide.bottomAnchor;
    }
  }

  [NSLayoutConstraint activateConstraints:@[
    [view.topAnchor constraintEqualToAnchor:topAnchor],
    [view.bottomAnchor constraintEqualToAnchor:superview.bottomAnchor],
    [view.leadingAnchor constraintEqualToAnchor:superview.leadingAnchor],
    [view.trailingAnchor constraintEqualToAnchor:superview.trailingAnchor],
  ]];
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  [self.view.window endEditing:YES];
}

@end
