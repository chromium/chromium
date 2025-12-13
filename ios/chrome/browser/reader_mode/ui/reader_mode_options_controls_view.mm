// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/ui/reader_mode_options_controls_view.h"

#import <UIKit/UIAccessibility.h>

#import "base/task/sequenced_task_runner.h"
#import "base/time/time.h"
#import "components/dom_distiller/core/mojom/distilled_page_prefs.mojom.h"
#import "ios/chrome/browser/reader_mode/ui/constants.h"
#import "ios/chrome/browser/reader_mode/ui/reader_mode_options_mutator.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {
constexpr CGFloat kCornerRadius = 12.0;
constexpr CGFloat kSpacing = 16.0;
constexpr CGFloat kHorizontalInsets = 12.0;
constexpr CGFloat kVerticalInsets = 16.0;
constexpr CGFloat kFontSizeStackSpacing = 1.0;
constexpr CGFloat kSmallFontSize = 17.0;
constexpr CGFloat kLargeFontSize = 24.0;
constexpr CGFloat kFirstRowHeight = 50.0;
constexpr CGFloat kSecondRowSpacing = 15.0;
constexpr CGFloat kSecondRowHeight = 48.0;
constexpr CGFloat kSelectedThemeBorderWidth = 3.0;
constexpr CGFloat kUnselectedThemeBorderWidth = 1.0;

// Delay for setting an utterance to be queued, it is required to ensure that
// standard announcements have already been started and thus would not interrupt
// the enqueued utterance.
constexpr base::TimeDelta kA11yAnnouncementQueueDelay = base::Seconds(2);

}  // namespace

@implementation ReaderModeOptionsControlsView {
  UIButton* _fontFamilyButton;
  UIButton* _decreaseFontSizeButton;
  UIButton* _increaseFontSizeButton;
  UIButton* _lightThemeButton;
  UIButton* _sepiaThemeButton;
  UIButton* _darkThemeButton;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _fontFamilyButton = [self createFontFamilyButton];
    _decreaseFontSizeButton = [self createDecreaseFontSizeButton];
    _increaseFontSizeButton = [self createIncreaseFontSizeButton];
    _lightThemeButton = [self createLightThemeButton];
    _sepiaThemeButton = [self createSepiaThemeButton];
    _darkThemeButton = [self createDarkThemeButton];

    UIStackView* mainVerticalStack = [[UIStackView alloc] init];
    mainVerticalStack.translatesAutoresizingMaskIntoConstraints = NO;
    mainVerticalStack.axis = UILayoutConstraintAxisVertical;
    mainVerticalStack.spacing = kSpacing;
    [self addSubview:mainVerticalStack];

    UIStackView* firstRowStack = [self createFirstRowStack];
    [mainVerticalStack addArrangedSubview:firstRowStack];

    UIStackView* secondRowStack = [self createSecondRowStack];
    [mainVerticalStack addArrangedSubview:secondRowStack];

    AddSameConstraintsWithInsets(
        mainVerticalStack, self,
        NSDirectionalEdgeInsetsMake(kVerticalInsets, kHorizontalInsets,
                                    kVerticalInsets, kHorizontalInsets));
  }
  return self;
}

#pragma mark - ReaderModeOptionsConsumer

- (void)setSelectedFontFamily:(dom_distiller::mojom::FontFamily)fontFamily {
  _fontFamilyButton.menu =
      [self createFontFamilyMenuWithSelectedFamily:fontFamily];
}

- (void)setSelectedTheme:(dom_distiller::mojom::Theme)theme {
  _lightThemeButton.configuration =
      [self createLightThemeButtonConfigurationSelected:
                theme == dom_distiller::mojom::Theme::kLight];
  _lightThemeButton.selected = theme == dom_distiller::mojom::Theme::kLight;
  _sepiaThemeButton.configuration =
      [self createSepiaThemeButtonConfigurationSelected:
                theme == dom_distiller::mojom::Theme::kSepia];
  _sepiaThemeButton.selected = theme == dom_distiller::mojom::Theme::kSepia;
  _darkThemeButton.configuration =
      [self createDarkThemeButtonConfigurationSelected:
                theme == dom_distiller::mojom::Theme::kDark];
  _darkThemeButton.selected = theme == dom_distiller::mojom::Theme::kDark;
}

