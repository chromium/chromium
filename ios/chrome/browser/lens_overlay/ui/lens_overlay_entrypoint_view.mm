// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_entrypoint_view.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

const CGFloat kLensCameraSymbolPointSize = 18.0;

}  // namespace

@implementation LensOverlayEntrypointButton

- (instancetype)init {
  self = [super init];

  if (self) {
    self.pointerInteractionEnabled = YES;
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

    [NSLayoutConstraint
        activateConstraints:@[ [self.widthAnchor
                                constraintEqualToAnchor:self.heightAnchor] ]];

    if (@available(iOS 17, *)) {
      __weak __typeof(self) weakSelf = self;
      NSArray<UITrait>* traits = TraitCollectionSetForTraits(
          @[ UITraitHorizontalSizeClass.self, UITraitVerticalSizeClass.self ]);

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

#pragma mark - private

- (void)setEnabledOnTraitChange:(UITraitCollection*)previousTraitCollection {
  if (self.traitCollection.verticalSizeClass !=
          previousTraitCollection.verticalSizeClass ||
      self.traitCollection.horizontalSizeClass !=
          previousTraitCollection.horizontalSizeClass) {
    self.enabled = !IsCompactHeight(self.traitCollection);
  }
}

@end
