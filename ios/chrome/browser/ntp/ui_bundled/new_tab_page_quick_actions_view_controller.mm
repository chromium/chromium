// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_quick_actions_view_controller.h"

#import "ios/chrome/browser/content_suggestions/ui_bundled/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_color_palette.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_image_background_trait.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_shortcuts_handler.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_trait.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_utils.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// The spacing in points between the buttons.
const CGFloat kButtonStackViewSpacing = 8.0;

// The height for the quick actions button row.
const CGFloat kQuickActionsHeight = 44.0;

// The border radius for a quick action button.
const CGFloat kButtonCornerRadius = 24.0;

// The sise of the quick actions symbols.
const CGFloat kSymbolPointSize = 18.0;

// The maximum font size for the quick actions button.
const CGFloat kMaximumFontSize = 20.0;

// The color used to match the fakebox background.
NSString* const kFakeboxMatchingBackgroundColor =
    @"fake_omnibox_bottom_gradient_color";

// Returns the color needed for the background of the button.
UIColor* ButtonBackgroundColor(NewTabPageColorPalette* colorPalette) {
  if (GetNTPMIAEntrypointVariation() ==
      NTPMIAEntrypointVariation::kOmniboxContainedSingleButton) {
    return colorPalette ? colorPalette.secondaryCellColor
                        : [UIColor colorNamed:kBackgroundColor];
  }

  // All other treatments use the same color as the fakebox.
  return colorPalette ? colorPalette.omniboxColor
                      : [UIColor colorNamed:kFakeboxMatchingBackgroundColor];
}

}  // namespace

@implementation NewTabPageQuickActionsViewController {
  // The stack view containing the quick actions buttons.
  UIStackView* _buttonStackView;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  _buttonStackView = [self createButtonStackView];
  [self.view addSubview:_buttonStackView];

  AddSameConstraints(_buttonStackView, self.view);
  [NSLayoutConstraint
      activateConstraints:@[ [_buttonStackView.heightAnchor
                              constraintEqualToConstant:kQuickActionsHeight] ]];
  BOOL showAIMEntrypoint = GetNTPMIAEntrypointVariation() ==
                           NTPMIAEntrypointVariation::kAIMInQuickAction;
  if (showAIMEntrypoint) {
    _aimButton =
        [self createButtonWithSymbolName:kMagnifyingglassSparkSymbol
                                   title:l10n_util::GetNSString(
                                             IDS_IOS_NTP_QUICK_ACTIONS_AIM)];
    [_buttonStackView addArrangedSubview:_aimButton];
  }

  BOOL showIncognito = GetNTPMIAEntrypointVariation() !=
                       NTPMIAEntrypointVariation::kEnlargedFakeboxNoIncognito;
  if (showIncognito) {
    if (showAIMEntrypoint) {
      _incognitoButton = [self
          createButtonWithSymbolName:kIncognitoSymbol
                               title:l10n_util::GetNSString(
                                         IDS_IOS_NTP_QUICK_ACTIONS_INCOGNITO)];
    } else {
      _incognitoButton = [self createButtonWithSymbolName:kIncognitoSymbol];
    }
    [_buttonStackView addArrangedSubview:_incognitoButton];
  }

  BOOL showVoiceLens = GetNTPMIAEntrypointVariation() !=
                       NTPMIAEntrypointVariation::kAIMInQuickAction;

  if (showVoiceLens) {
    _voiceSearchButton = [self createButtonWithSymbolName:kVoiceSymbol];
    _lensButton = [self createButtonWithSymbolName:kCameraLensSymbol];

    [_buttonStackView addArrangedSubview:_voiceSearchButton];
    [_buttonStackView addArrangedSubview:_lensButton];
  }

  [self setupQuickActionsButtonsAccessibility];

  [_lensButton addTarget:self
                  action:@selector(openLensViewFinder)
        forControlEvents:UIControlEventTouchUpInside];
  [_voiceSearchButton addTarget:self
                         action:@selector(loadVoiceSearch)
               forControlEvents:UIControlEventTouchUpInside];
  [_voiceSearchButton addTarget:self
                         action:@selector(preloadVoiceSearch:)
               forControlEvents:UIControlEventTouchDown];
  [_incognitoButton addTarget:self
                       action:@selector(openIncognitoSearch)
             forControlEvents:UIControlEventTouchUpInside];
  [_aimButton addTarget:self
                 action:@selector(openAIM)
       forControlEvents:UIControlEventTouchUpInside];
}

- (CGSize)preferredContentSize {
  return CGSizeMake(super.preferredContentSize.width, kQuickActionsHeight);
}

#pragma mark - Private

- (void)setupQuickActionsButtonsAccessibility {
  _incognitoButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_ACCNAME_NEW_INCOGNITO_TAB);
  _incognitoButton.accessibilityIdentifier = kNTPIncognitoQuickActionIdentifier;
  _lensButton.accessibilityLabel = l10n_util::GetNSString(IDS_IOS_ACCNAME_LENS);
  _lensButton.accessibilityIdentifier = kNTPLensQuickActionIdentifier;
  _voiceSearchButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_ACCNAME_VOICE_SEARCH);
  _voiceSearchButton.accessibilityIdentifier =
      kNTPVoiceSearchQuickActionIdentifier;
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
- (UIButton*)createButtonWithSymbolName:(NSString*)symbolName {
  return [self createButtonWithSymbolName:symbolName title:nil];
}

// Creates a new quick action button with the given `icon` and title.
- (UIButton*)createButtonWithSymbolName:(NSString*)symbolName
                                  title:(NSString*)title {
  UIButtonConfiguration* configuration =
      [UIButtonConfiguration plainButtonConfiguration];
  configuration.background.backgroundColor = ButtonBackgroundColor(nil);
  configuration.background.cornerRadius = kButtonCornerRadius;
  configuration.baseForegroundColor = [UIColor colorNamed:kGrey700Color];
  UIImage* icon = CustomSymbolWithPointSize(symbolName, kSymbolPointSize);
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
  if (GetNTPMIAEntrypointVariation() ==
      NTPMIAEntrypointVariation::kOmniboxContainedSingleButton) {
    button.configurationUpdateHandler =
        CreateThemedButtonConfigurationUpdateHandler(
            baseTintColor, ^(NewTabPageColorPalette* palette) {
              return ButtonBackgroundColor(palette);
            });
  } else {
    // Other variations change the blur background to match the omnibox.
    button.configurationUpdateHandler =
        CreateThemedButtonConfigurationUpdateHandler(
            baseTintColor,
            ^(NewTabPageColorPalette* palette) {
              return ButtonBackgroundColor(palette);
            },
            UIBlurEffectStyleSystemThickMaterial);
  }

  button.translatesAutoresizingMaskIntoConstraints = NO;
  button.configuration = configuration;
  return button;
}

#pragma mark - Button actions

- (void)openLensViewFinder {
  [self.NTPShortcutsHandler openLensViewFinder];
}

- (void)loadVoiceSearch {
  [self.NTPShortcutsHandler loadVoiceSearchFromView:_voiceSearchButton];
}

- (void)preloadVoiceSearch:(id)sender {
  [sender removeTarget:self
                action:@selector(preloadVoiceSearch:)
      forControlEvents:UIControlEventTouchDown];
  [self.NTPShortcutsHandler preloadVoiceSearch];
}

- (void)openIncognitoSearch {
  [self.NTPShortcutsHandler openIncognitoSearch];
}

- (void)openAIM {
  [self.NTPShortcutsHandler openMIA];
}

@end
