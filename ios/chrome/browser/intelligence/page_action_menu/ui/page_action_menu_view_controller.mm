// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/page_action_menu/ui/page_action_menu_view_controller.h"

#import "build/branding_buildflags.h"
#import "ios/chrome/browser/intelligence/bwg/utils/bwg_constants.h"
#import "ios/chrome/browser/intelligence/page_action_menu/utils/ai_hub_constants.h"
#import "ios/chrome/browser/intelligence/page_action_menu/utils/ai_hub_metrics.h"
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
const CGFloat kMenuBottomPadding = 54;

// The height of the menu's buttons.
const CGFloat kLargeButtonHeight = 60;
const CGFloat kSmallButtonHeight = 64;

// The point size of the icons for the small buttons.
const CGFloat kSmallButtonIconSize = 18;
const CGFloat kSmallButtonImagePadding = 2;

// The padding of the small buttons.
const CGFloat kSmallButtonPadding = 8;

// The margins between the small buttons.
const CGFloat kSpaceBetweenSmallButtons = 16;

// The opacity of the small buttons.
const CGFloat kSmallButtonOpacity = 0.95;

// The corner radius of the menu and its elements.
const CGFloat kMenuCornerRadius = 20;
const CGFloat kButtonsCornerRadius = 16;

// The padding between the image and text of the large button.
const CGFloat kLargeButtonImagePadding = 8;

}  // namespace

@interface PageActionMenuViewController () <
    UIAdaptivePresentationControllerDelegate>

// Whether reader mode is currently active.
@property(nonatomic, assign) BOOL readerModeActive;

@end

@implementation PageActionMenuViewController {
  // Stack view containing the menu's main content.
  UIStackView* _contentStackView;
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

  [self setupNavigationBar];

  _contentStackView = [[UIStackView alloc] init];
  _contentStackView.axis = UILayoutConstraintAxisVertical;
  _contentStackView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_contentStackView];

  // Horizontal stack view for the 2 side-by-side buttons.
  UIStackView* buttonsStackView = [self createSmallButtonsStackView];
  [_contentStackView addArrangedSubview:buttonsStackView];
  [_contentStackView setCustomSpacing:kStackViewMargins
                            afterView:buttonsStackView];

  // If Reader Mode is enabled, we use a 3-button UI. Otherwise, we just show
  // the `buttonsStackView`.
  if (IsReaderModeAvailable()) {
    // Adds the large Gemini entry point button.
    UIButton* BWGButton = [self createBWGButton];
    [_contentStackView addArrangedSubview:BWGButton];

    [NSLayoutConstraint activateConstraints:@[
      [BWGButton.heightAnchor
          constraintGreaterThanOrEqualToConstant:kLargeButtonHeight],
    ]];
  }

  [NSLayoutConstraint activateConstraints:@[
    // Anchors the menu to the sheet.
    [_contentStackView.topAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor
                       constant:kMenuTopPadding],
    [_contentStackView.leadingAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leadingAnchor
                       constant:kMenuSidePadding],
    [_contentStackView.trailingAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.trailingAnchor
                       constant:-kMenuSidePadding],

    // Anchors the height of menu elements.
    [buttonsStackView.heightAnchor
        constraintGreaterThanOrEqualToConstant:kSmallButtonHeight],
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
  [self dismissPageActionMenu];
}

#pragma mark - Private

// The total height of the presented menu.
- (CGFloat)preferredMenuHeight {
  CGFloat bottomPaddingAboveSafeArea =
      kMenuBottomPadding - self.view.safeAreaInsets.bottom;
  return self.navigationController.navigationBar.frame.size.height +
         [_contentStackView
             systemLayoutSizeFittingSize:UILayoutFittingCompressedSize]
             .height +
         kMenuTopPadding + bottomPaddingAboveSafeArea;
}

// Dismisses the page action menu.
- (void)dismissPageActionMenu {
  RecordAIHubAction(IOSAIHubAction::kDismiss);
  [self.pageActionMenuHandler dismissPageActionMenuWithCompletion:nil];
}

// Setups the navigation bar with a logo and dismiss button.
- (void)setupNavigationBar {
  // Configure the bar appearance.
  UINavigationBar* navigationBar = self.navigationController.navigationBar;
  UINavigationBarAppearance* appearance =
      [[UINavigationBarAppearance alloc] init];
  [appearance configureWithTransparentBackground];
  navigationBar.standardAppearance = appearance;

  // Add the dismiss button.
  UIBarButtonItem* dismissButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemClose
                           target:self
                           action:@selector(dismissPageActionMenu)];
  self.navigationItem.rightBarButtonItem = dismissButton;
}

