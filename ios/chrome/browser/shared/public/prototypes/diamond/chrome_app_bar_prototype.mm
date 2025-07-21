// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/public/prototypes/diamond/chrome_app_bar_prototype.h"

#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/prototypes/diamond/utils.h"
#import "ios/chrome/browser/shared/ui/symbols/buildflags.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/gradient_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Size of the symbols.
const CGFloat kSymbolSize = 22;

// Height of the "gap" in the mask of the browser background. This needs to be
// twice the corner radius because the
// bezierPathWithRoundedRect:byRoundingCorners:cornerRadii: method requires the
// height to be at least twice the corner radius.
CGFloat BrowserBackgroundGapHeight() {
  return 2 * kDiamondBrowserCornerRadius;
}

// Duration of the animation.
const CGFloat kBackgroundTransitionTime = 0.25;

UIButtonConfiguration* ButtonConfiguration() {
  UIButtonConfiguration* configuration =
      [UIButtonConfiguration plainButtonConfiguration];
  configuration.imagePlacement = NSDirectionalRectEdgeTop;
  configuration.imagePadding = 5;
  configuration.baseForegroundColor = UIColor.whiteColor;
  configuration.titleTextAttributesTransformer =
      ^NSDictionary<NSAttributedStringKey, id>*(
          NSDictionary<NSAttributedStringKey, id>* incoming) {
    NSMutableDictionary<NSAttributedStringKey, id>* outgoing =
        [incoming mutableCopy];
    outgoing[NSFontAttributeName] = [UIFont systemFontOfSize:11];
    return outgoing;
  };

  return configuration;
}

}  // namespace

@implementation ChromeAppBarPrototype {
  CAShapeLayer* _maskLayer;
  // Background when the tab grid is visible.
  UIVisualEffectView* _tabGridBackground;
  // Background when the browser is visible.
  UIVisualEffectView* _browserBackground;
  // The blur effect for the backgrounds.
  UIBlurEffect* _blurEffect;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    CHECK(IsDiamondPrototypeEnabled());

    _blurEffect =
        [UIBlurEffect effectWithStyle:UIBlurEffectStyleSystemThickMaterialDark];

    _tabGridBackground =
        [[UIVisualEffectView alloc] initWithEffect:_blurEffect];
    _tabGridBackground.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_tabGridBackground];
    AddSameConstraints(self, _tabGridBackground);

    _browserBackground = [[UIVisualEffectView alloc] initWithEffect:nil];
    _browserBackground.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_browserBackground];
    [NSLayoutConstraint activateConstraints:@[
      [_browserBackground.leadingAnchor
          constraintEqualToAnchor:self.leadingAnchor],
      [_browserBackground.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor],
      [_browserBackground.bottomAnchor
          constraintEqualToAnchor:self.bottomAnchor],
      [_browserBackground.topAnchor
          constraintEqualToAnchor:self.topAnchor
                         constant:-BrowserBackgroundGapHeight()],
    ]];

    _maskLayer = [CAShapeLayer layer];
    _maskLayer.fillRule = kCAFillRuleEvenOdd;
    _maskLayer.fillColor = [UIColor blackColor].CGColor;
    _browserBackground.layer.mask = _maskLayer;
    [self updateMask];

    UIButtonConfiguration* askGeminiConfiguration = ButtonConfiguration();
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
    askGeminiConfiguration.image =
        CustomSymbolWithPointSize(kGeminiBrandedLogoImage, kSymbolSize);
#else
    askGeminiConfiguration.image =
        DefaultSymbolWithPointSize(kGeminiNonBrandedLogoImage, kSymbolSize);
