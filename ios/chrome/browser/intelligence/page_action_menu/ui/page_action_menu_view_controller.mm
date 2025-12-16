// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/page_action_menu/ui/page_action_menu_view_controller.h"

#import "base/strings/sys_string_conversions.h"
#import "build/branding_buildflags.h"
#import "ios/chrome/browser/intelligence/bwg/utils/bwg_constants.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/intelligence/page_action_menu/ui/page_action_menu_feature.h"
#import "ios/chrome/browser/intelligence/page_action_menu/ui/page_action_menu_mutator.h"
#import "ios/chrome/browser/intelligence/page_action_menu/ui/page_action_menu_view_controller_delegate.h"
#import "ios/chrome/browser/intelligence/page_action_menu/utils/ai_hub_constants.h"
#import "ios/chrome/browser/intelligence/page_action_menu/utils/ai_hub_metrics.h"
#import "ios/chrome/browser/reader_mode/model/constants.h"
#import "ios/chrome/browser/reader_mode/model/features.h"
#import "ios/chrome/browser/shared/public/commands/bwg_commands.h"
#import "ios/chrome/browser/shared/public/commands/lens_overlay_commands.h"
#import "ios/chrome/browser/shared/public/commands/page_action_menu_commands.h"
#import "ios/chrome/browser/shared/public/commands/reader_mode_commands.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/chrome_button.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// The corner radii for the entire Page Action Menu.
const CGFloat kMenuCornerRadiusIOS26 = 28;
const CGFloat kMenuCornerRadius = 20;

// The spacing between elements in the menu.
const CGFloat kStackViewMargins = 20;

// The padding surrounding the menu's content.
const CGFloat kMenuSidePadding = 16;
const CGFloat kMenuTopPadding = 8;
const CGFloat kMenuBottomPadding = 60;

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

// The corner radius of the menu's elements.
const CGFloat kButtonsCornerRadius = 16;

// The padding between the image and text of the large button.
const CGFloat kLargeButtonImagePadding = 8;

// The corner radius for the reader mode icon.
const CGFloat kReaderModeIconCornerRadius = 6;

// The spacing for the reader mode content stack.
const CGFloat kReaderModeContentStackSpacing = 12;

// The size of the reader mode icon container.
const CGFloat kIconContainerSize = 32;

// The horizontal padding for the reader mode content stack.
const CGFloat kReaderModeContentStackHorizontalPadding = 16;

// The vertical padding for the reader mode content stack.
const CGFloat kReaderModeContentStackVerticalPadding = 10;

// The minimum height for feature rows in the Page Action Menu.
const CGFloat kFeatureRowHeight = 56;

// The spacing between icon and content in feature rows.
const CGFloat kFeatureRowContentSpacing = 12;

// The horizontal padding within feature rows.
const CGFloat kFeatureRowHorizontalPadding = 16;

// The vertical padding within feature rows.
const CGFloat kFeatureRowVerticalPadding = 12;

// The width for the veritical feature row divider.
const CGFloat kDividerWidth = 1.0;

}  // namespace

@interface PageActionMenuViewController ()

// Label of the Reader mode options button. Lazily created.
@property(nonatomic, strong) UILabel* readerModeOptionsButtonSubtitleLabel;

@end

@implementation PageActionMenuViewController {
  // Scroll view containing the menu's main content.
  UIScrollView* _scrollView;

  // Stack view containing the menu's main content.
  UIStackView* _contentStackView;

  // The entry point for Ask Gemini.
  UIButton* _BWGButton;

  // The entry point for the Lens overlay.
  UIButton* _lensButton;

  // Stack view containing dynamically generated feature rows.
  UIStackView* _featureRowsStackView;

  // Horizontal stack view containing the side-by-side small buttons.
  UIStackView* _smallButtonsStackView;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  [self configureCornerRadius];
  [self setupBlurredBackground];
  [self setupNavigationBar];
  [self setupScrollView];
  [self setupContent];
  [self setupConstraints];
  [self setupTraitChangeHandling];
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  __weak __typeof(self) weakSelf = self;
  [weakSelf.sheetPresentationController animateChanges:^{
    [weakSelf.sheetPresentationController invalidateDetents];
  }];
}

#pragma mark - Public

- (CGFloat)resolveDetentValueForSheetPresentation:
    (id<UISheetPresentationControllerDetentResolutionContext>)context {
  CGFloat bottomPaddingAboveSafeArea =
      kMenuBottomPadding - self.view.safeAreaInsets.bottom;
  return self.navigationController.navigationBar.frame.size.height +
         [_contentStackView
             systemLayoutSizeFittingSize:UILayoutFittingCompressedSize]
             .height +
         kMenuTopPadding + bottomPaddingAboveSafeArea;
}

#pragma mark - ReaderModeOptionsConsumer

- (void)setSelectedFontFamily:(dom_distiller::mojom::FontFamily)fontFamily {
  std::u16string fontFamilyString;
  switch (fontFamily) {
    case dom_distiller::mojom::FontFamily::kSansSerif:
      fontFamilyString = l10n_util::GetStringUTF16(
          IDS_IOS_READER_MODE_OPTIONS_FONT_FAMILY_SANS_SERIF_LABEL);
      break;
    case dom_distiller::mojom::FontFamily::kSerif:
      fontFamilyString = l10n_util::GetStringUTF16(
          IDS_IOS_READER_MODE_OPTIONS_FONT_FAMILY_SERIF_LABEL);
      break;
    case dom_distiller::mojom::FontFamily::kMonospace:
      fontFamilyString = l10n_util::GetStringUTF16(
          IDS_IOS_READER_MODE_OPTIONS_FONT_FAMILY_MONOSPACE_LABEL);
      break;
  }
  self.readerModeOptionsButtonSubtitleLabel.text = l10n_util::GetNSStringF(
      IDS_IOS_AI_HUB_READER_MODE_OPTIONS_FONT_LABEL, fontFamilyString);
}

- (void)setSelectedTheme:(dom_distiller::mojom::Theme)theme {
  // Nothing to do.
}

