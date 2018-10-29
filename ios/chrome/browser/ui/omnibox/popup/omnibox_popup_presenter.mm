// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_presenter.h"

#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_positioner.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_configuration.h"
#import "ios/chrome/browser/ui/toolbar/public/features.h"
#import "ios/chrome/browser/ui/util/named_guide.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/common/ui_util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_theme_resources.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kExpandAnimationDuration = 0.1;
const CGFloat kCollapseAnimationDuration = 0.05;
const CGFloat kVerticalOffset = 6;
}  // namespace

@interface OmniboxPopupPresenter ()
// Constraint for the bottom anchor of the popup.
@property(nonatomic, strong) NSLayoutConstraint* bottomConstraint;

@property(nonatomic, weak) id<OmniboxPopupPositioner> positioner;
@property(nonatomic, weak) UIViewController* viewController;
@property(nonatomic, strong) UIView* popupContainerView;
@end

@implementation OmniboxPopupPresenter
@synthesize viewController = _viewController;
@synthesize positioner = _positioner;
@synthesize popupContainerView = _popupContainerView;
@synthesize bottomConstraint = _bottomConstraint;

- (instancetype)initWithPopupPositioner:(id<OmniboxPopupPositioner>)positioner
                    popupViewController:(UIViewController*)viewController
                              incognito:(BOOL)incognito {
  self = [super init];
  if (self) {
    _positioner = positioner;
    _viewController = viewController;

    // Popup uses same colors as the toolbar, so the ToolbarConfiguration is
    // used to get the style.
    ToolbarConfiguration* configuration = [[ToolbarConfiguration alloc]
        initWithStyle:incognito ? INCOGNITO : NORMAL];

    UIBlurEffect* effect = [configuration blurEffect];

    if (effect) {
      UIVisualEffectView* effectView =
          [[UIVisualEffectView alloc] initWithEffect:effect];
      [effectView.contentView addSubview:viewController.view];
      _popupContainerView = effectView;

    } else {
      UIView* containerView = [[UIView alloc] init];
      [containerView addSubview:viewController.view];
      _popupContainerView = containerView;
    }
    _popupContainerView.backgroundColor = [configuration blurBackgroundColor];
    _popupContainerView.translatesAutoresizingMaskIntoConstraints = NO;
    viewController.view.translatesAutoresizingMaskIntoConstraints = NO;
    AddSameConstraints(viewController.view, _popupContainerView);
  }
  return self;
}

- (void)updateHeightAndAnimateAppearanceIfNecessary {
  UIView* popup = self.popupContainerView;
  if (!popup.superview) {
    UIViewController* parentVC = [self.positioner popupParentViewController];
    [parentVC addChildViewController:self.viewController];
    [[self.positioner popupParentView] addSubview:popup];
    [self.viewController didMoveToParentViewController:parentVC];

    [self initialLayout];
  }

  if (!IsIPadIdiom()) {
    self.bottomConstraint.active = YES;
  }

  if (popup.bounds.size.height == 0) {
    // Animate if it expanding.
    [UIView animateWithDuration:kExpandAnimationDuration
                          delay:0
                        options:UIViewAnimationOptionCurveEaseInOut
                     animations:^{
                       [[popup superview] layoutIfNeeded];
                     }
                     completion:nil];
  }
}

- (void)animateCollapse {
  UIView* retainedPopupView = self.popupContainerView;
  UIViewController* retainedViewController = self.viewController;
  if (!IsIPadIdiom()) {
    self.bottomConstraint.active = NO;
  }

  [UIView animateWithDuration:kCollapseAnimationDuration
      delay:0
      options:UIViewAnimationOptionCurveEaseInOut
      animations:^{
        [[self.popupContainerView superview] layoutIfNeeded];
      }
      completion:^(BOOL) {
        [retainedViewController willMoveToParentViewController:nil];
        [retainedPopupView removeFromSuperview];
        [retainedViewController removeFromParentViewController];
      }];
}

#pragma mark - Private

// Layouts the popup when it is just added to the view hierarchy.
- (void)initialLayout {
  UIView* popup = self.popupContainerView;
  // Creates the constraints if the view is newly added to the view hierarchy.
  // On iPad the height of the popup is fixed.

  // This constraint will only be activated on iPhone as the popup is taking
  // the full height.
  self.bottomConstraint = [popup.bottomAnchor
      constraintEqualToAnchor:[popup superview].bottomAnchor];

  // Position the top anchor of the popup relatively to the layout guide
  // positioned on the omnibox.
  UILayoutGuide* topLayout =
      [NamedGuide guideWithName:kOmniboxGuide view:popup];
  NSLayoutConstraint* topConstraint =
      [popup.topAnchor constraintEqualToAnchor:topLayout.bottomAnchor];
  topConstraint.constant = kVerticalOffset;

  [NSLayoutConstraint activateConstraints:@[
    [popup.leadingAnchor constraintEqualToAnchor:popup.superview.leadingAnchor],
    [popup.trailingAnchor
        constraintEqualToAnchor:popup.superview.trailingAnchor],
    topConstraint,
  ]];

  [popup layoutIfNeeded];
  [[popup superview] layoutIfNeeded];
}

@end