// Creates a horizontal stack view for the side-by-side small buttons.
- (UIStackView*)createSmallButtonsStackView {
  // Create the stack view.
  UIStackView* stackView = [[UIStackView alloc] init];
  stackView.axis = UILayoutConstraintAxisHorizontal;
  stackView.distribution = UIStackViewDistributionFillEqually;
  stackView.alignment = UIStackViewAlignmentFill;
  stackView.spacing = kSpaceBetweenSmallButtons;
  stackView.translatesAutoresizingMaskIntoConstraints = NO;

  // Create the small buttons and add them to the stack view.
  UIButton* lensButton = [self
      createSmallButtonWithIcon:CustomSymbolWithPointSize(kCameraLensSymbol,
                                                          kSmallButtonIconSize)
                          title:l10n_util::GetNSString(
                                    IDS_IOS_AI_HUB_LENS_LABEL)
                        enabled:[self isLensAvailable]];
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
                                enabled:YES];
    [readerModeButton addTarget:self
                         action:@selector(handleReaderModeTapped:)
               forControlEvents:UIControlEventTouchUpInside];
    [stackView addArrangedSubview:readerModeButton];
  } else {
    UIButton* BWGSmallButton =
        [self createSmallButtonWithIcon:[self askGeminiIcon]
                                  title:l10n_util::GetNSString(
                                            IDS_IOS_AI_HUB_GEMINI_LABEL)
                                enabled:YES];
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
  buttonConfiguration.image = [self askGeminiIcon];
  buttonConfiguration.imagePlacement = NSDirectionalRectEdgeLeading;
  buttonConfiguration.imagePadding = kLargeButtonImagePadding;
  buttonConfiguration.baseForegroundColor =
      [UIColor colorNamed:kSolidWhiteColor];

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
                action:@selector(handleBWGTapped:)
      forControlEvents:UIControlEventTouchUpInside];

  return button;
}

// Creates and returns a small button with an icon and a title for the label. If
// the button is not `enabled`, a greyed out UI is shown and the tap target is
// disabled.
- (UIButton*)createSmallButtonWithIcon:(UIImage*)image
                                 title:(NSString*)title
                               enabled:(BOOL)enabled {
  // Create the background config.
  UIBackgroundConfiguration* backgroundConfig =
      [UIBackgroundConfiguration clearConfiguration];
  backgroundConfig.backgroundColor =
      [[UIColor colorNamed:kPrimaryBackgroundColor]
          colorWithAlphaComponent:kSmallButtonOpacity];
  backgroundConfig.cornerRadius = kButtonsCornerRadius;

  // Create the button config.
  UIButtonConfiguration* buttonConfiguration =
      [UIButtonConfiguration filledButtonConfiguration];
  buttonConfiguration.image = image;
  buttonConfiguration.imagePlacement = NSDirectionalRectEdgeTop;
  buttonConfiguration.imagePadding = kSmallButtonImagePadding;
  buttonConfiguration.baseForegroundColor = [UIColor colorNamed:kBlue600Color];
  buttonConfiguration.background = backgroundConfig;
  buttonConfiguration.contentInsets = NSDirectionalEdgeInsetsMake(
      kSmallButtonPadding, 0, kSmallButtonPadding, 0);

  // Set the font and text color as attributes.
  NSMutableDictionary* titleAttributes = [[NSMutableDictionary alloc] init];
  [titleAttributes
      setObject:PreferredFontForTextStyle(UIFontTextStyleSubheadline,
                                          UIFontWeightRegular)
         forKey:NSFontAttributeName];
  // If the button is enabled, override the text color. Otherwise, inherit the
  // disabled font color.
  if (enabled) {
    [titleAttributes setObject:[UIColor colorNamed:kTextPrimaryColor]
                        forKey:NSForegroundColorAttributeName];
  }
  NSMutableAttributedString* string =
      [[NSMutableAttributedString alloc] initWithString:title];
  [string addAttributes:titleAttributes range:NSMakeRange(0, string.length)];
  buttonConfiguration.attributedTitle = string;

  UIButton* button = [UIButton buttonWithConfiguration:buttonConfiguration
                                         primaryAction:nil];
  button.translatesAutoresizingMaskIntoConstraints = NO;

  [button setEnabled:enabled];

  return button;
}

// Returns the symbol for the Ask Gemini button.
- (UIImage*)askGeminiIcon {
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
  return CustomSymbolWithPointSize(kGeminiBrandedLogoImage,
                                   kSmallButtonIconSize);
#else
  return DefaultSymbolWithPointSize(kGeminiNonBrandedLogoImage,
                                    kSmallButtonIconSize);
#endif
}

// Whether the Lens overlay is currently available.
- (BOOL)isLensAvailable {
  return self.lensOverlayHandler != nil;
}

#pragma mark - Handlers

// Dismisses this view controller and starts the BWG overlay.
- (void)handleBWGTapped:(UIButton*)button {
  RecordAIHubAction(IOSAIHubAction::kGemini);
  PageActionMenuViewController* __weak weakSelf = self;
  [self.pageActionMenuHandler dismissPageActionMenuWithCompletion:^{
    [weakSelf.BWGHandler startBWGFlowWithEntryPoint:bwg::EntryPoint::AIHub];
  }];
}

// Dismisses the view controller and starts the Lens overlay.
- (void)handleLensEntryPointTapped:(UIButton*)button {
  RecordAIHubAction(IOSAIHubAction::kLens);
  PageActionMenuViewController* __weak weakSelf = self;
  [self.pageActionMenuHandler dismissPageActionMenuWithCompletion:^{
    [weakSelf.lensOverlayHandler
        createAndShowLensUI:YES
                 entrypoint:LensOverlayEntrypoint::kAIHub
                 completion:nil];
  }];
}

// Dismisses the view controller and starts Reader mode.
- (void)handleReaderModeTapped:(UIButton*)button {
  RecordAIHubAction(IOSAIHubAction::kReaderMode);
  PageActionMenuViewController* __weak weakSelf = self;
  [self.pageActionMenuHandler dismissPageActionMenuWithCompletion:^{
    weakSelf.readerModeActive ? [weakSelf.readerModeHandler hideReaderMode]
                              : [weakSelf.readerModeHandler showReaderMode];
  }];
}

@end