- (void)setDecreaseFontSizeButtonEnabled:(BOOL)enabled {
  // Nothing to do.
}

- (void)setIncreaseFontSizeButtonEnabled:(BOOL)enabled {
  // Nothing to do.
}

- (void)announceFontSizeMultiplier:(CGFloat)multiplier {
  // Nothing to do.
}

#pragma mark - PageActionMenuConsumer

- (void)pageLoadStatusChanged {
  [self updateButton:_BWGButton enabled:[self.mutator isGeminiAvailable]];
}

#pragma mark - Private

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

  self.title = l10n_util::GetNSString(IDS_IOS_PAGE_ACTION_MENU_TITLE);

  // Add the dismiss button.
  UIBarButtonItem* dismissButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemClose
                           target:self
                           action:@selector(dismissPageActionMenu)];
  dismissButton.accessibilityIdentifier =
      kAIHubDismissButtonAccessibilityIdentifier;
  self.navigationItem.rightBarButtonItem = dismissButton;
}

// Creates a stack view with Reader mode buttons when it is active.
- (UIView*)createReaderModeActiveSection {
  UIStackView* horizontalStackView = [[UIStackView alloc] init];
  horizontalStackView.axis = UILayoutConstraintAxisHorizontal;
  horizontalStackView.alignment = UIStackViewAlignmentFill;
  horizontalStackView.distribution = UIStackViewDistributionFill;
  horizontalStackView.translatesAutoresizingMaskIntoConstraints = NO;
  horizontalStackView.clipsToBounds = YES;
  horizontalStackView.backgroundColor =
      [[UIColor colorNamed:kGroupedSecondaryBackgroundColor]
          colorWithAlphaComponent:kSmallButtonOpacity];
  horizontalStackView.layer.cornerRadius = kButtonsCornerRadius;

  // Leading button shows Reader mode options.
  UIButton* readerModeOptionsButton = [self createReaderModeOptionsButton];

  // Trailing button hides Reader mode.
  UIButton* hideReaderModeButton = [self createHideReaderModeButton];

  // Divider
  UIView* divider = [[UIView alloc] init];
  divider.backgroundColor = [UIColor colorNamed:kSeparatorColor];
  divider.translatesAutoresizingMaskIntoConstraints = NO;

  [horizontalStackView addArrangedSubview:readerModeOptionsButton];
  [horizontalStackView addArrangedSubview:divider];
  [horizontalStackView addArrangedSubview:hideReaderModeButton];

  [NSLayoutConstraint activateConstraints:@[
    [divider.widthAnchor constraintEqualToConstant:kDividerWidth],
    [horizontalStackView.heightAnchor
        constraintGreaterThanOrEqualToConstant:kSmallButtonHeight],
  ]];

  return horizontalStackView;
}

// Creates the Reader mode options button.
- (UIButton*)createReaderModeOptionsButton {
  UIStackView* buttonContentStack = [[UIStackView alloc] init];
  buttonContentStack.translatesAutoresizingMaskIntoConstraints = NO;
  buttonContentStack.axis = UILayoutConstraintAxisHorizontal;
  buttonContentStack.alignment = UIStackViewAlignmentCenter;
  buttonContentStack.spacing = kReaderModeContentStackSpacing;
  buttonContentStack.userInteractionEnabled = NO;

  // Add leading icon.
  UIView* leadingIconContainer = [self
      createIconWithImage:DefaultSymbolWithPointSize(GetReaderModeSymbolName(),
                                                     kSmallButtonIconSize)];
  [buttonContentStack addArrangedSubview:leadingIconContainer];

  // Add stack with title and subtitle.
  UILabel* titleLabel = [[UILabel alloc] init];
  titleLabel.text = l10n_util::GetNSString(IDS_IOS_AI_HUB_READER_MODE_LABEL);
  titleLabel.font = PreferredFontForTextStyle(UIFontTextStyleSubheadline,
                                              UIFontWeightRegular);
  titleLabel.adjustsFontForContentSizeCategory = YES;
  titleLabel.maximumContentSizeCategory = UIContentSizeCategoryExtraExtraLarge;
  titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  UIStackView* labelStack = [[UIStackView alloc] initWithArrangedSubviews:@[
    titleLabel, self.readerModeOptionsButtonSubtitleLabel
  ]];
  labelStack.axis = UILayoutConstraintAxisVertical;
  labelStack.alignment = UIStackViewAlignmentLeading;
  [buttonContentStack addArrangedSubview:labelStack];

  // Add trailing icon.
  UIImageView* trailingIcon = [[UIImageView alloc]
      initWithImage:DefaultSymbolWithPointSize(kChevronRightSymbol,
                                               kSmallButtonIconSize)];
  trailingIcon.translatesAutoresizingMaskIntoConstraints = NO;
  trailingIcon.tintColor = [UIColor colorNamed:kTextSecondaryColor];
  [buttonContentStack addArrangedSubview:trailingIcon];

  // Create button with `buttonContentStack` as content.
  UIButton* button = [UIButton buttonWithType:UIButtonTypeCustom];
  button.translatesAutoresizingMaskIntoConstraints = NO;
  button.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_AI_HUB_READER_MODE_OPTIONS_BUTTON_TITLE);
  [button addTarget:self
                action:@selector(handleReaderModeOptionsTapped:)
      forControlEvents:UIControlEventTouchUpInside];
  [button addSubview:buttonContentStack];

  // Add constraints.
  AddSameConstraintsWithInsets(
      buttonContentStack, button,
      NSDirectionalEdgeInsetsMake(kReaderModeContentStackVerticalPadding,
                                  kReaderModeContentStackHorizontalPadding,
                                  kReaderModeContentStackVerticalPadding,
                                  kReaderModeContentStackHorizontalPadding));
  AddSizeConstraints(trailingIcon, trailingIcon.intrinsicContentSize);

  return button;
}

