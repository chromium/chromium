// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/chrome_app_bar_prototype.h"

#import "ios/chrome/browser/shared/public/features/diamond_prototype_utils.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/buildflags.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// Size of the symbols.
const CGFloat kSymbolSize = 22;

UIButtonConfiguration* ButtonConfiguration() {
  UIButtonConfiguration* configuration =
      [UIButtonConfiguration plainButtonConfiguration];
  configuration.imagePlacement = NSDirectionalRectEdgeTop;
  configuration.baseForegroundColor = UIColor.whiteColor;

  return configuration;
}

}  // namespace

@implementation ChromeAppBarPrototype

- (instancetype)init {
  self = [super init];
  if (self) {
    CHECK(IsDiamondPrototypeEnabled());

    UIBlurEffect* blurEffect =
        [UIBlurEffect effectWithStyle:UIBlurEffectStyleSystemThickMaterialDark];
    UIVisualEffectView* blurBackground =
        [[UIVisualEffectView alloc] initWithEffect:blurEffect];
    blurBackground.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:blurBackground];
    AddSameConstraints(self, blurBackground);

    UIButtonConfiguration* askGeminiConfiguration = ButtonConfiguration();
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
    askGeminiConfiguration.image =
        CustomSymbolWithPointSize(kGeminiBrandedLogoImage, kSymbolSize);
#else
    askGeminiConfiguration.image =
        DefaultSymbolWithPointSize(kGeminiNonBrandedLogoImage, kSymbolSize);
#endif  // BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)

    askGeminiConfiguration.title = @"TODO - gemini";
    _askGeminiButton = [UIButton buttonWithConfiguration:askGeminiConfiguration
                                           primaryAction:nil];
    _askGeminiButton.translatesAutoresizingMaskIntoConstraints = NO;

    UIButtonConfiguration* openNewTabConfiguration = ButtonConfiguration();
    openNewTabConfiguration.image =
        DefaultSymbolWithPointSize(kPlusInCircleSymbol, kSymbolSize);
    openNewTabConfiguration.title = @"TODO - new tab";
    _openNewTabButton =
        [UIButton buttonWithConfiguration:openNewTabConfiguration
                            primaryAction:nil];
    _openNewTabButton.translatesAutoresizingMaskIntoConstraints = NO;

    UIButtonConfiguration* tabGridConfiguration = ButtonConfiguration();
    // TODO(crbug.com/429955447): replace the symbol with a tab grid icon,
    // including number of tabs.
    tabGridConfiguration.image =
        DefaultSymbolWithPointSize(@"square", kSymbolSize);
    tabGridConfiguration.title = @"TODO - tab grid";
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
  }
  return self;
}

- (void)didMoveToSuperview {
  [super didMoveToSuperview];
  CHECK(IsDiamondPrototypeEnabled());
}

@end
