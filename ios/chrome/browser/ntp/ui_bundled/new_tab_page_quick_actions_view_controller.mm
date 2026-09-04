// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_quick_actions_view_controller.h"

#import "ios/chrome/browser/content_suggestions/ui/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_color_palette.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_image_background_trait.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_shortcuts_handler.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_trait.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_utils.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// The spacing in points between the buttons.
const CGFloat kButtonStackViewSpacing = 8.0;

// The height for the quick actions button row.
constexpr CGFloat kQuickActionsHeight = 44.0;
constexpr CGFloat kQuickActionsHeightUICleanup = 50.0;

// The border radius for a quick action button.
const CGFloat kButtonCornerRadius = 24.0;

// The size of the quick actions symbols.
constexpr CGFloat kSymbolPointSize = 18.0;
constexpr CGFloat kSymbolPointSizeUICleanup = 14.0;

// The maximum font size for the quick actions button.
const CGFloat kMaximumFontSize = 20.0;

// The horizontal inset margin for the button stack view in a regular x regular
// size class.
constexpr CGFloat kHorizontalInsetRegularXRegular = 36.0;

// Returns the leading margin for the button stack based on the window's size
// class.
CGFloat HorizontalInsetForQuickActions(
    id<UITraitEnvironment> trait_environment) {
  if (!IsNewTabPageUICleanupEnabled()) {
    return 0.0;
  }
  return IsRegularXRegularSizeClass(trait_environment)
             ? kHorizontalInsetRegularXRegular
             : 0.0;
}

// The color used to match the fakebox background.
NSString* const kFakeboxMatchingBackgroundColor =
    @"fake_omnibox_bottom_gradient_color";

// Returns the color needed for the background of the button.
UIColor* ButtonBackgroundColor(NewTabPageColorPalette* color_palette) {
  if (color_palette) {
    return color_palette.omniboxColor;
  }
  if (IsNewTabPageUICleanupEnabled()) {
    return [UIColor colorNamed:kNTPQuickActionChipColor];
  }
  return [UIColor colorNamed:kFakeboxMatchingBackgroundColor];
}

}  // namespace

@implementation NewTabPageQuickActionsViewController {
  // The stack view containing the quick actions buttons.
  UIStackView* _buttonStackView;

  // Constraints for the leading and trailing edges of the `_buttonStackView`.
  NSLayoutConstraint* _stackViewLeadingConstraint;
  NSLayoutConstraint* _stackViewTrailingConstraint;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  _buttonStackView = [self createButtonStackView];
  [self.view addSubview:_buttonStackView];

  CGFloat inset = HorizontalInsetForQuickActions(self);

  _stackViewLeadingConstraint = [_buttonStackView.leadingAnchor
      constraintEqualToAnchor:self.view.leadingAnchor
                     constant:inset];
  _stackViewTrailingConstraint = [_buttonStackView.trailingAnchor
      constraintEqualToAnchor:self.view.trailingAnchor
                     constant:-inset];

  [NSLayoutConstraint activateConstraints:@[
    [_buttonStackView.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [_buttonStackView.bottomAnchor
        constraintEqualToAnchor:self.view.bottomAnchor],
    [_buttonStackView.heightAnchor
        constraintEqualToConstant:IsNewTabPageUICleanupEnabled()
                                      ? kQuickActionsHeightUICleanup
                                      : kQuickActionsHeight],
    _stackViewLeadingConstraint,
    _stackViewTrailingConstraint,
  ]];

  if (IsNewTabPageUICleanupEnabled()) {
    [self registerForTraitChanges:@[
      UITraitHorizontalSizeClass.class, UITraitVerticalSizeClass.class
    ]
                       withAction:@selector(updateButtonStackConstraints)];
  }

  if (IsAimEnabledInNtp()) {
    _aimButton =
        [self createButtonWithSymbol:SymbolMagnifyingglassSpark
                               title:l10n_util::GetNSString(
                                         IDS_IOS_NTP_QUICK_ACTIONS_AIM)];
    [_buttonStackView addArrangedSubview:_aimButton];
    [self.layoutGuideCenter referenceView:_aimButton
                                underName:kNTPAIMButtonGuide];
  }

  _incognitoButton =
      [self createButtonWithSymbol:SymbolIncognito
                             title:l10n_util::GetNSString(
                                       IDS_IOS_NTP_QUICK_ACTIONS_INCOGNITO)];
  [_buttonStackView addArrangedSubview:_incognitoButton];

  [self setupQuickActionsButtonsAccessibility];

  [_incognitoButton addTarget:self
                       action:@selector(openIncognitoSearch)
             forControlEvents:UIControlEventTouchUpInside];
  [_aimButton addTarget:self
                 action:@selector(openAIM)
       forControlEvents:UIControlEventTouchUpInside];
}

- (CGSize)preferredContentSize {
  return CGSizeMake(super.preferredContentSize.width,
                    IsNewTabPageUICleanupEnabled()
                        ? kQuickActionsHeightUICleanup
                        : kQuickActionsHeight);
}

#pragma mark - Private

// Updates the horizontal constraints for the button stack view based on the
// layout environment.
- (void)updateButtonStackConstraints {
  CHECK(IsNewTabPageUICleanupEnabled());
  if (!_stackViewLeadingConstraint && !_stackViewTrailingConstraint) {
    return;
  }
  CGFloat inset = HorizontalInsetForQuickActions(self);
  _stackViewLeadingConstraint.constant = inset;
  _stackViewTrailingConstraint.constant = -inset;
}

- (void)setupQuickActionsButtonsAccessibility {
  _incognitoButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_ACCNAME_NEW_INCOGNITO_TAB);
  _incognitoButton.accessibilityIdentifier = kNTPIncognitoQuickActionIdentifier;
}