- (UILabel*)readerModeOptionsButtonSubtitleLabel {
  if (_readerModeOptionsButtonSubtitleLabel) {
    return _readerModeOptionsButtonSubtitleLabel;
  }

  UILabel* label = [[UILabel alloc] init];
  label.font =
      PreferredFontForTextStyle(UIFontTextStyleFootnote, UIFontWeightRegular);
  label.textColor = [UIColor colorNamed:kTextSecondaryColor];
  label.lineBreakMode = NSLineBreakByWordWrapping;
  label.numberOfLines = 0;
  label.adjustsFontForContentSizeCategory = YES;
  label.maximumContentSizeCategory = UIContentSizeCategoryExtraExtraLarge;

  _readerModeOptionsButtonSubtitleLabel = label;
  return _readerModeOptionsButtonSubtitleLabel;
}

// Creates the button to hide Reader mode.
- (UIButton*)createHideReaderModeButton {
  UIButtonConfiguration* configuration =
      [UIButtonConfiguration plainButtonConfiguration];
  UIFont* fontAttribute =
      PreferredFontForTextStyle(UIFontTextStyleSubheadline, UIFontWeightMedium);
  NSDictionary* attributes = @{
    NSFontAttributeName : fontAttribute,
    NSForegroundColorAttributeName : [UIColor colorNamed:kBlue600Color]
  };
  NSMutableAttributedString* attributedTitle =
      [[NSMutableAttributedString alloc]
          initWithString:l10n_util::GetNSString(
                             IDS_IOS_AI_HUB_HIDE_BUTTON_LABEL)
              attributes:attributes];
  configuration.attributedTitle = attributedTitle;

  UIButton* button = [UIButton buttonWithConfiguration:configuration
                                         primaryAction:nil];
  button.translatesAutoresizingMaskIntoConstraints = NO;
  button.maximumContentSizeCategory = UIContentSizeCategoryExtraExtraLarge;
  [button addTarget:self
                action:@selector(handleReaderModeTapped:)
      forControlEvents:UIControlEventTouchUpInside];

  [button
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisHorizontal];
  [button setContentHuggingPriority:UILayoutPriorityRequired
                            forAxis:UILayoutConstraintAxisHorizontal];

  return button;
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
  _lensButton = [self
      createSmallButtonWithIcon:CustomSymbolWithPointSize(kCameraLensSymbol,
                                                          kSmallButtonIconSize)
                          title:l10n_util::GetNSString(
                                    IDS_IOS_AI_HUB_LENS_LABEL)
                        enabled:[self.mutator isLensAvailableForTraitCollection:
                                                  self.traitCollection]
        accessibilityIdentifier:kAIHubLensButtonAccessibilityIdentifier];
  [_lensButton addTarget:self
                  action:@selector(handleLensEntryPointTapped:)
        forControlEvents:UIControlEventTouchUpInside];
  [stackView addArrangedSubview:_lensButton];

  if (IsReaderModeAvailable() && ![self.mutator isReaderModeActive]) {
    UIImage* readerModeImage = DefaultSymbolWithPointSize(
        GetReaderModeSymbolName(), kSmallButtonIconSize);

    NSString* readerModeLabelText =
        l10n_util::GetNSString(IDS_IOS_AI_HUB_READER_MODE_LABEL);

    UIButton* readerModeButton =
        [self createSmallButtonWithIcon:readerModeImage
                                  title:readerModeLabelText
                                enabled:[self.mutator isReaderModeAvailable]
                accessibilityIdentifier:
                    kAIHubReaderModeButtonAccessibilityIdentifier];
    [readerModeButton addTarget:self
                         action:@selector(handleReaderModeTapped:)
               forControlEvents:UIControlEventTouchUpInside];
    [stackView addArrangedSubview:readerModeButton];
  } else {
    _BWGButton = [self
        createSmallButtonWithIcon:[self askGeminiIcon]
                            title:l10n_util::GetNSString(
                                      IDS_IOS_AI_HUB_GEMINI_LABEL)
                          enabled:[self.mutator isGeminiAvailable]
          accessibilityIdentifier:kAIHubAskGeminiButtonAccessibilityIdentifier];
    [_BWGButton addTarget:self
                   action:@selector(handleBWGTapped:)
         forControlEvents:UIControlEventTouchUpInside];
    [stackView addArrangedSubview:_BWGButton];
  }

  return stackView;
}

// Creates a large button for the BWG entry point.
- (UIButton*)createBWGButton {
  ChromeButton* button =
      [[ChromeButton alloc] initWithStyle:ChromeButtonStylePrimary];

  // Create the background config.
  UIBackgroundConfiguration* backgroundConfig = button.configuration.background;
  backgroundConfig.backgroundColor = [UIColor colorNamed:kBlue600Color];

  // Create the button config.
  UIButtonConfiguration* buttonConfiguration = button.configuration;
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
  button.configuration = buttonConfiguration;

  button.translatesAutoresizingMaskIntoConstraints = NO;
  button.accessibilityIdentifier = kAIHubAskGeminiButtonAccessibilityIdentifier;
  [button addTarget:self
                action:@selector(handleBWGTapped:)
      forControlEvents:UIControlEventTouchUpInside];

  [self updateButton:button enabled:[self.mutator isGeminiAvailable]];

  return button;
}

