// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_granular_fill_cell_content_configuration.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/autofill/atmemory/public/at_memory_constants.h"
#import "ios/chrome/browser/autofill/atmemory/utils/atmemory_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/button_util.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// Padding inside each chip button.
constexpr CGFloat kChipPadding = 12.0;

// Horizontal padding of the content view inside the cell.
constexpr CGFloat kContentHorizontalPadding = 16.0;

// Vertical padding of the content view inside the cell.
constexpr CGFloat kContentVerticalPadding = 8.0;

// Vertical spacing between the attribute name label and the chip button.
constexpr CGFloat kStackVerticalSpacing = 4.0;

// Creates and configures the attribute name label.
UILabel* CreateAttributeLabel() {
  UILabel* label = [[UILabel alloc] init];
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleCaption2];
  label.textColor = [UIColor colorNamed:kTextSecondaryColor];
  label.adjustsFontForContentSizeCategory = YES;
  return label;
}

// Updates the title and accessibility label of a chip button.
void UpdateChipButton(UIButton* button, NSString* value) {
  SetConfigurationTitle(button, value);
  button.accessibilityLabel = value;
}

// Creates and configures the selectable chip button.
UIButton* CreateChipButton() {
  UIButton* button = [UIButton buttonWithType:UIButtonTypeCustom];
  UIButtonConfiguration* buttonConfiguration =
      [UIButtonConfiguration plainButtonConfiguration];
  buttonConfiguration.contentInsets = NSDirectionalEdgeInsetsMake(
      kChipPadding, kChipPadding, kChipPadding, kChipPadding);
  buttonConfiguration.cornerStyle = UIButtonConfigurationCornerStyleCapsule;
  buttonConfiguration.baseForegroundColor =
      [UIColor colorNamed:kTextPrimaryColor];

  UIBackgroundConfiguration* backgroundConfiguration =
      [UIBackgroundConfiguration clearConfiguration];
  backgroundConfiguration.backgroundColor =
      [UIColor colorNamed:kTertiaryBackgroundColor];
  buttonConfiguration.background = backgroundConfiguration;
  button.configuration = buttonConfiguration;

  SetConfigurationFont(
      button, [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote]);

  button.titleLabel.adjustsFontForContentSizeCategory = YES;
  return button;
}

}  // namespace

#pragma mark - AtMemoryGranularFillContentView

// Private UIView implementing UIContentView to render
// AtMemoryGranularFillCellContentConfiguration.
@interface AtMemoryGranularFillContentView : UIView <UIContentView>

- (instancetype)initWithConfiguration:
    (AtMemoryGranularFillCellContentConfiguration*)configuration
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

@end

@implementation AtMemoryGranularFillContentView {
  // Current content configuration applied to this view.
  AtMemoryGranularFillCellContentConfiguration* _configuration;
  // Label displaying the attribute name (e.g. "Hotel Name", "Address").
  UILabel* _attributeLabel;
  // Selectable value chip button.
  UIButton* _chipButton;
  // Vertical container stack view hosting the attribute label and chip button.
  UIStackView* _containerStackView;
}

- (instancetype)initWithConfiguration:
    (AtMemoryGranularFillCellContentConfiguration*)configuration {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    _attributeLabel = CreateAttributeLabel();
    _chipButton = CreateChipButton();
    [_chipButton addTarget:self
                    action:@selector(onButtonTapped:)
          forControlEvents:UIControlEventTouchUpInside];

    _containerStackView = [[UIStackView alloc]
        initWithArrangedSubviews:@[ _attributeLabel, _chipButton ]];
    _containerStackView.axis = UILayoutConstraintAxisVertical;
    _containerStackView.alignment = UIStackViewAlignmentLeading;
    _containerStackView.spacing = kStackVerticalSpacing;
    _containerStackView.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_containerStackView];

    AddSameConstraintsWithInsets(
        _containerStackView, self,
        NSDirectionalEdgeInsetsMake(
            kContentVerticalPadding, kContentHorizontalPadding,
            kContentVerticalPadding, kContentHorizontalPadding));

    self.configuration = configuration;
  }
  return self;
}

- (void)onButtonTapped:(UIButton*)sender {
  if (_configuration.selectionHandler &&
      _configuration.attributeValue.length > 0) {
    _configuration.selectionHandler(_configuration.attributeValue);
  }
}

#pragma mark - UIContentView

- (id<UIContentConfiguration>)configuration {
  return [_configuration copy];
}

- (void)setConfiguration:(id<UIContentConfiguration>)configuration {
  AtMemoryGranularFillCellContentConfiguration* config =
      base::apple::ObjCCast<AtMemoryGranularFillCellContentConfiguration>(
          configuration);
  CHECK(config);
  _configuration = [config copy];

  _attributeLabel.text = _configuration.attributeName;
  UpdateChipButton(_chipButton, _configuration.attributeValue);

  _attributeLabel.accessibilityIdentifier =
      GetAtMemoryGranularFillAttributeLabelAccessibilityIdentifier(
          _configuration.attributeName);
  _chipButton.accessibilityIdentifier =
      GetAtMemoryGranularFillChipButtonAccessibilityIdentifier(
          _configuration.attributeName);

  self.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
}

@end

#pragma mark - AtMemoryGranularFillCellContentConfiguration

@implementation AtMemoryGranularFillCellContentConfiguration

+ (instancetype)cellConfiguration {
  return [[self alloc] init];
}

#pragma mark - NSCopying

- (id)copyWithZone:(NSZone*)zone {
  AtMemoryGranularFillCellContentConfiguration* copy =
      [[AtMemoryGranularFillCellContentConfiguration allocWithZone:zone] init];
  copy.attributeName = self.attributeName;
  copy.attributeValue = self.attributeValue;
  copy.selectionHandler = self.selectionHandler;
  return copy;
}

#pragma mark - UIContentConfiguration

- (UIView<UIContentView>*)makeContentView {
  return [[AtMemoryGranularFillContentView alloc] initWithConfiguration:self];
}

- (instancetype)updatedConfigurationForState:(id<UIConfigurationState>)state {
  return self;
}

@end
