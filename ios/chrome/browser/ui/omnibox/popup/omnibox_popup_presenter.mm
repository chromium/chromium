// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_presenter.h"

#import "base/time/time.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_ui_features.h"
#import "ios/chrome/browser/ui/omnibox/popup/content_providing.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_container_view.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_configuration.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_theme_resources.h"
#import "ui/base/device_form_factor.h"
#import "ui/gfx/ios/uikit_util.h"

namespace {
const CGFloat kVerticalOffset = 6;
const CGFloat kPopupBottomPaddingTablet = 80;

/// Duration of the fade in animation.
constexpr NSTimeInterval kFadeInAnimationDuration =
    base::Milliseconds(300).InSecondsF();
/// Vertical offset of the suggestions when fading in.
const CGFloat kFadeAnimationVerticalOffset = 12;

}  // namespace

@interface OmniboxPopupPresenter ()
/// Constraint for the bottom anchor of the popup when form factor is phone.
@property(nonatomic, strong) NSLayoutConstraint* bottomConstraintPhone;
/// Constraint for the bottom anchor of the popup when form factor is tablet.
@property(nonatomic, strong) NSLayoutConstraint* bottomConstraintTablet;

@property(nonatomic, weak) id<OmniboxPopupPresenterDelegate> delegate;
@property(nonatomic, weak) UIViewController<ContentProviding>* viewController;
@property(nonatomic, strong) UIView* popupContainerView;
/// Separator for the bottom edge of the popup on iPad.
@property(nonatomic, strong) UIView* bottomSeparator;
/// Top constraint between the popup and it's container. This is used to animate
/// suggestions when focusing the omnibox.
@property(nonatomic, strong) NSLayoutConstraint* popupTopConstraint;

// The layout guide center to use to refer to the omnibox.
@property(nonatomic, strong) LayoutGuideCenter* layoutGuideCenter;

@end

@implementation OmniboxPopupPresenter {
  /// Type of the toolbar that contains the omnibox when it's not focused. The
  /// animation of focusing/defocusing the omnibox changes depending on this
  /// position.
  ToolbarType _unfocusedOmniboxToolbarType;
}