// Creates and returns a small button with an icon and a title for the label. If
// the button is not `enabled`, a greyed out UI is shown and the tap target is
// disabled.
- (UIButton*)createSmallButtonWithIcon:(UIImage*)image
                                 title:(NSString*)title
                               enabled:(BOOL)enabled
               accessibilityIdentifier:(NSString*)accessibilityIdentifier {
  // Create the background config.
  UIBackgroundConfiguration* backgroundConfig =
      [UIBackgroundConfiguration clearConfiguration];
  backgroundConfig.backgroundColor =
      [[UIColor colorNamed:kGroupedSecondaryBackgroundColor]
          colorWithAlphaComponent:kSmallButtonOpacity];
  backgroundConfig.cornerRadius = kButtonsCornerRadius;

  // Create the button config.
  UIButtonConfiguration* buttonConfiguration =
      [UIButtonConfiguration filledButtonConfiguration];
  buttonConfiguration.titleAlignment =
      UIButtonConfigurationTitleAlignmentCenter;
  buttonConfiguration.image = image;
  buttonConfiguration.imagePlacement = NSDirectionalRectEdgeTop;
  buttonConfiguration.imagePadding = kSmallButtonImagePadding;
  buttonConfiguration.baseForegroundColor = [UIColor colorNamed:kBlue600Color];
  buttonConfiguration.background = backgroundConfig;
  buttonConfiguration.contentInsets = NSDirectionalEdgeInsetsMake(
      kSmallButtonPadding, 0, kSmallButtonPadding, 0);

  // Set the font and text color as attributes.
  NSMutableParagraphStyle* paragraphStyle =
      [[NSMutableParagraphStyle alloc] init];
  paragraphStyle.alignment = NSTextAlignmentCenter;
  NSMutableDictionary* titleAttributes = [[NSMutableDictionary alloc] init];
  [titleAttributes
      setObject:PreferredFontForTextStyle(UIFontTextStyleSubheadline,
                                          UIFontWeightRegular)
         forKey:NSFontAttributeName];
  [titleAttributes setObject:paragraphStyle
                      forKey:NSParagraphStyleAttributeName];
  NSMutableAttributedString* string =
      [[NSMutableAttributedString alloc] initWithString:title];
  NSRange titleRange = NSMakeRange(0, string.length);
  [string addAttributes:titleAttributes range:NSMakeRange(0, string.length)];
  [string addAttribute:NSForegroundColorAttributeName
                 value:[UIColor colorNamed:kTextPrimaryColor]
                 range:titleRange];
  buttonConfiguration.attributedTitle = string;

  UIButton* button = [UIButton buttonWithConfiguration:buttonConfiguration
                                         primaryAction:nil];
  button.translatesAutoresizingMaskIntoConstraints = NO;
  button.accessibilityIdentifier = accessibilityIdentifier;

  [self updateButton:button enabled:enabled];

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
  __weak __typeof(self.readerModeHandler) weakReaderModeHandler =
      self.readerModeHandler;
  if ([self.mutator isReaderModeActive]) {
    [self.pageActionMenuHandler dismissPageActionMenuWithCompletion:^{
      [weakReaderModeHandler hideReaderMode];
    }];
  } else {
    [self.pageActionMenuHandler dismissPageActionMenuWithCompletion:^{
      [weakReaderModeHandler
          showReaderModeFromAccessPoint:ReaderModeAccessPoint::kAIHub];
    }];
  }
}

// Navigates to the Reader mode options.
- (void)handleReaderModeOptionsTapped:(UIButton*)button {
  RecordAIHubAction(IOSAIHubAction::kReaderModeOptions);
  [self.delegate viewControllerDidTapReaderModeOptionsButton:self];
}

#pragma mark - Private

// Configures the correct preferred corner radius given the form factor.
- (void)configureCornerRadius {
  CGFloat preferredCornerRadius = kMenuCornerRadius;
  if (@available(iOS 26, *)) {
    preferredCornerRadius = kMenuCornerRadiusIOS26;
  }

  CGFloat cornerRadius = IsSplitToolbarMode(self.presentingViewController)
                             ? preferredCornerRadius
                             : UISheetPresentationControllerAutomaticDimension;
  self.navigationController.sheetPresentationController.preferredCornerRadius =
      cornerRadius;
}

// Updates the availability of the Lens entry point.
- (void)updateLensAvailability:(UITraitCollection*)traitCollection {
  [self updateButton:_lensButton
             enabled:[self.mutator
                         isLensAvailableForTraitCollection:traitCollection]];
}

// Updates a `button` for whether it's `enabled`.
- (void)updateButton:(UIButton*)button enabled:(BOOL)enabled {
  // Only disable user interaction to not affect the tint color of the title and
  // image.
  button.userInteractionEnabled = enabled;
  button.alpha = enabled ? 1.0 : 0.5;
  button.enabled = enabled;
}


// Sets up blurred background effect for the Page Action Menu.
- (void)setupBlurredBackground {
  UIBlurEffect* blurEffect =
      [UIBlurEffect effectWithStyle:UIBlurEffectStyleSystemThickMaterial];
  UIVisualEffectView* blurEffectView =
      [[UIVisualEffectView alloc] initWithEffect:blurEffect];
  blurEffectView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:blurEffectView];
  AddSameConstraints(blurEffectView, self.view);
}

// Configures scroll view and content stack view for the menu layout.
- (void)setupScrollView {
  _scrollView = [[UIScrollView alloc] init];
  _scrollView.translatesAutoresizingMaskIntoConstraints = NO;
  _scrollView.showsVerticalScrollIndicator = NO;
  [self.view addSubview:_scrollView];

  _contentStackView = [[UIStackView alloc] init];
  _contentStackView.axis = UILayoutConstraintAxisVertical;
  _contentStackView.translatesAutoresizingMaskIntoConstraints = NO;
  [_scrollView addSubview:_contentStackView];
}

// Builds the main content sections including Reader Mode and feature rows.
- (void)setupContent {
  // Reader mode section.
  if ([self.mutator isReaderModeActive]) {
    UIView* originalReaderModeSection = [self createReaderModeActiveSection];
    [_contentStackView addArrangedSubview:originalReaderModeSection];
    [_contentStackView setCustomSpacing:kStackViewMargins
                              afterView:originalReaderModeSection];
  }

  // Create dedicated feature rows container.
  if (IsProactiveSuggestionsFrameworkEnabled()) {
    _featureRowsStackView = [[UIStackView alloc] init];
    _featureRowsStackView.axis = UILayoutConstraintAxisVertical;
    _featureRowsStackView.translatesAutoresizingMaskIntoConstraints = NO;
    [_contentStackView addArrangedSubview:_featureRowsStackView];

    [self rebuildFeatureRows];
  }

  // Horizontal stack view for the 2 side-by-side buttons.
  _smallButtonsStackView = [self createSmallButtonsStackView];
  [_contentStackView addArrangedSubview:_smallButtonsStackView];
  [_contentStackView setCustomSpacing:kStackViewMargins
                            afterView:_smallButtonsStackView];

  // If Reader Mode is available but inactive, we use a 3-button UI. Otherwise,
  // we just show the `buttonsStackView`, with an additional Reader mode section
  // (above) if Reader mode is available and active.
  if (IsReaderModeAvailable() && ![self.mutator isReaderModeActive]) {
    // Adds the large Gemini entry point button.
    _BWGButton = [self createBWGButton];
    [_contentStackView addArrangedSubview:_BWGButton];

    [NSLayoutConstraint activateConstraints:@[
      [_BWGButton.heightAnchor
          constraintGreaterThanOrEqualToConstant:kLargeButtonHeight],
    ]];
  }
}