// Creates a new horizontal button stack view.
- (UIStackView*)createButtonStackView {
  UIStackView* stackView = [[UIStackView alloc] init];
  stackView.translatesAutoresizingMaskIntoConstraints = NO;
  stackView.distribution = UIStackViewDistributionFillEqually;
  stackView.alignment = UIStackViewAlignmentFill;
  stackView.axis = UILayoutConstraintAxisHorizontal;
  stackView.spacing = kButtonStackViewSpacing;

  return stackView;
}

// Creates a new quick action button with the given `icon`.
- (UIButton*)createButtonWithSymbol:(Symbol)symbol {
  return [self createButtonWithSymbol:symbol title:nil];
}

// Creates a new quick action button with the given `icon` and title.
- (UIButton*)createButtonWithSymbol:(Symbol)symbol title:(NSString*)title {
  UIButtonConfiguration* configuration =
      [UIButtonConfiguration plainButtonConfiguration];
  configuration.background.backgroundColor = ButtonBackgroundColor(nil);
  configuration.background.cornerRadius = kButtonCornerRadius;
  configuration.baseForegroundColor = [UIColor colorNamed:kGrey700Color];
  UIImage* icon;
  if (IsNewTabPageUICleanupEnabled()) {
    UIImageSymbolConfiguration* symbolConfiguration =
        [UIImageSymbolConfiguration
            configurationWithPointSize:kSymbolPointSizeUICleanup
                                weight:UIImageSymbolWeightSemibold];
    icon = SymbolWithConfiguration(symbol, symbolConfiguration);
  } else {
    icon = SymbolWithPointSize(symbol, kSymbolPointSize);
  }
  configuration.image = MakeSymbolMonochrome(icon);

  if (title) {
    UIFont* font = PreferredFontForTextStyle(
        UIFontTextStyleSubheadline, UIFontWeightRegular, kMaximumFontSize);
    NSDictionary* attributes = @{NSFontAttributeName : font};
    NSAttributedString* attributedTitle =
        [[NSAttributedString alloc] initWithString:title attributes:attributes];
    configuration.attributedTitle = attributedTitle;
    configuration.titleLineBreakMode = NSLineBreakByTruncatingTail;
    configuration.imagePadding = 8;
  }

  UIButton* button = [[UIButton alloc] init];
  UIColor* baseTintColor =
      content_suggestions::DefaultIconTintColorWithAIMAllowed(YES);
  button.configurationUpdateHandler =
      CreateThemedButtonConfigurationUpdateHandler(
          baseTintColor,
          ^(NewTabPageColorPalette* palette) {
            return ButtonBackgroundColor(palette);
          },
          UIBlurEffectStyleSystemThickMaterial);

  button.translatesAutoresizingMaskIntoConstraints = NO;
  button.configuration = configuration;
  return button;
}

#pragma mark - Button actions

- (void)openIncognitoSearch {
  [self.NTPShortcutsHandler openIncognitoSearch];
}

- (void)openAIM {
  [self.NTPShortcutsHandler openAIM];
}

- (void)setLayoutGuideCenter:(LayoutGuideCenter*)layoutGuideCenter {
  _layoutGuideCenter = layoutGuideCenter;
  if (_aimButton) {
    [_layoutGuideCenter referenceView:_aimButton underName:kNTPAIMButtonGuide];
  }
}

@end