- (void)setDecreaseFontSizeButtonEnabled:(BOOL)enabled {
  _decreaseFontSizeButton.enabled = enabled;
}

- (void)setIncreaseFontSizeButtonEnabled:(BOOL)enabled {
  _increaseFontSizeButton.enabled = enabled;
}

- (void)announceFontSizeMultiplier:(CGFloat)multiplier {
  if (UIAccessibilityIsVoiceOverRunning()) {
    NSNumberFormatter* numberFormatter = [[NSNumberFormatter alloc] init];
    numberFormatter.numberStyle = NSNumberFormatterPercentStyle;
    numberFormatter.maximumFractionDigits = 0;
    NSString* announcement = [numberFormatter stringFromNumber:@(multiplier)];
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, base::BindOnce(^{
          UIAccessibilityPostNotification(
              UIAccessibilityAnnouncementNotification, announcement);
        }),
        kA11yAnnouncementQueueDelay);
  }
}

#pragma mark - UI actions

// Decrease the font size using the mutator.
- (void)decreaseFontSize:(id)sender {
  [self.mutator decreaseFontSize];
}

// Increase the font size using the mutator.
- (void)increaseFontSize:(id)sender {
  [self.mutator increaseFontSize];
}

// Select the light theme using the mutator.
- (void)selectLightTheme:(id)sender {
  [self.mutator setTheme:dom_distiller::mojom::Theme::kLight];
}

// Select the sepia theme using the mutator.
- (void)selectSepiaTheme:(id)sender {
  [self.mutator setTheme:dom_distiller::mojom::Theme::kSepia];
}

// Select the dark theme using the mutator.
- (void)selectDarkTheme:(id)sender {
  [self.mutator setTheme:dom_distiller::mojom::Theme::kDark];
}

#pragma mark - Stacks creation helpers

// Returns the first row stack.
- (UIStackView*)createFirstRowStack {
  UIStackView* firstRowStack = [[UIStackView alloc] init];
  firstRowStack.axis = UILayoutConstraintAxisHorizontal;
  firstRowStack.spacing = kSpacing;
  firstRowStack.distribution = UIStackViewDistributionFillEqually;

  UIView* fontFamilyButton = _fontFamilyButton;
  UIView* fontFamilyButtonContainer = [[UIView alloc] init];
  fontFamilyButtonContainer.clipsToBounds = YES;
  fontFamilyButtonContainer.translatesAutoresizingMaskIntoConstraints = NO;
  [fontFamilyButtonContainer addSubview:fontFamilyButton];
  AddSameConstraints(fontFamilyButtonContainer, fontFamilyButton);

  if (@available(iOS 26, *)) {
    fontFamilyButtonContainer.cornerConfiguration = [UICornerConfiguration
        configurationWithUniformRadius:[UICornerRadius
                                           containerConcentricRadius]];
  } else {
    fontFamilyButtonContainer.layer.cornerRadius = kCornerRadius;
  }

  [firstRowStack addArrangedSubview:fontFamilyButtonContainer];

  UIView* fontSizeStack = [self createFontSizeStack];

  if (@available(iOS 26, *)) {
    fontSizeStack.cornerConfiguration = [UICornerConfiguration
        configurationWithUniformRadius:[UICornerRadius
                                           containerConcentricRadius]];
  } else {
    fontSizeStack.layer.cornerRadius = kCornerRadius;
  }

  [firstRowStack addArrangedSubview:fontSizeStack];

  NSLayoutConstraint* heightConstraint = [firstRowStack.heightAnchor
      constraintGreaterThanOrEqualToConstant:kFirstRowHeight];
  heightConstraint.priority = UILayoutPriorityRequired;
  heightConstraint.active = YES;

  return firstRowStack;
}