// Sets up Auto Layout constraints for scroll view and content stack.
- (void)setupConstraints {
  [NSLayoutConstraint activateConstraints:@[
    // Scroll view constraints.
    [_scrollView.topAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor
                       constant:kMenuTopPadding],
    [_scrollView.leadingAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leadingAnchor
                       constant:kMenuSidePadding],
    [_scrollView.trailingAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.trailingAnchor
                       constant:-kMenuSidePadding],
    [_scrollView.bottomAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.bottomAnchor],

    // Content stack view constraints.
    [_contentStackView.topAnchor constraintEqualToAnchor:_scrollView.topAnchor],
    [_contentStackView.leadingAnchor
        constraintEqualToAnchor:_scrollView.leadingAnchor],
    [_contentStackView.trailingAnchor
        constraintEqualToAnchor:_scrollView.trailingAnchor],
    [_contentStackView.bottomAnchor
        constraintEqualToAnchor:_scrollView.bottomAnchor],
    [_contentStackView.widthAnchor
        constraintEqualToAnchor:_scrollView.widthAnchor],
    [_smallButtonsStackView.heightAnchor
        constraintGreaterThanOrEqualToConstant:kSmallButtonHeight],
  ]];
}

// Rebuilds feature rows based on current availability state.
- (void)rebuildFeatureRows {
  CHECK(IsProactiveSuggestionsFrameworkEnabled());

  // Clear existing feature rows.
  for (UIView* view in _featureRowsStackView.arrangedSubviews) {
    [_featureRowsStackView removeArrangedSubview:view];
    [view removeFromSuperview];
  }

  // Get active features from mediator.
  NSArray<PageActionMenuFeature*>* activeFeatures =
      [self.mutator activeFeatures];

  UIView* lastView = nil;

  for (PageActionMenuFeature* feature in activeFeatures) {
    UIView* featureRow = [self createFeatureRowWithData:feature];
    [_featureRowsStackView addArrangedSubview:featureRow];
    [_featureRowsStackView setCustomSpacing:kFeatureRowVerticalPadding
                                  afterView:featureRow];
    lastView = featureRow;
  }

  // Add permission explanation if needed.
  if ([self hasPermissionFeatures:activeFeatures]) {
    UILabel* explanation = [self createPermissionExplanationLabel];
    [_featureRowsStackView addArrangedSubview:explanation];
    lastView = explanation;
  }

  if (lastView) {
    UIView* divider =
        [self createDividerWithOrientation:UILayoutConstraintAxisHorizontal];
    [_featureRowsStackView addArrangedSubview:divider];
    [_featureRowsStackView setCustomSpacing:kStackViewMargins
                                  afterView:lastView];
    [_featureRowsStackView setCustomSpacing:kStackViewMargins
                                  afterView:divider];
  }

  if (_featureRowsStackView.arrangedSubviews.count > 0) {
    [_contentStackView setCustomSpacing:kStackViewMargins
                              afterView:_featureRowsStackView];
  }
}

// Creates a divider line with specified orientation.
- (UIView*)createDividerWithOrientation:(UILayoutConstraintAxis)orientation {
  UIView* divider = [[UIView alloc] init];
  divider.backgroundColor = [UIColor colorNamed:kSeparatorColor];
  divider.translatesAutoresizingMaskIntoConstraints = NO;

  if (orientation == UILayoutConstraintAxisHorizontal) {
    // Horizontal divider.
    [divider.heightAnchor constraintEqualToConstant:kDividerWidth].active = YES;
  } else {
    // Vertical divider.
    [divider.widthAnchor constraintEqualToConstant:kDividerWidth].active = YES;
  }

  return divider;
}

// Creates a navigation chevron icon.
- (UIImageView*)createNavigationChevron {
  UIImageView* chevronIcon = [[UIImageView alloc]
      initWithImage:DefaultSymbolWithPointSize(kChevronRightSymbol,
                                               kSmallButtonIconSize)];
  chevronIcon.translatesAutoresizingMaskIntoConstraints = NO;
  chevronIcon.tintColor = [UIColor colorNamed:kTextQuaternaryColor];
  return chevronIcon;
}

// Registers for trait collection changes to handle device orientation updates.
- (void)setupTraitChangeHandling {
  __weak PageActionMenuViewController* weakSelf = self;
  NSArray<UITrait>* traits = TraitCollectionSetForTraits(
      @[ UITraitHorizontalSizeClass.class, UITraitVerticalSizeClass.class ]);
  [self registerForTraitChanges:traits
                    withHandler:^(id<UITraitEnvironment> traitEnvironment,
                                  UITraitCollection* previousCollection) {
                      [weakSelf updateLensAvailability:traitEnvironment
                                                           .traitCollection];
                    }];
}

