// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/page_action_menu/ui/page_action_menu_view_controller.h"

#import "build/branding_buildflags.h"
#import "ios/chrome/browser/intelligence/page_action_menu/utils/ai_hub_constants.h"
#import "ios/chrome/browser/reader_mode/model/features.h"
#import "ios/chrome/browser/shared/public/commands/bwg_commands.h"
#import "ios/chrome/browser/shared/public/commands/lens_overlay_commands.h"
#import "ios/chrome/browser/shared/public/commands/page_action_menu_commands.h"
#import "ios/chrome/browser/shared/public/commands/reader_mode_commands.h"
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
const CGFloat kMenuBottomPaddingWithoutReaderMode = 32;

// The height of the menu's buttons.
const CGFloat kButtonHeight = 60;

// The point size of the icons for the small buttons.
const CGFloat kSmallButtonIconSize = 18;

// The padding of the small button.
const CGFloat kSmallButtonPadding = 8;

// The corner radius of the menu and its elements.
const CGFloat kMenuCornerRadius = 20;
const CGFloat kButtonsCornerRadius = 16;

// The height of the menu's header.
const CGFloat kMenuHeaderHeight = 58;

}  // namespace

@interface PageActionMenuViewController () <
    UIAdaptivePresentationControllerDelegate>

// Whether reader mode is currently active.
@property(nonatomic, assign) BOOL readerModeActive;

@end

@implementation PageActionMenuViewController {
  UIStackView* _mainStackView;
}

- (instancetype)initWithReaderModeActive:(BOOL)readerModeActive {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _readerModeActive = readerModeActive;
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.presentationController.delegate = self;

  // Add blurred background.
  UIBlurEffect* blurEffect =
      [UIBlurEffect effectWithStyle:UIBlurEffectStyleRegular];
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

  // If Reader Mode is enabled, we use a 3-button UI. Otherwise, we just show
  // the `buttonsStackView`.
  if (IsReaderModeAvailable()) {
    // Adds the large Gemini entry point button.
    UIButton* BWGButton = [self createBWGButton];
    [_mainStackView addArrangedSubview:BWGButton];

    [NSLayoutConstraint activateConstraints:@[
      [BWGButton.heightAnchor
          constraintGreaterThanOrEqualToConstant:kButtonHeight],
    ]];
  }

  // Activates constraints for the menu.
  AddSameConstraintsWithInsets(
      _mainStackView, self.view.safeAreaLayoutGuide,
      NSDirectionalEdgeInsetsMake(kMenuTopPadding, kMenuSidePadding,
                                  IsReaderModeAvailable()
                                      ? kMenuBottomPadding
                                      : kMenuBottomPaddingWithoutReaderMode,
                                  kMenuSidePadding));
  [NSLayoutConstraint activateConstraints:@[
    [menuHeader.heightAnchor constraintEqualToConstant:kMenuHeaderHeight],
    [buttonsStackView.heightAnchor
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
  self.sheetPresentationController.prefersEdgeAttachedInCompactHeight = YES;
  self.sheetPresentationController.prefersGrabberVisible = NO;
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self.pageActionMenuHandler dismissPageActionMenuWithCompletion:nil];
}

#pragma mark - Private

// The total height of the presented menu.
- (CGFloat)preferredMenuHeight {
  return
      [_mainStackView systemLayoutSizeFittingSize:UILayoutFittingCompressedSize]
          .height +
      kMenuTopPadding +
      (IsReaderModeAvailable() ? kMenuBottomPadding
                               : kMenuBottomPaddingWithoutReaderMode);
}

// Dismisses the page action menu.
- (void)dismissPageActionMenu {
  [self.pageActionMenuHandler dismissPageActionMenuWithCompletion:nil];
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
  // TODO(crbug.com/419246126): Use Chrome branded logo.
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
                                    IDS_IOS_AI_HUB_LENS_LABEL)
                    destructive:NO];
  [lensButton addTarget:self
                 action:@selector(handleLensEntryPointTapped:)
       forControlEvents:UIControlEventTouchUpInside];
  [stackView addArrangedSubview:lensButton];

  if (IsReaderModeAvailable()) {
    UIImage* readerModeImage =
        _readerModeActive
            ? DefaultSymbolWithPointSize(kHideActionSymbol,
                                         kSmallButtonIconSize)
            : DefaultSymbolWithPointSize(kReaderModeSymbolPostIOS18,
                                         kSmallButtonIconSize);

    NSString* readerModeLabelText =
        _readerModeActive
            ? l10n_util::GetNSString(IDS_IOS_AI_HUB_HIDE_READER_MODE_LABEL)
            : l10n_util::GetNSString(IDS_IOS_AI_HUB_READER_MODE_LABEL);

    UIButton* readerModeButton =
        [self createSmallButtonWithIcon:readerModeImage
                                  title:readerModeLabelText
                            destructive:_readerModeActive];
    [readerModeButton addTarget:self
                         action:@selector(handleReaderModeTapped:)
               forControlEvents:UIControlEventTouchUpInside];
    [stackView addArrangedSubview:readerModeButton];
  } else {
    // TODO(crbug.com/419067173): Update the icon.
    UIButton* BWGSmallButton =
        [self createSmallButtonWithIcon:DefaultSymbolWithPointSize(
                                            @"sparkle", kSmallButtonIconSize)
                                  title:l10n_util::GetNSString(
                                            IDS_IOS_AI_HUB_BWG_LABEL)
                            destructive:NO];
    [BWGSmallButton addTarget:self
                       action:@selector(handleBWGTapped:)
             forControlEvents:UIControlEventTouchUpInside];
    [stackView addArrangedSubview:BWGSmallButton];
  }

  return stackView;
}