// Returns the second row stack.
- (UIStackView*)createSecondRowStack {
  UIStackView* secondRowStack = [[UIStackView alloc] init];
  secondRowStack.axis = UILayoutConstraintAxisHorizontal;
  secondRowStack.spacing = kSecondRowSpacing;
  secondRowStack.distribution = UIStackViewDistributionFillEqually;
  secondRowStack.alignment = UIStackViewAlignmentFill;

  [secondRowStack addArrangedSubview:_lightThemeButton];
  [secondRowStack addArrangedSubview:_sepiaThemeButton];
  [secondRowStack addArrangedSubview:_darkThemeButton];

  [secondRowStack.heightAnchor
      constraintGreaterThanOrEqualToConstant:kSecondRowHeight]
      .active = YES;

  return secondRowStack;
}

// Returns the font size stack.
- (UIStackView*)createFontSizeStack {
  UIStackView* fontSizeStack = [[UIStackView alloc] init];
  fontSizeStack.axis = UILayoutConstraintAxisHorizontal;
  fontSizeStack.spacing = kFontSizeStackSpacing;
  fontSizeStack.distribution = UIStackViewDistributionFillEqually;
  fontSizeStack.clipsToBounds = YES;

  [fontSizeStack addArrangedSubview:_decreaseFontSizeButton];
  [fontSizeStack addArrangedSubview:_increaseFontSizeButton];

  return fontSizeStack;
}

#pragma mark - Font family menu button creation helpers

// Returns the font family selection button.
- (UIButton*)createFontFamilyButton {
  UIButtonConfiguration* configuration =
      [UIButtonConfiguration grayButtonConfiguration];
  configuration.titleAlignment = UIButtonConfigurationTitleAlignmentLeading;
  configuration.baseForegroundColor = [UIColor colorNamed:kTextPrimaryColor];
  configuration.baseBackgroundColor =
      [UIColor colorNamed:kGroupedPrimaryBackgroundColor];
  configuration.background.cornerRadius = 0;
  UIButton* button = [UIButton buttonWithConfiguration:configuration
                                         primaryAction:nil];
  button.translatesAutoresizingMaskIntoConstraints = NO;
  button.maximumContentSizeCategory = UIContentSizeCategoryExtraExtraLarge;
  button.contentHorizontalAlignment =
      UIControlContentHorizontalAlignmentLeading;
  button.changesSelectionAsPrimaryAction = YES;
  button.showsMenuAsPrimaryAction = YES;
  button.accessibilityIdentifier =
      kReaderModeOptionsFontFamilyButtonAccessibilityIdentifier;
  // By default show Sans-Serif as the selected font family.
  button.menu = [self createFontFamilyMenuWithSelectedFamily:
                          dom_distiller::mojom::FontFamily::kSansSerif];
  return button;
}

// Returns the font family selection menu.
- (UIMenu*)createFontFamilyMenuWithSelectedFamily:
    (dom_distiller::mojom::FontFamily)selectedFontFamily {
  UIAction* sansSerifAction = [self createSansSerifAction];
  UIAction* serifAction = [self createSerifAction];
  UIAction* monospaceAction = [self createMonospaceAction];

  switch (selectedFontFamily) {
    case dom_distiller::mojom::FontFamily::kSansSerif:
      sansSerifAction.state = UIMenuElementStateOn;
      break;
    case dom_distiller::mojom::FontFamily::kSerif:
      serifAction.state = UIMenuElementStateOn;
      break;
    case dom_distiller::mojom::FontFamily::kMonospace:
      monospaceAction.state = UIMenuElementStateOn;
      break;
  }

  return [UIMenu
      menuWithTitle:l10n_util::GetNSString(
                        IDS_IOS_READER_MODE_OPTIONS_FONT_FAMILY_MENU_TITLE)
           children:@[ monospaceAction, sansSerifAction, serifAction ]];
}

// Returns the action to select the Sans-serif font family.
- (UIAction*)createSansSerifAction {
  return [UIAction
      actionWithTitle:
          l10n_util::GetNSString(
              IDS_IOS_READER_MODE_OPTIONS_FONT_FAMILY_SANS_SERIF_LABEL)
                image:nil
           identifier:nil
              handler:^(UIAction* action) {
                [self.mutator
                    setFontFamily:dom_distiller::mojom::FontFamily::kSansSerif];
              }];
}