// Creates UI view for a single feature row based on the provided feature data.
- (UIView*)createFeatureRowWithData:(PageActionMenuFeature*)feature {
  UIView* containerView = [[UIView alloc] init];
  containerView.translatesAutoresizingMaskIntoConstraints = NO;
  containerView.backgroundColor =
      [[UIColor colorNamed:kGroupedSecondaryBackgroundColor]
          colorWithAlphaComponent:kSmallButtonOpacity];
  containerView.layer.cornerRadius = kButtonsCornerRadius;

  // Handle split action.
  if (feature.actionType == PageActionMenuSettingsAction) {
    return [self createSplitActionRowWithData:feature
                                containerView:containerView];
  }
  UIStackView* stackView = [[UIStackView alloc] init];
  stackView.axis = UILayoutConstraintAxisHorizontal;
  stackView.alignment = UIStackViewAlignmentCenter;
  stackView.spacing = kFeatureRowContentSpacing;
  stackView.translatesAutoresizingMaskIntoConstraints = NO;
  [containerView addSubview:stackView];

  UIView* iconView = [self createIconWithImage:feature.icon];
  [stackView addArrangedSubview:iconView];

  UIStackView* labelsStack = [[UIStackView alloc] init];
  labelsStack.axis = UILayoutConstraintAxisVertical;
  labelsStack.alignment = UIStackViewAlignmentLeading;

  UILabel* titleLabel = [[UILabel alloc] init];
  titleLabel.text = feature.title;
  titleLabel.font = PreferredFontForTextStyle(UIFontTextStyleSubheadline,
                                              UIFontWeightRegular);
  titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  [labelsStack addArrangedSubview:titleLabel];

  if (feature.subtitle && feature.subtitle.length > 0) {
    UILabel* subtitleLabel = [[UILabel alloc] init];
    subtitleLabel.text = feature.subtitle;
    subtitleLabel.font =
        PreferredFontForTextStyle(UIFontTextStyleFootnote, UIFontWeightRegular);
    subtitleLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
    [labelsStack addArrangedSubview:subtitleLabel];
  }

  [stackView addArrangedSubview:labelsStack];

  switch (feature.actionType) {
    case PageActionMenuToggleAction: {
      UISwitch* toggleSwitch = [[UISwitch alloc] init];
      toggleSwitch.on = feature.toggleState;
      toggleSwitch.tag = feature.featureType;
      [toggleSwitch addTarget:self
                       action:@selector(handleFeatureToggle:)
             forControlEvents:UIControlEventValueChanged];

      if (feature.featureType == PageActionMenuCameraPermission ||
          feature.featureType == PageActionMenuMicrophonePermission) {
        [self updateAccessibilityLabelForSwitch:toggleSwitch
                                    featureType:feature.featureType];
      }

      [stackView addArrangedSubview:toggleSwitch];
      break;
    }
    case PageActionMenuButtonAction: {
      if (feature.actionText && feature.actionText.length > 0) {
        UIButton* actionButton = [UIButton buttonWithType:UIButtonTypeSystem];
        [actionButton setTitle:feature.actionText
                      forState:UIControlStateNormal];
        actionButton.titleLabel.font = PreferredFontForTextStyle(
            UIFontTextStyleSubheadline, UIFontWeightMedium);
        [actionButton setTitleColor:[UIColor colorNamed:kBlue600Color]
                           forState:UIControlStateNormal];
        actionButton.tag = feature.featureType;
        [actionButton addTarget:self
                         action:@selector(handleFeatureButton:)
               forControlEvents:UIControlEventTouchUpInside];
        [stackView addArrangedSubview:actionButton];

        // Add chevron for price tracking.
        if (feature.featureType == PageActionMenuPriceTracking) {
          UIImageView* chevronIcon = [self createNavigationChevron];
          [stackView addArrangedSubview:chevronIcon];
          actionButton.accessibilityLabel = l10n_util::GetNSString(
              IDS_IOS_AI_HUB_OPEN_PRICE_TRACKING_ACCESSIBILITY_LABEL);
        } else if (feature.featureType == PageActionMenuPopupBlocker) {
          actionButton.accessibilityLabel = l10n_util::GetNSString(
              IDS_IOS_AI_HUB_ALWAYS_SHOW_POPUPS_ACCESSIBILITY_LABEL);
        }
      }
      break;
    }
    case PageActionMenuSettingsAction:
      // Already handled above, should never reach here.
      break;
  }

  BOOL hasNavigation = (feature.actionType == PageActionMenuSettingsAction ||
                        feature.featureType == PageActionMenuPriceTracking);
  CGFloat rowHeight = hasNavigation ? kSmallButtonHeight : kFeatureRowHeight;

  [NSLayoutConstraint activateConstraints:@[
    [containerView.heightAnchor
        constraintGreaterThanOrEqualToConstant:rowHeight],

    [stackView.leadingAnchor
        constraintEqualToAnchor:containerView.leadingAnchor
                       constant:kFeatureRowHorizontalPadding],
    [stackView.trailingAnchor
        constraintEqualToAnchor:containerView.trailingAnchor
                       constant:-kFeatureRowHorizontalPadding],
    [stackView.topAnchor constraintEqualToAnchor:containerView.topAnchor
                                        constant:kFeatureRowVerticalPadding],
    [stackView.bottomAnchor
        constraintEqualToAnchor:containerView.bottomAnchor
                       constant:-kFeatureRowVerticalPadding],
  ]];

  [labelsStack setContentHuggingPriority:UILayoutPriorityDefaultLow
                                 forAxis:UILayoutConstraintAxisHorizontal];

  return containerView;
}

// Creates explanation label for site-specific permission context.
- (UILabel*)createPermissionExplanationLabel {
  CHECK(IsProactiveSuggestionsFrameworkEnabled());
  UILabel* label = [[UILabel alloc] init];
  NSString* domain = [self.mutator currentSiteDomain];
  label.text =
      l10n_util::GetNSStringF(IDS_IOS_AI_HUB_PERMISSION_SITE_EXPLANATION,
                              base::SysNSStringToUTF16(domain));
  label.font =
      PreferredFontForTextStyle(UIFontTextStyleFootnote, UIFontWeightRegular);
  label.textColor = [UIColor colorNamed:kTextSecondaryColor];
  label.numberOfLines = 0;
  label.textAlignment = NSTextAlignmentLeft;
  return label;
}

