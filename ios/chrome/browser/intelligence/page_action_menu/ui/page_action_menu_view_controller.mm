// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/page_action_menu/ui/page_action_menu_view_controller.h"

#import "build/branding_buildflags.h"
#import "ios/chrome/browser/intelligence/page_action_menu/utils/ai_hub_constants.h"
#import "ios/chrome/browser/shared/public/commands/glic_commands.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// The spacing between elements in the menu.
const CGFloat kStackViewMargins = 16;

// The padding surrounding the menu's content.
const CGFloat kMenuSidePadding = 16;
const CGFloat kMenuTopPadding = 8;
const CGFloat kMenuBottomPadding = 16;

// The height of the menu's buttons.
const CGFloat kButtonHeight = 60;

// The point size of the icons for the small buttons.
const CGFloat kSmallButtonIconSize = 18;

// The padding of the small button.
const CGFloat kSmallButtonPadding = 8;

// The corner radius of the menu and its elements.
const CGFloat kMenuCornerRadius = 16;
const CGFloat kButtonsCornerRadius = 16;

// The height of the menu's header.
const CGFloat kMenuHeaderHeight = 58;

}  // namespace

@implementation PageActionMenuViewController {
  UIStackView* _mainStackView;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  // Add blurred background.
  UIBlurEffect* blurEffect =
      [UIBlurEffect effectWithStyle:UIBlurEffectStyleLight];
  UIVisualEffectView* blurEffectView =
      [[UIVisualEffectView alloc] initWithEffect:blurEffect];
  blurEffectView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:blurEffectView];
  AddSameConstraints(blurEffectView, self.view);

  // Configure main content stack view.
  _mainStackView = [[UIStackView alloc] init];
  _mainStackView.axis = UILayoutConstraintAxisVertical;
  _mainStackView.distribution = UIStackViewDistributionEqualCentering;
  _mainStackView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_mainStackView];

  UIView* menuHeader = [self createMenuHeader];
  [_mainStackView addArrangedSubview:menuHeader];

  // Horizontal stack view for the 2 side-by-side buttons.
  UIStackView* buttonsStackView = [self createSmallButtonsStackView];
  [_mainStackView addArrangedSubview:buttonsStackView];
  [_mainStackView setCustomSpacing:kStackViewMargins
                         afterView:buttonsStackView];

  // Adds the large Gemini entry point button.
  UIButton* askGeminiButton = [self createAskGeminiButton];
  [_mainStackView addArrangedSubview:askGeminiButton];

  // Activates constraints for the menu.
  AddSameConstraintsWithInsets(
      _mainStackView, self.view.safeAreaLayoutGuide,
      NSDirectionalEdgeInsetsMake(kMenuTopPadding, kMenuSidePadding,
                                  kMenuBottomPadding, kMenuSidePadding));
  [NSLayoutConstraint activateConstraints:@[
    [menuHeader.heightAnchor constraintEqualToConstant:kMenuHeaderHeight],
    [buttonsStackView.heightAnchor
        constraintGreaterThanOrEqualToConstant:kButtonHeight],
    [askGeminiButton.heightAnchor
        constraintGreaterThanOrEqualToConstant:kButtonHeight],
  ]];

  // Configure presentation sheet.
  __weak PageActionMenuViewController* weakSelf = self;
  auto detentResolver = ^CGFloat(
      id<UISheetPresentationControllerDetentResolutionContext> context) {
    return [weakSelf preferredMenuHeight];
  };
  UISheetPresentationControllerDetent* initialDetent =
      [UISheetPresentationControllerDetent
          customDetentWithIdentifier:kAIHubDetentIdentifier
                            resolver:detentResolver];
  self.sheetPresentationController.detents = @[
    initialDetent,
  ];
  self.sheetPresentationController.selectedDetentIdentifier =
      kAIHubDetentIdentifier;
  self.sheetPresentationController.preferredCornerRadius = kMenuCornerRadius;
  self.sheetPresentationController.prefersGrabberVisible = NO;
}

#pragma mark - Private

// The total height of the presented menu.
// TODO(crbug.com/419245200): Adapt height for landscape.
- (CGFloat)preferredMenuHeight {
  return
      [_mainStackView systemLayoutSizeFittingSize:UILayoutFittingCompressedSize]
          .height +
      kMenuTopPadding + kMenuBottomPadding;
}

// Dismisses the page action menu.
- (void)dismissPageActionMenu {
  // TODO(crbug.com/414374298): Handle button actions and make sure the
  // coordinator is stopped when the menu is dismissed.
}

// Creates a top bar header with a logo and dismiss button.
- (UIView*)createMenuHeader {
  // Configure the bar.
  UINavigationBar* topBar = [[UINavigationBar alloc] init];
  topBar.translatesAutoresizingMaskIntoConstraints = NO;
  UINavigationBarAppearance* appearance =
      [[UINavigationBarAppearance alloc] init];
  [appearance configureWithTransparentBackground];
  topBar.standardAppearance = appearance;

  // Add the dismiss button.
  UIBarButtonItem* dismissButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemClose
                           target:self
                           action:@selector(dismissPageActionMenu)];
  UINavigationItem* navigationItem = [[UINavigationItem alloc] init];
  navigationItem.rightBarButtonItem = dismissButton;
  [topBar setItems:@[ navigationItem ] animated:NO];

  // Add the logo.
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
  // TODO(crbug.com/414374298): Use Chrome branded logo.
  UIImageView* logoIcon = [[UIImageView alloc]
      initWithImage:[UIImage imageNamed:@"page_action_menu_header_chromium"]];
