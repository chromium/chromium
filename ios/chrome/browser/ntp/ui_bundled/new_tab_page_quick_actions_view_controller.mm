// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_quick_actions_view_controller.h"

#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_shortcuts_handler.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
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

  BOOL showIncognito = GetNTPMIAEntrypointVariation() !=
                       NTPMIAEntrypointVariation::kEnlargedFakeboxNoIncognito;
  if (showIncognito) {
    _incognitoButton = [self createButtonWithSymbolName:kIncognitoSymbol];
    [_buttonStackView addArrangedSubview:_incognitoButton];
  }

  _voiceSearchButton = [self createButtonWithSymbolName:kVoiceSymbol];
  _lensButton = [self createButtonWithSymbolName:kCameraLensSymbol];

  [_buttonStackView addArrangedSubview:_voiceSearchButton];
  [_buttonStackView addArrangedSubview:_lensButton];

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
}

- (CGSize)preferredContentSize {
  return CGSizeMake(super.preferredContentSize.width, kQuickActionsHeight);
}

#pragma mark - Private

- (void)setupQuickActionsButtonsAccessibility {
  _incognitoButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_ACCNAME_NEW_INCOGNITO_TAB);
  _lensButton.accessibilityLabel = l10n_util::GetNSString(IDS_IOS_ACCNAME_LENS);
  _voiceSearchButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_ACCNAME_VOICE_SEARCH);
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
  UIButton* button = [[UIButton alloc] init];
  button.translatesAutoresizingMaskIntoConstraints = NO;
  button.backgroundColor = [UIColor colorNamed:kBackgroundColor];
  button.layer.cornerRadius = kButtonCornerRadius;
  button.tintColor = [UIColor colorNamed:kGrey700Color];
  UIImage* icon = CustomSymbolWithPointSize(symbolName, kSymbolPointSize);
  [button setImage:MakeSymbolMonochrome(icon) forState:UIControlStateNormal];

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

@end