// Returns true if any features in the array are permission-based features.
- (BOOL)hasPermissionFeatures:(NSArray<PageActionMenuFeature*>*)features {
  for (PageActionMenuFeature* feature in features) {
    if (feature.featureType == PageActionMenuCameraPermission ||
        feature.featureType == PageActionMenuMicrophonePermission) {
      return YES;
    }
  }
  return NO;
}

// Updates the accessibility label for a permission toggle switch.
- (void)updateAccessibilityLabelForSwitch:(UISwitch*)toggleSwitch
                              featureType:
                                  (PageActionMenuFeatureType)featureType {
  if (featureType == PageActionMenuCameraPermission) {
    toggleSwitch.accessibilityLabel =
        toggleSwitch.isOn
            ? l10n_util::GetNSString(
                  IDS_IOS_AI_HUB_TURN_OFF_CAMERA_ACCESSIBILITY_LABEL)
            : l10n_util::GetNSString(
                  IDS_IOS_AI_HUB_TURN_ON_CAMERA_ACCESSIBILITY_LABEL);
  } else if (featureType == PageActionMenuMicrophonePermission) {
    toggleSwitch.accessibilityLabel =
        toggleSwitch.isOn
            ? l10n_util::GetNSString(
                  IDS_IOS_AI_HUB_TURN_OFF_MICROPHONE_ACCESSIBILITY_LABEL)
            : l10n_util::GetNSString(
                  IDS_IOS_AI_HUB_TURN_ON_MICROPHONE_ACCESSIBILITY_LABEL);
  }
}

// Handles toggle switch changes for permission-based features.
- (void)handleFeatureToggle:(UISwitch*)toggleSwitch {
  CHECK(IsProactiveSuggestionsFrameworkEnabled());
  PageActionMenuFeatureType featureType =
      (PageActionMenuFeatureType)toggleSwitch.tag;

  [self.mutator updatePermission:toggleSwitch.isOn forFeature:featureType];
  [self updateAccessibilityLabelForSwitch:toggleSwitch featureType:featureType];
}

// Handles button taps for action-based features like translate and popup
// blocker.
- (void)handleFeatureButton:(UIButton*)button {
  CHECK(IsProactiveSuggestionsFrameworkEnabled());
  PageActionMenuFeatureType featureType = (PageActionMenuFeatureType)button.tag;

  switch (featureType) {
    case PageActionMenuTranslate:
      [self.mutator revertTranslation];
      [self.pageActionMenuHandler dismissPageActionMenuWithCompletion:nil];
      break;
    case PageActionMenuPopupBlocker:
      [self.mutator allowBlockedPopups];
      break;
    case PageActionMenuPriceTracking: {
      // Capture the mutator before dismissal.
      id<PageActionMenuMutator> mutator = self.mutator;
      [self.pageActionMenuHandler dismissPageActionMenuWithCompletion:^{
        [mutator openPriceInsightsPanel];
      }];
      break;
    }
    default:
      break;
  }
}

// Handles taps on the left side of split action feature rows.
- (void)handleFeatureRowTap:(UIButton*)sender {
  CHECK(IsProactiveSuggestionsFrameworkEnabled());
  PageActionMenuFeatureType featureType = (PageActionMenuFeatureType)sender.tag;

  switch (featureType) {
    case PageActionMenuTranslate: {
      // Call modal first, then dismiss.
      [self.mutator openTranslateOptions];
      [self.pageActionMenuHandler dismissPageActionMenuWithCompletion:nil];
      break;
    }
    default:
      break;
  }
}

// Creates the horizontal content stack with icon, labels, and chevron.
- (UIStackView*)createFeatureRowContentStackWithFeature:
    (PageActionMenuFeature*)feature {
  UIStackView* contentStack = [[UIStackView alloc] init];
  contentStack.translatesAutoresizingMaskIntoConstraints = NO;
  contentStack.axis = UILayoutConstraintAxisHorizontal;
  contentStack.alignment = UIStackViewAlignmentCenter;
  contentStack.spacing = kFeatureRowContentSpacing;
  contentStack.userInteractionEnabled = NO;

  UIView* iconView = [self createIconWithImage:feature.icon];

  [contentStack addArrangedSubview:iconView];

  UIStackView* labelsStack =
      [self createFeatureRowLabelsStackWithFeature:feature];
  [contentStack addArrangedSubview:labelsStack];

  UIImageView* chevronIcon = [self createNavigationChevron];
  [contentStack addArrangedSubview:chevronIcon];

  return contentStack;
}

// Creates the vertical stack containing labels for feature rows.
- (UIStackView*)createFeatureRowLabelsStackWithFeature:
    (PageActionMenuFeature*)feature {
  UIStackView* labelsStack = [[UIStackView alloc] init];
  labelsStack.axis = UILayoutConstraintAxisVertical;
  labelsStack.alignment = UIStackViewAlignmentLeading;

  UILabel* titleLabel = [[UILabel alloc] init];
  titleLabel.text = feature.title;
  titleLabel.font = PreferredFontForTextStyle(UIFontTextStyleSubheadline,
                                              UIFontWeightRegular);
  titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  [labelsStack addArrangedSubview:titleLabel];

  if (feature.subtitle && feature.subtitle.length > 0) {
    UILabel* subtitleLabel = [[UILabel alloc] init];
    subtitleLabel.text = feature.subtitle;
    subtitleLabel.font =
        PreferredFontForTextStyle(UIFontTextStyleFootnote, UIFontWeightRegular);
    subtitleLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
    [labelsStack addArrangedSubview:subtitleLabel];
  }

  return labelsStack;
}