- (instancetype)
    initWithPopupPresenterDelegate:(id<OmniboxPopupPresenterDelegate>)delegate
               popupViewController:
                   (UIViewController<ContentProviding>*)viewController
                 layoutGuideCenter:(LayoutGuideCenter*)layoutGuideCenter
                         incognito:(BOOL)incognito {
  self = [super init];
  if (self) {
    _delegate = delegate;
    _viewController = viewController;
    _layoutGuideCenter = layoutGuideCenter;

    // Popup uses same colors as the toolbar, so the ToolbarConfiguration is
    // used to get the style.
    ToolbarConfiguration* configuration = [[ToolbarConfiguration alloc]
        initWithStyle:incognito ? ToolbarStyle::kIncognito
                                : ToolbarStyle::kNormal];

    UIView* containerView = [[OmniboxPopupContainerView alloc] init];
    [containerView addSubview:viewController.view];
    _popupContainerView = containerView;

    UIUserInterfaceStyle userInterfaceStyle =
        incognito ? UIUserInterfaceStyleDark : UIUserInterfaceStyleUnspecified;
    // Both the container view and the popup view controller need
    // overrideUserInterfaceStyle set because the overall popup background
    // comes from the container, but overrideUserInterfaceStyle won't
    // propagate from a view to any subviews in a different view controller.
    _popupContainerView.overrideUserInterfaceStyle = userInterfaceStyle;
    viewController.overrideUserInterfaceStyle = userInterfaceStyle;

    _popupContainerView.backgroundColor = [configuration backgroundColor];

    _popupContainerView.translatesAutoresizingMaskIntoConstraints = NO;
    viewController.view.translatesAutoresizingMaskIntoConstraints = NO;

    if (IsIpadPopoutOmniboxEnabled()) {
      _popupContainerView.clipsToBounds = YES;
      _popupContainerView.layer.cornerRadius = 11.0f;

      UIColor* borderColor =
          incognito ? [UIColor.whiteColor colorWithAlphaComponent:0.12]
                    : [UIColor.blackColor colorWithAlphaComponent:0.12];

      _popupContainerView.layer.borderColor = borderColor.CGColor;
      _popupContainerView.layer.borderWidth = 2.0f;
      AddSameConstraints(viewController.view, _popupContainerView);
    } else {
      AddSameConstraintsToSides(viewController.view, _popupContainerView,
                                LayoutSides::kLeading | LayoutSides::kTrailing |
                                    LayoutSides::kBottom);
      _popupTopConstraint = [viewController.view.topAnchor
          constraintEqualToAnchor:_popupContainerView.topAnchor];
      _popupTopConstraint.active = YES;
    }

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

- (void)updatePopupOnFocus:(BOOL)isFocusingOmnibox {
  BOOL popupHasContent = self.viewController.hasContent;
  BOOL popupIsOnscreen = self.popupContainerView.superview != nil;
  if (!popupHasContent && popupIsOnscreen) {
    // If intrinsic size is 0 and popup is onscreen, we want to remove the
    // popup view.
    if (ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_TABLET) {
      self.bottomConstraintPhone.active = NO;
      self.bottomSeparator.hidden = YES;
    } else if (!IsIpadPopoutOmniboxEnabled()) {
      self.bottomConstraintTablet.active = NO;
    }

    [self.viewController willMoveToParentViewController:nil];
    [self.popupContainerView removeFromSuperview];
    [self.viewController removeFromParentViewController];

    self.open = NO;
    [self.delegate popupDidCloseForPresenter:self];
  } else if (popupHasContent && !popupIsOnscreen) {
    // If intrinsic size is nonzero and popup is offscreen, we want to add it.
    UIViewController* parentVC =
        [self.delegate popupParentViewControllerForPresenter:self];
    [parentVC addChildViewController:self.viewController];
    [[self.delegate popupParentViewForPresenter:self]
        addSubview:self.popupContainerView];
    [self.viewController didMoveToParentViewController:parentVC];

    BOOL enableFocusAnimation =
        IsBottomOmniboxSteadyStateEnabled() && isFocusingOmnibox &&
        _unfocusedOmniboxToolbarType == ToolbarType::kSecondary;

    [self initialLayoutAnimated:enableFocusAnimation];

    if (ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_TABLET) {
      self.bottomConstraintPhone.active = YES;
      self.bottomSeparator.hidden = NO;
    } else if (!IsIpadPopoutOmniboxEnabled()) {
      self.bottomConstraintTablet.active = YES;
    }

    self.open = YES;
    [self.delegate popupDidOpenForPresenter:self];
  }
}

/// With popout omnibox, the popup might be in either of two states:
/// a) regular x regular state, where the popup matches OB width
/// b) compact state, where popup takes whole screen width
/// Therefore, on trait collection change, re-add the popup and recreate the
/// constraints to make sure the correct ones are used.
- (void)updatePopupAfterTraitCollectionChange {
  DCHECK(IsIpadPopoutOmniboxEnabled());

  if (!self.open) {
    return;
  }

  // Re-add the popup container to break any existing constraints.
  [self.popupContainerView removeFromSuperview];
  [[self.delegate popupParentViewForPresenter:self]
      addSubview:self.popupContainerView];

  // Re-add necessary constraints.
  [self initialLayoutAnimated:NO];

  if (ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_TABLET) {
    self.bottomConstraintPhone.active = YES;
    self.bottomSeparator.hidden = NO;
  }
}

#pragma mark - ToolbarOmniboxConsumer

- (void)steadyStateOmniboxMovedToToolbar:(ToolbarType)toolbarType {
  _unfocusedOmniboxToolbarType = toolbarType;
}

#pragma mark - Private

/// Layouts the popup when it is just added to the view hierarchy.
- (void)initialLayoutAnimated:(BOOL)isAnimated {
  UIView* popup = self.popupContainerView;
  // Creates the constraints if the view is newly added to the view hierarchy.

  // On phone form factor the popup is taking the full height.
  self.bottomConstraintPhone =
      [popup.bottomAnchor constraintEqualToAnchor:popup.superview.bottomAnchor];
  // On tablet form factor the popup is padded on the bottom to allow the user
  // to defocus the omnibox.
  self.bottomConstraintTablet = [popup.superview.bottomAnchor
      constraintGreaterThanOrEqualToAnchor:popup.bottomAnchor
                                  constant:kPopupBottomPaddingTablet];

  // Install in the superview the guide tracking the top omnibox.
  UILayoutGuide* topOmniboxGuide =
      [self.layoutGuideCenter makeLayoutGuideNamed:kTopOmniboxGuide];
  [[popup superview] addLayoutGuide:topOmniboxGuide];
  // Position the top anchor of the popup relatively to that layout guide.
  NSLayoutConstraint* topConstraint =
      [popup.topAnchor constraintEqualToAnchor:topOmniboxGuide.bottomAnchor
                                      constant:kVerticalOffset];

  NSMutableArray<NSLayoutConstraint*>* constraintsToActivate =
      [NSMutableArray arrayWithObject:topConstraint];

  if (IsIpadPopoutOmniboxEnabled() &&
      IsRegularXRegularSizeClass(self.popupContainerView)) {
    [constraintsToActivate addObjectsFromArray:@[
      [popup.leadingAnchor
          constraintEqualToAnchor:topOmniboxGuide.leadingAnchor],
      [popup.trailingAnchor
          constraintEqualToAnchor:topOmniboxGuide.trailingAnchor],
    ]];
  } else {
    [constraintsToActivate addObjectsFromArray:@[
      [popup.leadingAnchor
          constraintEqualToAnchor:popup.superview.leadingAnchor],
      [popup.trailingAnchor
          constraintEqualToAnchor:popup.superview.trailingAnchor],
    ]];
  }

  [NSLayoutConstraint activateConstraints:constraintsToActivate];

  [[popup superview] layoutIfNeeded];

  if (isAnimated) {
    [self animatePopupOnOmniboxFocus];
  }
}

/// Animates the popup for omnibox focus.
- (void)animatePopupOnOmniboxFocus {
  CHECK(IsBottomOmniboxSteadyStateEnabled());
  __weak __typeof__(self) weakSelf = self;
  self.viewController.view.alpha = 0.0;
  self.popupTopConstraint.constant = kFadeAnimationVerticalOffset;
  [self.popupContainerView.superview layoutIfNeeded];

  auto constraintForVisiblePopup = ^{
    weakSelf.viewController.view.alpha = 1.0;
    weakSelf.popupTopConstraint.constant = 0.0;
    [weakSelf.popupContainerView.superview layoutIfNeeded];
  };

  [UIView animateWithDuration:kFadeInAnimationDuration
                   animations:constraintForVisiblePopup
                   completion:^(BOOL _) {
                     constraintForVisiblePopup();
                   }];
}

@end