// Returns the action to select the Serif font family.
- (UIAction*)createSerifAction {
  UIAction* serifAction = [UIAction
      actionWithTitle:l10n_util::GetNSString(
                          IDS_IOS_READER_MODE_OPTIONS_FONT_FAMILY_SERIF_LABEL)
                image:nil
           identifier:nil
              handler:^(UIAction* action) {
                [self.mutator
                    setFontFamily:dom_distiller::mojom::FontFamily::kSerif];
              }];
  return serifAction;
}

// Returns the action to select the Monospace font family.
- (UIAction*)createMonospaceAction {
  UIAction* monospaceAction = [UIAction
      actionWithTitle:
          l10n_util::GetNSString(
              IDS_IOS_READER_MODE_OPTIONS_FONT_FAMILY_MONOSPACE_LABEL)
                image:nil
           identifier:nil
              handler:^(UIAction* action) {
                [self.mutator
                    setFontFamily:dom_distiller::mojom::FontFamily::kMonospace];
              }];
  return monospaceAction;
}

#pragma mark - Font size buttons creation helpers

// Returns the decrease font size button.
- (UIButton*)createDecreaseFontSizeButton {
  UIButton* button =
      [self createFontSizeButtonWithTitle:
                l10n_util::GetNSString(
                    IDS_IOS_READER_MODE_OPTIONS_FONT_SIZE_DECREASE_BUTTON_LABEL)
                                 fontSize:kSmallFontSize];
  button.accessibilityLabel = l10n_util::GetNSString(
      IDS_IOS_READER_MODE_OPTIONS_FONT_SIZE_DECREASE_BUTTON_ACCESSIBILITY_LABEL);
  button.accessibilityIdentifier =
      kReaderModeOptionsDecreaseFontSizeButtonAccessibilityIdentifier;
  [button addTarget:self
                action:@selector(decreaseFontSize:)
      forControlEvents:UIControlEventTouchUpInside];
  return button;
}

// Returns the increase font size button.
- (UIButton*)createIncreaseFontSizeButton {
  UIButton* button =
      [self createFontSizeButtonWithTitle:
                l10n_util::GetNSString(
                    IDS_IOS_READER_MODE_OPTIONS_FONT_SIZE_INCREASE_BUTTON_LABEL)
                                 fontSize:kLargeFontSize];
  button.accessibilityLabel = l10n_util::GetNSString(
      IDS_IOS_READER_MODE_OPTIONS_FONT_SIZE_INCREASE_BUTTON_ACCESSIBILITY_LABEL);
  button.accessibilityIdentifier =
      kReaderModeOptionsIncreaseFontSizeButtonAccessibilityIdentifier;
  [button addTarget:self
                action:@selector(increaseFontSize:)
      forControlEvents:UIControlEventTouchUpInside];
  return button;
}

// Returns a font size button with the given `title` and `fontSize`.
- (UIButton*)createFontSizeButtonWithTitle:(NSString*)title
                                  fontSize:(CGFloat)fontSize {
  UIButtonConfiguration* configuration =
      [UIButtonConfiguration grayButtonConfiguration];
  configuration.baseBackgroundColor =
      [UIColor colorNamed:kGroupedPrimaryBackgroundColor];
  configuration.baseForegroundColor = [UIColor colorNamed:kTextPrimaryColor];
  configuration.background.cornerRadius = 0;
  UIFont* font = [UIFont systemFontOfSize:fontSize];
  font = [[UIFontMetrics defaultMetrics] scaledFontForFont:font];
  configuration.attributedTitle =
      [[NSAttributedString alloc] initWithString:title
                                      attributes:@{NSFontAttributeName : font}];
  UIButton* button = [UIButton buttonWithConfiguration:configuration
                                         primaryAction:nil];
  button.maximumContentSizeCategory = UIContentSizeCategoryExtraExtraLarge;
  return button;
}

#pragma mark - Theme buttons creation helpers

// Returns the light theme button.
- (UIButton*)createLightThemeButton {
  UIButtonConfiguration* configuration =
      [self createLightThemeButtonConfigurationSelected:NO];
  UIButton* button = [UIButton buttonWithConfiguration:configuration
                                         primaryAction:nil];
  button.maximumContentSizeCategory = UIContentSizeCategoryExtraExtraLarge;
  button.accessibilityLabel = l10n_util::GetNSString(
      IDS_IOS_READER_MODE_OPTIONS_COLOR_THEME_BUTTON_ACCESSIBILITY_LABEL_LIGHT);
  button.accessibilityIdentifier =
      kReaderModeOptionsLightThemeButtonAccessibilityIdentifier;
  [button addTarget:self
                action:@selector(selectLightTheme:)
      forControlEvents:UIControlEventTouchUpInside];
  return button;
}