// Creates the trailing action button for the right side of split feature rows.
- (UIButton*)createTrailingButtonWithFeature:(PageActionMenuFeature*)feature {
  if (!feature.actionText || feature.actionText.length == 0) {
    return nil;
  }

  UIButtonConfiguration* configuration =
      [UIButtonConfiguration plainButtonConfiguration];
  UIFont* fontAttribute =
      PreferredFontForTextStyle(UIFontTextStyleSubheadline, UIFontWeightMedium);

  NSDictionary* attributes = @{
    NSFontAttributeName : fontAttribute,
    NSForegroundColorAttributeName : [UIColor colorNamed:kBlue600Color]
  };

  NSMutableAttributedString* attributedTitle =
      [[NSMutableAttributedString alloc] initWithString:feature.actionText
                                             attributes:attributes];
  configuration.attributedTitle = attributedTitle;

  UIButton* trailingButton = [UIButton buttonWithConfiguration:configuration
                                                 primaryAction:nil];
  trailingButton.translatesAutoresizingMaskIntoConstraints = NO;
  trailingButton.maximumContentSizeCategory =
      UIContentSizeCategoryExtraExtraLarge;
  trailingButton.tag = feature.featureType;

  [trailingButton addTarget:self
                     action:@selector(handleFeatureButton:)
           forControlEvents:UIControlEventTouchUpInside];

  [trailingButton
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisHorizontal];
  [trailingButton setContentHuggingPriority:UILayoutPriorityRequired
                                    forAxis:UILayoutConstraintAxisHorizontal];

  return trailingButton;
}

// Creates a feature row with split actions (left tap area, divider, right
// button).
- (UIView*)createSplitActionRowWithData:(PageActionMenuFeature*)feature
                          containerView:(UIView*)containerView {
  // Create horizontal stack.
  UIStackView* horizontalStackView = [[UIStackView alloc] init];
  horizontalStackView.axis = UILayoutConstraintAxisHorizontal;
  horizontalStackView.alignment = UIStackViewAlignmentFill;
  horizontalStackView.distribution = UIStackViewDistributionFill;
  horizontalStackView.translatesAutoresizingMaskIntoConstraints = NO;
  [containerView addSubview:horizontalStackView];

  // Create leading button.
  UIButton* leadingButton = [UIButton buttonWithType:UIButtonTypeCustom];
  leadingButton.translatesAutoresizingMaskIntoConstraints = NO;
  leadingButton.tag = feature.featureType;

  [leadingButton addTarget:self
                    action:@selector(handleFeatureRowTap:)
          forControlEvents:UIControlEventTouchUpInside];

  if (feature.featureType == PageActionMenuTranslate) {
    leadingButton.accessibilityLabel = l10n_util::GetNSString(
        IDS_IOS_AI_HUB_OPEN_TRANSLATE_SETTINGS_ACCESSIBILITY_LABEL);
  }

  UIStackView* buttonContentStack =
      [self createFeatureRowContentStackWithFeature:feature];
  [leadingButton addSubview:buttonContentStack];
  [horizontalStackView addArrangedSubview:leadingButton];

  // Add divider.
  UIView* divider =
      [self createDividerWithOrientation:UILayoutConstraintAxisVertical];
  [horizontalStackView addArrangedSubview:divider];

  // Add trailing button if needed.
  UIButton* trailingButton = [self createTrailingButtonWithFeature:feature];
  if (trailingButton) {
    [horizontalStackView addArrangedSubview:trailingButton];
  }

  // Set up constraints.
  [self setupSplitRowConstraints:containerView
             horizontalStackView:horizontalStackView
              buttonContentStack:buttonContentStack
                   leadingButton:leadingButton];

  return containerView;
}

// Sets up Auto Layout constraints for split action row container and content.
- (void)setupSplitRowConstraints:(UIView*)containerView
             horizontalStackView:(UIStackView*)horizontalStackView
              buttonContentStack:(UIStackView*)buttonContentStack
                   leadingButton:(UIButton*)leadingButton {
  [NSLayoutConstraint activateConstraints:@[
    [buttonContentStack.leadingAnchor
        constraintEqualToAnchor:leadingButton.leadingAnchor
                       constant:kFeatureRowHorizontalPadding],
    [buttonContentStack.trailingAnchor
        constraintEqualToAnchor:leadingButton.trailingAnchor
                       constant:-kFeatureRowHorizontalPadding],
    [buttonContentStack.topAnchor
        constraintEqualToAnchor:leadingButton.topAnchor
                       constant:kFeatureRowVerticalPadding],
    [buttonContentStack.bottomAnchor
        constraintEqualToAnchor:leadingButton.bottomAnchor
                       constant:-kFeatureRowVerticalPadding],

    [horizontalStackView.leadingAnchor
        constraintEqualToAnchor:containerView.leadingAnchor],
    [horizontalStackView.trailingAnchor
        constraintEqualToAnchor:containerView.trailingAnchor],
    [horizontalStackView.topAnchor
        constraintEqualToAnchor:containerView.topAnchor],
    [horizontalStackView.bottomAnchor
        constraintEqualToAnchor:containerView.bottomAnchor],

    [containerView.heightAnchor
        constraintGreaterThanOrEqualToConstant:kSmallButtonHeight],
  ]];

  UIStackView* labelsStack = buttonContentStack.arrangedSubviews[1];
  [labelsStack setContentHuggingPriority:UILayoutPriorityDefaultLow
                                 forAxis:UILayoutConstraintAxisHorizontal];
}

// Creates an icon with background container and rounded corners.
- (UIView*)createIconWithImage:(UIImage*)image {
  UIView* iconContainer = [[UIView alloc] init];
  iconContainer.translatesAutoresizingMaskIntoConstraints = NO;
  iconContainer.backgroundColor = [UIColor colorNamed:kBlueHaloColor];
  iconContainer.layer.cornerRadius = kReaderModeIconCornerRadius;

  UIImageView* icon = [[UIImageView alloc] initWithImage:image];
  icon.translatesAutoresizingMaskIntoConstraints = NO;
  icon.tintColor = [UIColor colorNamed:kBlue600Color];
  [iconContainer addSubview:icon];

  [NSLayoutConstraint activateConstraints:@[
    [iconContainer.widthAnchor constraintEqualToConstant:kIconContainerSize],
    [iconContainer.heightAnchor constraintEqualToConstant:kIconContainerSize],
    [icon.centerXAnchor constraintEqualToAnchor:iconContainer.centerXAnchor],
    [icon.centerYAnchor constraintEqualToAnchor:iconContainer.centerYAnchor],
  ]];

  return iconContainer;
}

@end