#endif  // BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)

    askGeminiConfiguration.title =
        l10n_util::GetNSString(IDS_IOS_DIAMOND_PROTOTYPE_ASK_GEMINI);
    _askGeminiButton = [UIButton buttonWithConfiguration:askGeminiConfiguration
                                           primaryAction:nil];
    _askGeminiButton.translatesAutoresizingMaskIntoConstraints = NO;

    UIButtonConfiguration* openNewTabConfiguration = ButtonConfiguration();
    openNewTabConfiguration.image =
        DefaultSymbolWithPointSize(kPlusInCircleSymbol, kSymbolSize);
    openNewTabConfiguration.title =
        l10n_util::GetNSString(IDS_IOS_DIAMOND_PROTOTYPE_NEW_TAB);
    _openNewTabButton =
        [UIButton buttonWithConfiguration:openNewTabConfiguration
                            primaryAction:nil];
    _openNewTabButton.translatesAutoresizingMaskIntoConstraints = NO;

    UIButtonConfiguration* tabGridConfiguration = ButtonConfiguration();
    // TODO(crbug.com/429955447): replace the symbol with a tab grid icon,
    // including number of tabs.
    tabGridConfiguration.image =
        DefaultSymbolWithPointSize(@"square", kSymbolSize);
    tabGridConfiguration.title =
        l10n_util::GetNSString(IDS_IOS_DIAMOND_PROTOTYPE_ALL_TABS);
    _tabGridButton = [UIButton buttonWithConfiguration:tabGridConfiguration
                                         primaryAction:nil];
    _tabGridButton.translatesAutoresizingMaskIntoConstraints = NO;

    UIStackView* stackView = [[UIStackView alloc] initWithArrangedSubviews:@[
      _askGeminiButton, _openNewTabButton, _tabGridButton
    ]];
    stackView.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:stackView];
    [stackView.heightAnchor
        constraintEqualToConstant:kChromeAppBarPrototypeHeight]
        .active = YES;
    AddSameConstraints(self.safeAreaLayoutGuide, stackView);

    NSNotificationCenter* notificationCenter =
        [NSNotificationCenter defaultCenter];
    [notificationCenter addObserver:self
                           selector:@selector(didEnterTabGrid)
                               name:kDiamondEnterTabGridNotification
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(didLeaveTabGrid)
                               name:kDiamondLeaveTabGridNotification
                             object:nil];
  }
  return self;
}

- (void)didMoveToSuperview {
  [super didMoveToSuperview];
  CHECK(IsDiamondPrototypeEnabled());
}

- (void)layoutSubviews {
  [super layoutSubviews];
  [self updateMask];
}

#pragma mark - Private

// Callback when exiting the grid.
- (void)didLeaveTabGrid {
  __weak UIVisualEffectView* browserBackground = _browserBackground;
  __weak UIVisualEffectView* tabGridBackground = _tabGridBackground;
  UIBlurEffect* blur = _blurEffect;
  [UIView animateWithDuration:kBackgroundTransitionTime
                   animations:^{
                     browserBackground.effect = blur;
                     tabGridBackground.effect = nil;
                   }];
}

// Callback when entering the grid.
- (void)didEnterTabGrid {
  __weak UIVisualEffectView* browserBackground = _browserBackground;
  __weak UIVisualEffectView* tabGridBackground = _tabGridBackground;
  UIBlurEffect* blur = _blurEffect;
  [UIView animateWithDuration:kBackgroundTransitionTime
                   animations:^{
                     browserBackground.effect = nil;
                     tabGridBackground.effect = blur;
                   }];
}

// Updates the mask of the background blur.
- (void)updateMask {
  CGRect maskFrame = CGRectMake(0, 0, _browserBackground.bounds.size.width,
                                BrowserBackgroundGapHeight());

  UIBezierPath* path =
      [UIBezierPath bezierPathWithRect:_browserBackground.bounds];
  UIBezierPath* cutoutPath = [UIBezierPath
      bezierPathWithRoundedRect:maskFrame
              byRoundingCorners:UIRectCornerBottomLeft | UIRectCornerBottomRight
                    cornerRadii:CGSizeMake(kDiamondBrowserCornerRadius,
                                           kDiamondBrowserCornerRadius)];
  [path appendPath:cutoutPath];

  _maskLayer.frame = maskFrame;
  _maskLayer.path = path.CGPath;
}

@end