// Returns the sepia theme button.
- (UIButton*)createSepiaThemeButton {
  UIButtonConfiguration* configuration =
      [self createSepiaThemeButtonConfigurationSelected:NO];
  UIButton* button = [UIButton buttonWithConfiguration:configuration
                                         primaryAction:nil];
  button.maximumContentSizeCategory = UIContentSizeCategoryExtraExtraLarge;
  button.accessibilityLabel = l10n_util::GetNSString(
      IDS_IOS_READER_MODE_OPTIONS_COLOR_THEME_BUTTON_ACCESSIBILITY_LABEL_SEPIA);
  button.accessibilityIdentifier =
      kReaderModeOptionsSepiaThemeButtonAccessibilityIdentifier;
  [button addTarget:self
                action:@selector(selectSepiaTheme:)
      forControlEvents:UIControlEventTouchUpInside];
  return button;
}

// Returns the dark theme button.
- (UIButton*)createDarkThemeButton {
  UIButtonConfiguration* configuration =
      [self createDarkThemeButtonConfigurationSelected:NO];
  UIButton* button = [UIButton buttonWithConfiguration:configuration
                                         primaryAction:nil];
  button.maximumContentSizeCategory = UIContentSizeCategoryExtraExtraLarge;
  button.accessibilityLabel = l10n_util::GetNSString(
      IDS_IOS_READER_MODE_OPTIONS_COLOR_THEME_BUTTON_ACCESSIBILITY_LABEL_DARK);
  button.accessibilityIdentifier =
      kReaderModeOptionsDarkThemeButtonAccessibilityIdentifier;
  [button addTarget:self
                action:@selector(selectDarkTheme:)
      forControlEvents:UIControlEventTouchUpInside];
  return button;
}

// Returns the light theme button configuration.
- (UIButtonConfiguration*)createLightThemeButtonConfigurationSelected:
    (BOOL)selected {
  return [self
      createThemeButtonConfigurationSelected:selected
                                   textColor:ReaderModeLightTextColor()
                             backgroundColor:ReaderModeLightBackgroundColor()];
}

// Returns the sepia theme button configuration.
- (UIButtonConfiguration*)createSepiaThemeButtonConfigurationSelected:
    (BOOL)selected {
  return [self
      createThemeButtonConfigurationSelected:selected
                                   textColor:ReaderModeSepiaTextColor()
                             backgroundColor:ReaderModeSepiaBackgroundColor()];
}

// Returns the dark theme button configuration.
- (UIButtonConfiguration*)createDarkThemeButtonConfigurationSelected:
    (BOOL)selected {
  return [self
      createThemeButtonConfigurationSelected:selected
                                   textColor:ReaderModeDarkTextColor()
                             backgroundColor:ReaderModeDarkBackgroundColor()];
}

// Returns a theme button configuration with given `textColor` and
// `backgroundColor`. If `selected` is YES then the button border is blue.
- (UIButtonConfiguration*)
    createThemeButtonConfigurationSelected:(BOOL)selected
                                 textColor:(UIColor*)textColor
                           backgroundColor:(UIColor*)backgroundColor {
  UIButtonConfiguration* configuration =
      [UIButtonConfiguration filledButtonConfiguration];
  configuration.title = l10n_util::GetNSString(
      IDS_IOS_READER_MODE_OPTIONS_COLOR_THEME_BUTTON_LABEL);
  configuration.baseForegroundColor = textColor;
  configuration.baseBackgroundColor = backgroundColor;
  configuration.background.strokeColor =
      selected ? [UIColor colorNamed:kBlue600Color]
               : [UIColor colorNamed:kGroupedPrimaryBackgroundColor];
  configuration.background.strokeWidth =
      selected ? kSelectedThemeBorderWidth : kUnselectedThemeBorderWidth;
  configuration.cornerStyle = UIButtonConfigurationCornerStyleCapsule;
  return configuration;
}

@end