// Creates a large button for the BWG entry point.
- (UIButton*)createBWGButton {
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
      initWithString:l10n_util::GetNSString(IDS_IOS_AI_HUB_BWG_LABEL)];
  [string addAttributes:titleAttributes range:NSMakeRange(0, string.length)];
  buttonConfiguration.attributedTitle = string;

  UIButton* button = [UIButton buttonWithConfiguration:buttonConfiguration
                                         primaryAction:nil];
  button.translatesAutoresizingMaskIntoConstraints = NO;
  [button addTarget:self
                action:@selector(handleBWGTapped:)
      forControlEvents:UIControlEventTouchUpInside];

  return button;
}

// Creates and returns a small button with an icon and a title for the label. If
// `destructive` is YES, the button applies red styling.
- (UIButton*)createSmallButtonWithIcon:(UIImage*)image
                                 title:(NSString*)title
                           destructive:(BOOL)destructive {
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
  buttonConfiguration.baseForegroundColor =
      destructive ? [UIColor colorNamed:kRed500Color]
                  : [UIColor colorNamed:kBlue600Color];
  buttonConfiguration.background = backgroundConfig;
  buttonConfiguration.contentInsets = NSDirectionalEdgeInsetsMake(
      kSmallButtonPadding, 0, kSmallButtonPadding, 0);

  // Set the font and text color as attributes.
  UIFont* font = PreferredFontForTextStyle(UIFontTextStyleSubheadline,
                                           UIFontWeightRegular);
  NSDictionary* titleAttributes = @{
    NSFontAttributeName : font,
    NSForegroundColorAttributeName : destructive
        ? [UIColor colorNamed:kRed500Color]
        : [UIColor colorNamed:kTextPrimaryColor]
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

// Dismisses this view controller and starts the BWG overlay.
- (void)handleBWGTapped:(UIButton*)button {
  PageActionMenuViewController* __weak weakSelf = self;
  [self.pageActionMenuHandler dismissPageActionMenuWithCompletion:^{
    [weakSelf.BWGHandler startBWGFlow];
  }];
}

- (void)handleLensEntryPointTapped:(UIButton*)button {
  PageActionMenuViewController* __weak weakSelf = self;
  [self.pageActionMenuHandler dismissPageActionMenuWithCompletion:^{
    [weakSelf.lensOverlayHandler
        createAndShowLensUI:YES
                 entrypoint:LensOverlayEntrypoint::kAIHub
                 completion:nil];
  }];
}

- (void)handleReaderModeTapped:(UIButton*)button {
  PageActionMenuViewController* __weak weakSelf = self;
  [self.pageActionMenuHandler dismissPageActionMenuWithCompletion:^{
    weakSelf.readerModeActive ? [weakSelf.readerModeHandler hideReaderMode]
                              : [weakSelf.readerModeHandler showReaderMode];
  }];
}

@end
