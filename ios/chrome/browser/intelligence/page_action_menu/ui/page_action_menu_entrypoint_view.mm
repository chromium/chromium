// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/page_action_menu/ui/page_action_menu_entrypoint_view.h"

#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// The point size of the entry point's symbol.
const CGFloat kIconPointSize = 18.0;

// The width of the extended button's tappable area.
const CGFloat kMinimumWidth = 44;

// The width of the background view.
const CGFloat kBackgroundWidth = 26;

// Scale factor for highlight state.
const CGFloat kHighlightScaling = 0.7;

}  // namespace

@implementation PageActionMenuEntrypointView {
  // Button's background subview.
  UIView* _backgroundView;
}

- (instancetype)init {
  self = [super init];

  if (self) {
    self.pointerInteractionEnabled = YES;
    self.minimumDiameter = kMinimumWidth;
    self.pointerStyleProvider = CreateDefaultEffectCirclePointerStyleProvider();
    self.tintColor = [UIColor colorNamed:kToolbarButtonColor];

    self.accessibilityLabel = l10n_util::GetNSString(
        IDS_IOS_BWG_PAGE_ACTION_MENU_ENTRY_POINT_ACCESSIBILITY_LABEL);

    UIImageSymbolConfiguration* symbolConfig = [UIImageSymbolConfiguration
        configurationWithPointSize:kIconPointSize
                            weight:UIImageSymbolWeightRegular
                             scale:UIImageSymbolScaleMedium];
    [self setPreferredSymbolConfiguration:symbolConfig
                          forImageInState:UIControlStateNormal];
    if (IsDirectBWGEntryPoint()) {
      self.accessibilityLabel =
          l10n_util::GetNSString(IDS_IOS_BWG_ASK_GEMINI_ACCESSIBILITY_LABEL);
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
      [self setImage:CustomSymbolWithPointSize(kGeminiBrandedLogoImage,
                                               kIconPointSize)
            forState:UIControlStateNormal];
#else
      [self setImage:DefaultSymbolWithPointSize(kGeminiNonBrandedLogoImage,
                                                kIconPointSize)
            forState:UIControlStateNormal];
#endif
    } else {
      [self setImage:CustomSymbolWithPointSize(kTextSparkSymbol, kIconPointSize)
            forState:UIControlStateNormal];
    }
    self.imageView.contentMode = UIViewContentModeScaleAspectFit;
    [self createBackgroundView];

    [NSLayoutConstraint activateConstraints:@[
      [self.widthAnchor constraintGreaterThanOrEqualToConstant:kMinimumWidth],
      [_backgroundView.widthAnchor constraintEqualToConstant:kBackgroundWidth],
      [_backgroundView.heightAnchor constraintEqualToConstant:kBackgroundWidth],
      [_backgroundView.centerXAnchor
          constraintEqualToAnchor:self.centerXAnchor],
      [_backgroundView.centerYAnchor
          constraintEqualToAnchor:self.centerYAnchor],
    ]];
  }
  return self;
}

#pragma mark - PageActionMenuEntryPointCommands

- (void)toggleEntryPointHighlight:(BOOL)highlight {
  NSTimeInterval animationDuration = 0.3;
  __weak __typeof(self) weakSelf = self;
  [UIView animateWithDuration:animationDuration
                   animations:^{
                     [weakSelf updateHighlightState:highlight];
                   }];
}

#pragma mark - Private

// Updates properties related to highlighting the button.
- (void)updateHighlightState:(BOOL)shouldHighlight {
  if (shouldHighlight) {
    self.imageView.transform =
        CGAffineTransformMakeScale(kHighlightScaling, kHighlightScaling);
    self.tintColor = [UIColor colorNamed:kSolidWhiteColor];
    _backgroundView.hidden = NO;
  } else {
    self.imageView.transform = CGAffineTransformIdentity;
    self.tintColor = [UIColor colorNamed:kToolbarButtonColor];
    _backgroundView.hidden = YES;
  }
}

// Creates button's background view.
- (void)createBackgroundView {
  _backgroundView = [[UIView alloc] init];
  _backgroundView.layer.cornerRadius = kBackgroundWidth / 2;
  _backgroundView.backgroundColor = [UIColor colorNamed:kBlueColor];
  _backgroundView.clipsToBounds = YES;
  _backgroundView.hidden = YES;
  _backgroundView.userInteractionEnabled = NO;
  _backgroundView.translatesAutoresizingMaskIntoConstraints = NO;
  [self insertSubview:_backgroundView atIndex:0];
}

@end
