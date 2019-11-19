// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_presenter.h"

#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_configuration.h"
#import "ios/chrome/browser/ui/toolbar/public/features.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#import "ios/chrome/browser/ui/util/named_guide.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#include "ios/chrome/common/ui_util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_theme_resources.h"
#import "ui/gfx/ios/uikit_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kVerticalOffset = 6;
}  // namespace

@interface OmniboxPopupPresenter ()
// Constraint for the bottom anchor of the popup.
@property(nonatomic, strong) NSLayoutConstraint* bottomConstraint;

@property(nonatomic, weak) id<OmniboxPopupPresenterDelegate> delegate;
@property(nonatomic, weak) UIViewController* viewController;
@property(nonatomic, strong) UIView* popupContainerView;
// Separator for the bottom edge of the popup on iPad.
@property(nonatomic, strong) UIView* bottomSeparator;

@end

@implementation OmniboxPopupPresenter

- (instancetype)initWithPopupPresenterDelegate:
                    (id<OmniboxPopupPresenterDelegate>)delegate
                           popupViewController:(UIViewController*)viewController
                                     incognito:(BOOL)incognito {
  self = [super init];
  if (self) {
    _delegate = delegate;
    _viewController = viewController;

    // Popup uses same colors as the toolbar, so the ToolbarConfiguration is
    // used to get the style.
    ToolbarConfiguration* configuration = [[ToolbarConfiguration alloc]
        initWithStyle:incognito ? INCOGNITO : NORMAL];

    UIView* containerView = [[UIView alloc] init];
    [containerView addSubview:viewController.view];
    _popupContainerView = containerView;
    if (@available(iOS 13, *)) {
      UIUserInterfaceStyle userInterfaceStyle =
          incognito ? UIUserInterfaceStyleDark
                    : UIUserInterfaceStyleUnspecified;
      // Both the container view and the popup view controller need
      // overrideUserInterfaceStyle set because the overall popup background
      // comes from the container, but overrideUserInterfaceStyle won't
      // propagate from a view to any subviews in a different view controller.
      _popupContainerView.overrideUserInterfaceStyle = userInterfaceStyle;
      viewController.overrideUserInterfaceStyle = userInterfaceStyle;
    }
    _popupContainerView.backgroundColor = [configuration backgroundColor];
    _popupContainerView.translatesAutoresizingMaskIntoConstraints = NO;
    viewController.view.translatesAutoresizingMaskIntoConstraints = NO;
    AddSameConstraints(viewController.view, _popupContainerView);

    // Add bottom separator. This will only be visible on iPad where
    // the omnibox doesn't fill the whole screen.
    _bottomSeparator = [[UIView alloc] initWithFrame:CGRectZero];
    _bottomSeparator.translatesAutoresizingMaskIntoConstraints = NO;
    _bottomSeparator.backgroundColor = [UIColor colorNamed:kToolbarShadowColor];

    [_popupContainerView addSubview:self.bottomSeparator];
    CGFloat separatorHeight =
        ui::AlignValueToUpperPixel(kToolbarSeparatorHeight);
    [NSLayoutConstraint activateConstraints:@[
      [self.bottomSeparator.heightAnchor
          constraintEqualToConstant:separatorHeight],
      [self.bottomSeparator.leadingAnchor
          constraintEqualToAnchor:_popupContainerView.leadingAnchor],
      [self.bottomSeparator.trailingAnchor
          constraintEqualToAnchor:_popupContainerView.trailingAnchor],
      [self.bottomSeparator.topAnchor
          constraintEqualToAnchor:_popupContainerView.bottomAnchor],
    ]];
  }
  return self;
}

- (void)updatePopup {
  BOOL popupHeightIsZero =
      self.viewController.view.intrinsicContentSize.height == 0;
  BOOL popupIsOnscreen = self.popupContainerView.superview != nil;
  if (popupHeightIsZero && popupIsOnscreen) {
    // If intrinsic size is 0 and popup is onscreen, we want to remove the
    // popup view.
    if (!IsIPadIdiom()) {
      self.bottomConstraint.active = NO;
      self.bottomSeparator.hidden = YES;
    }

    [self.viewController willMoveToParentViewController:nil];
    [self.popupContainerView removeFromSuperview];
    [self.viewController removeFromParentViewController];

    self.open = NO;
    [self.delegate popupDidCloseForPresenter:self];
  } else if (!popupHeightIsZero && !popupIsOnscreen) {
    // If intrinsic size is nonzero and popup is offscreen, we want to add it.
    UIViewController* parentVC =
        [self.delegate popupParentViewControllerForPresenter:self];
    [parentVC addChildViewController:self.viewController];
    [[self.delegate popupParentViewForPresenter:self]
        addSubview:self.popupContainerView];
    [self.viewController didMoveToParentViewController:parentVC];

    [self initialLayout];

    if (!IsIPadIdiom()) {
      self.bottomConstraint.active = YES;
      self.bottomSeparator.hidden = NO;
    }

    self.open = YES;
    [self.delegate popupDidOpenForPresenter:self];
  }
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

  [[popup superview] layoutIfNeeded];
}

@end