#else
  UIImageView* logoIcon = [[UIImageView alloc]
      initWithImage:[UIImage imageNamed:@"page_action_menu_header_chromium"]];
#endif
  logoIcon.translatesAutoresizingMaskIntoConstraints = NO;
  [topBar addSubview:logoIcon];
  [NSLayoutConstraint activateConstraints:@[
    [logoIcon.centerXAnchor constraintEqualToAnchor:topBar.centerXAnchor],
    [logoIcon.centerYAnchor constraintEqualToAnchor:topBar.centerYAnchor],
  ]];

  return topBar;
}

// Creates a horizontal stack view for the side-by-side small buttons.
- (UIStackView*)createSmallButtonsStackView {
  // Create the stack view.
  UIStackView* stackView = [[UIStackView alloc] init];
  stackView.axis = UILayoutConstraintAxisHorizontal;
  stackView.distribution = UIStackViewDistributionFillEqually;
  stackView.alignment = UIStackViewAlignmentFill;
  stackView.spacing = kStackViewMargins;
  stackView.translatesAutoresizingMaskIntoConstraints = NO;

  // Create the small buttons and add them to the stack view.
  UIButton* lensButton = [self
      createSmallButtonWithIcon:CustomSymbolWithPointSize(kCameraLensSymbol,
                                                          kSmallButtonIconSize)
                          title:l10n_util::GetNSString(
                                    IDS_IOS_AI_HUB_LENS_LABEL)];
  [stackView addArrangedSubview:lensButton];
  UIButton* readerModeButton =
      [self createSmallButtonWithIcon:DefaultSymbolWithPointSize(
                                          kReaderModeSymbolPostIOS18,
                                          kSmallButtonIconSize)
                                title:l10n_util::GetNSString(
                                          IDS_IOS_AI_HUB_READER_MODE_LABEL)];
  [stackView addArrangedSubview:readerModeButton];

  return stackView;
}

// Creates a large button for the Gemini entry point.
- (UIButton*)createAskGeminiButton {
  // Create the background config.
  UIBackgroundConfiguration* backgroundConfig =
      [UIBackgroundConfiguration clearConfiguration];
  backgroundConfig.backgroundColor = [UIColor colorNamed:kBlue600Color];
  backgroundConfig.cornerRadius = kButtonsCornerRadius;

  // Create the button config.
  UIButtonConfiguration* buttonConfiguration =
      [UIButtonConfiguration filledButtonConfiguration];
  buttonConfiguration.background = backgroundConfig;

  // Set the font and text color as attributes.
  UIFont* font = PreferredFontForTextStyle(UIFontTextStyleHeadline);
  NSDictionary* titleAttributes = @{
    NSFontAttributeName : font,
  };
  NSMutableAttributedString* string = [[NSMutableAttributedString alloc]
      initWithString:l10n_util::GetNSString(IDS_IOS_AI_HUB_GEMINI_LABEL)];
  [string addAttributes:titleAttributes range:NSMakeRange(0, string.length)];
  buttonConfiguration.attributedTitle = string;

  UIButton* button = [UIButton buttonWithConfiguration:buttonConfiguration
                                         primaryAction:nil];
  button.translatesAutoresizingMaskIntoConstraints = NO;
  [button addTarget:self
                action:@selector(handleGlicTapped:)
      forControlEvents:UIControlEventTouchUpInside];

  return button;
}

// Creates and returns a small button with an icon and a title for the label.
- (UIButton*)createSmallButtonWithIcon:(UIImage*)image title:(NSString*)title {
  // Create the background config.
  UIBackgroundConfiguration* backgroundConfig =
      [UIBackgroundConfiguration clearConfiguration];
  backgroundConfig.backgroundColor = [UIColor colorNamed:kBackgroundColor];
  backgroundConfig.cornerRadius = kButtonsCornerRadius;

  // Create the button config.
  UIButtonConfiguration* buttonConfiguration =
      [UIButtonConfiguration filledButtonConfiguration];
  buttonConfiguration.image = image;
  buttonConfiguration.imagePlacement = NSDirectionalRectEdgeTop;
  buttonConfiguration.baseForegroundColor = [UIColor colorNamed:kBlue600Color];
  buttonConfiguration.background = backgroundConfig;
  buttonConfiguration.contentInsets = NSDirectionalEdgeInsetsMake(
      kSmallButtonPadding, 0, kSmallButtonPadding, 0);

  // Set the font and text color as attributes.
  UIFont* font = PreferredFontForTextStyle(UIFontTextStyleSubheadline,
                                           UIFontWeightRegular);
  NSDictionary* titleAttributes = @{
    NSFontAttributeName : font,
    NSForegroundColorAttributeName : [UIColor colorNamed:kTextPrimaryColor]
  };
  NSMutableAttributedString* string =
      [[NSMutableAttributedString alloc] initWithString:title];
  [string addAttributes:titleAttributes range:NSMakeRange(0, string.length)];
  buttonConfiguration.attributedTitle = string;

  UIButton* button = [UIButton buttonWithConfiguration:buttonConfiguration
                                         primaryAction:nil];
  button.translatesAutoresizingMaskIntoConstraints = NO;

  return button;
}

#pragma mark - Private

// Dismisses this view controller and starts the GLIC overlay.
- (void)handleGlicTapped:(UIButton*)button {
  PageActionMenuViewController* __weak weakSelf = self;
  [self dismissViewControllerAnimated:YES
                           completion:^{
                             [weakSelf.handler startGlicFlow];
                           }];
}

@end
