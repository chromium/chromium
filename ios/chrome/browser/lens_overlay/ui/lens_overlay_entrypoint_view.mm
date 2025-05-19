// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_entrypoint_view.h"

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/lens_overlay/coordinator/lens_overlay_availability.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

const CGFloat kLensCameraSymbolPointSize = 18.0;
const CGFloat kMinimumWidth = 44;

// The size of the visibility indicator in points.
const CGFloat kVisibilityIndicatorSize = 30.0;

}  // namespace

@implementation LensOverlayEntrypointButton {
  raw_ptr<const PrefService> _profilePrefs;

  // Indicates whether the feature is currently active and visible.
  UIView* _visibilityIndicatorView;
}

- (instancetype)initWithProfilePrefs:(const PrefService*)profilePrefs {
  self = [super init];

  if (self) {
    _profilePrefs = profilePrefs;
    self.pointerInteractionEnabled = YES;
    self.minimumDiameter = kMinimumWidth;
    self.pointerStyleProvider = CreateDefaultEffectCirclePointerStyleProvider();
    self.tintColor = [UIColor colorNamed:kToolbarButtonColor];
    self.accessibilityLabel = l10n_util::GetNSString(
        IDS_IOS_LENS_OVERLAY_ENTRYPOINT_BUTTON_ACCESSIBILITY_LABEL);

    UIImageSymbolConfiguration* symbolConfig = [UIImageSymbolConfiguration
        configurationWithPointSize:kLensCameraSymbolPointSize
                            weight:UIImageSymbolWeightRegular
                             scale:UIImageSymbolScaleMedium];
    [self setPreferredSymbolConfiguration:symbolConfig
                          forImageInState:UIControlStateNormal];

    [self setImage:CustomSymbolWithPointSize(kCameraLensSymbol,
                                             kLensCameraSymbolPointSize)
          forState:UIControlStateNormal];
    self.imageView.contentMode = UIViewContentModeScaleAspectFit;

    [NSLayoutConstraint activateConstraints:@[
      [self.widthAnchor constraintGreaterThanOrEqualToConstant:kMinimumWidth]
    ]];

    if (@available(iOS 17, *)) {
      __weak __typeof(self) weakSelf = self;
      NSArray<UITrait>* traits = TraitCollectionSetForTraits(@[
        UITraitHorizontalSizeClass.class, UITraitVerticalSizeClass.class
      ]);

      [self registerForTraitChanges:traits
                        withHandler:^(id<UITraitEnvironment> traitEnvironment,
                                      UITraitCollection* previousCollection) {
                          [weakSelf setEnabledOnTraitChange:previousCollection];
                        }];
    }
  }

  return self;
}

#if !defined(__IPHONE_17_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_17_0
- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  [self setEnabledOnTraitChange:previousTraitCollection];
}
#endif

- (void)setLensOverlayActive:(BOOL)active {
  if (active) {
    if ([_visibilityIndicatorView isDescendantOfView:self]) {
      return;
    }
    _visibilityIndicatorView = [[UIView alloc] init];
    _visibilityIndicatorView.translatesAutoresizingMaskIntoConstraints = NO;
    _visibilityIndicatorView.backgroundColor =
        [UIColor colorNamed:kGrey300Color];
    [self insertSubview:_visibilityIndicatorView belowSubview:self.imageView];
    _visibilityIndicatorView.layer.cornerRadius = kVisibilityIndicatorSize / 2;
    _visibilityIndicatorView.userInteractionEnabled = NO;
    AddSameCenterConstraints(self, _visibilityIndicatorView);
    AddSizeConstraints(
        _visibilityIndicatorView,
        CGSizeMake(kVisibilityIndicatorSize, kVisibilityIndicatorSize));

    self.accessibilityLabel = l10n_util::GetNSString(
        IDS_IOS_LENS_OVERLAY_ENTRYPOINT_BUTTON_STOP_ACCESSIBILITY_LABEL);
  } else {
    [_visibilityIndicatorView removeFromSuperview];
    _visibilityIndicatorView = nil;
    self.accessibilityLabel = l10n_util::GetNSString(
        IDS_IOS_LENS_OVERLAY_ENTRYPOINT_BUTTON_ACCESSIBILITY_LABEL);
  }
}

#pragma mark - private

- (void)setEnabledOnTraitChange:(UITraitCollection*)previousTraitCollection {
  if (IsLensOverlayLandscapeOrientationEnabled(_profilePrefs)) {
    return;
  }

  if (self.traitCollection.verticalSizeClass !=
          previousTraitCollection.verticalSizeClass ||
      self.traitCollection.horizontalSizeClass !=
          previousTraitCollection.horizontalSizeClass) {
    self.enabled = !IsCompactHeight(self.traitCollection);
  }
}

@end
