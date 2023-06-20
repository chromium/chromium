// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/secondary_toolbar_view_controller.h"

#import "base/check.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_utils.h"
#import "ios/chrome/browser/ui/toolbar/secondary_toolbar_view.h"
#import "ios/chrome/common/ui/util/ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SecondaryToolbarViewController ()

/// Redefined to be a `SecondaryToolbarView`.
@property(nonatomic, strong) SecondaryToolbarView* view;

@end

@implementation SecondaryToolbarViewController

@dynamic view;

- (void)loadView {
  self.view =
      [[SecondaryToolbarView alloc] initWithButtonFactory:self.buttonFactory];
  DCHECK(self.layoutGuideCenter);
  [self.layoutGuideCenter referenceView:self.view
                              underName:kSecondaryToolbarGuide];
}

#pragma mark - FullscreenUIElement

- (void)updateForFullscreenProgress:(CGFloat)progress {
  [super updateForFullscreenProgress:progress];

  CGFloat alphaValue = fmax(progress * 2 - 1, 0);
  if (IsBottomOmniboxSteadyStateEnabled()) {
    self.view.buttonStackView.alpha = alphaValue;
  }

  self.view.locationBarTopConstraint.constant =
      [self verticalMarginForLocationBarForFullscreenProgress:progress];
}

#pragma mark - Private

// Returns the vertical margin to the location bar based on fullscreen
// `progress`, aligned to the nearest pixel.
- (CGFloat)verticalMarginForLocationBarForFullscreenProgress:(CGFloat)progress {
  const CGFloat clampedFontSizeMultiplier = ToolbarClampedFontSizeMultiplier(
      self.traitCollection.preferredContentSizeCategory);

  return AlignValueToPixel(
      (kBottomAdaptiveLocationBarTopMargin * progress +
       kBottomAdaptiveLocationBarVerticalMarginFullscreen * (1 - progress)) *
          clampedFontSizeMultiplier +
      (clampedFontSizeMultiplier - 1) * kLocationBarVerticalMarginDynamicType);
}

@end
