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

@implementation ChromeAppBarPrototype

- (instancetype)init {
  self = [super init];
  if (self) {
    CHECK(IsDiamondPrototypeEnabled());

    UIColor* backgroundColor =
        [[UIColor colorNamed:kStaticGrey600Color] colorWithAlphaComponent:0.4];
    self.backgroundColor = backgroundColor;
    GradientView* gradientView =
        [[GradientView alloc] initWithTopColor:UIColor.clearColor
                                   bottomColor:backgroundColor];
    gradientView.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:gradientView];

    [NSLayoutConstraint activateConstraints:@[
      [gradientView.bottomAnchor constraintEqualToAnchor:self.topAnchor],
      [gradientView.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
      [gradientView.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
      [gradientView.heightAnchor constraintEqualToConstant:16],
    ]];

    UIBlurEffect* blurEffect =
        [UIBlurEffect effectWithStyle:UIBlurEffectStyleSystemThinMaterialDark];
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
  }
  return self;
}

- (void)didMoveToSuperview {
  [super didMoveToSuperview];
  CHECK(IsDiamondPrototypeEnabled());
}

@end
