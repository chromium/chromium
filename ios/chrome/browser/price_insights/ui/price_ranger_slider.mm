// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/price_insights/ui/price_ranger_slider.h"

#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/price_insights/ui/price_insights_constants.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// The height of the slider track.
const CGFloat kTrackHeight = 4.0f;

// The width of the circle stroke around the slider thumb.
const CGFloat kCircleStroke = 2.0f;

// The height of the background line behind the slider.
const CGFloat kBackgroundLineHeight = 2.0f;

// The diameter of the circle representing the slider thumb.
const CGFloat kCircleDiameter = 12.0f;

// The relative width of the slider compared to its container.
const CGFloat kSliderWidthRatio = 0.56f;

// Size of the space between the slider and the labels.
const CGFloat kSliderViewContentSpacing = 10.0f;

}  // namespace

@implementation PriceRangeSlider

- (instancetype)init {
  self = [super init];
  if (self) {
    UIImage* thumbImage = [self createCircleImage];
    [self setThumbImage:thumbImage forState:UIControlStateNormal];
    self.maximumTrackTintColor = [UIColor colorNamed:kBlue600Color];
    self.minimumTrackTintColor = [UIColor colorNamed:kBlue600Color];
    self.userInteractionEnabled = NO;
  }
  return self;
}

- (CGRect)trackRectForBounds:(CGRect)bounds {
  CGRect rect = [super trackRectForBounds:bounds];
  rect.size.height = kTrackHeight;
  CGFloat verticalCenter = bounds.size.height / 2;
  //  Adjust the track's y-origin to center it vertically
  rect.origin.y = verticalCenter - (kTrackHeight / 2);
  return rect;
}

#pragma mark - Private

// Creates a circular image with a specified diameter, stroke width, and colors.
- (UIImage*)createCircleImage {
  CGFloat circleDiameter = kCircleDiameter + kCircleStroke;
  CGRect rect = CGRectMake(0.0f, 0.0f, circleDiameter, circleDiameter);
  UIGraphicsBeginImageContextWithOptions(rect.size, NO, 0.0f);
  CGContextRef context = UIGraphicsGetCurrentContext();
  CGContextSetLineWidth(context, kCircleStroke);
  [[UIColor colorNamed:kBackgroundColor] setStroke];
  [[UIColor colorNamed:kBlue600Color] setFill];
  CGContextAddEllipseInRect(
      context, CGRectInset(rect, kCircleStroke / 2, kCircleStroke / 2));
  CGContextDrawPath(context, kCGPathFillStroke);
  UIImage* image = UIGraphicsGetImageFromCurrentImageContext();
  UIGraphicsEndImageContext();

  return image;
}

@end

@implementation PriceRangeSliderView {
  NSString* _minimumLabelText;
  NSString* _maximumLabelText;
  CGFloat _sliderHorizontalInset;
  CGFloat _sliderWidth;
  int64_t _minimumValue;
  int64_t _maximumValue;
  int64_t _currentValue;
}

- (instancetype)initWithMinimumLabelText:(NSString*)minimumLabelText
                        maximumLabelText:(NSString*)maximumLabelText
                            minimumValue:(int64_t)minimumValue
                            maximumValue:(int64_t)maximumValue
                            currentValue:(int64_t)currentValue
                         sliderViewWidth:(CGFloat)sliderViewWidth {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    _minimumLabelText = minimumLabelText;
    _maximumLabelText = maximumLabelText;
    _minimumValue = minimumValue;
    _maximumValue = maximumValue;
    _currentValue = currentValue;
    _sliderWidth = sliderViewWidth * kSliderWidthRatio;
    _sliderHorizontalInset = sliderViewWidth * (1 - kSliderWidthRatio) / 2;

    self.axis = UILayoutConstraintAxisVertical;
    self.spacing = kSliderViewContentSpacing;
    self.alignment = UIStackViewAlignmentFill;

    [self createSlider];
    [self createSliderLabels];
  }
  return self;
}

#pragma mark - Private

// Creates and configures a price range slider view with a horizontal line
// indicator.
- (void)createSlider {
  UIView* sliderContentView = [[UIView alloc] init];
  sliderContentView.translatesAutoresizingMaskIntoConstraints = NO;

  UIView* horizontalLineView = [self createHorizontalLine];
  [sliderContentView addSubview:horizontalLineView];

  PriceRangeSlider* slider = [[PriceRangeSlider alloc] init];
  slider.minimumValue = _minimumValue;
  slider.maximumValue = _maximumValue;
  slider.value = _currentValue;
  slider.translatesAutoresizingMaskIntoConstraints = NO;
  [sliderContentView addSubview:slider];

  [self addArrangedSubview:sliderContentView];

  [NSLayoutConstraint activateConstraints:@[
    [sliderContentView.leadingAnchor
        constraintEqualToAnchor:self.leadingAnchor],
    [sliderContentView.trailingAnchor
        constraintEqualToAnchor:self.trailingAnchor],
    [horizontalLineView.heightAnchor
        constraintEqualToConstant:kBackgroundLineHeight],
    [horizontalLineView.leadingAnchor
        constraintEqualToAnchor:sliderContentView.leadingAnchor],
    [horizontalLineView.trailingAnchor
        constraintEqualToAnchor:sliderContentView.trailingAnchor],
    [horizontalLineView.centerYAnchor
        constraintEqualToAnchor:sliderContentView.centerYAnchor],
    [slider.centerYAnchor
        constraintEqualToAnchor:sliderContentView.centerYAnchor],
    [slider.centerXAnchor
        constraintEqualToAnchor:sliderContentView.centerXAnchor],
    [slider.widthAnchor constraintEqualToConstant:_sliderWidth],
  ]];
}

// Creates and arranges labels for a price range slider component.
- (void)createSliderLabels {
  UILabel* lowLabel =
      [self createLabelWithText:l10n_util::GetNSString(
                                    IDS_PRICE_INSIGHTS_RANGE_LABEL_LOW)
          accessibilityIdentifier:kPriceRangeSliderLowLabelIdentifier];
  UILabel* minimumLabel =
      [self createLabelWithText:_minimumLabelText
          accessibilityIdentifier:kPriceRangeSliderLowPriceIdentifier];
  UILabel* maximumLabel =
      [self createLabelWithText:_maximumLabelText
          accessibilityIdentifier:kPriceRangeSliderHighPriceIdentifier];
  UILabel* highLabel =
      [self createLabelWithText:l10n_util::GetNSString(
                                    IDS_PRICE_INSIGHTS_RANGE_LABEL_HIGH)
          accessibilityIdentifier:kPriceRangeSliderHighLabelIdentifier];

  UIStackView* leftStackView = [self createHorizontalStackView];
  [leftStackView addArrangedSubview:lowLabel];
  [leftStackView addArrangedSubview:minimumLabel];

  UIStackView* rightStackView = [self createHorizontalStackView];
  [rightStackView addArrangedSubview:maximumLabel];
  [rightStackView addArrangedSubview:highLabel];

  UIStackView* labelStackView = [self createHorizontalStackView];
  [labelStackView addArrangedSubview:leftStackView];
  [labelStackView addArrangedSubview:rightStackView];

  [self addArrangedSubview:labelStackView];

  [NSLayoutConstraint activateConstraints:@[
    [minimumLabel.centerXAnchor
        constraintGreaterThanOrEqualToAnchor:leftStackView.leadingAnchor
                                    constant:_sliderHorizontalInset],
    [maximumLabel.centerXAnchor
        constraintLessThanOrEqualToAnchor:rightStackView.trailingAnchor
                                 constant:-(_sliderHorizontalInset)],
    [labelStackView.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
    [labelStackView.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
  ]];
}

// Creates and configures a UILabels.
- (UILabel*)createLabelWithText:(NSString*)text
        accessibilityIdentifier:(NSString*)identifier {
  UILabel* label = [[UILabel alloc] init];
  label.textAlignment = NSTextAlignmentLeft;
  label.adjustsFontForContentSizeCategory = YES;
  label.adjustsFontSizeToFitWidth = NO;
  label.lineBreakMode = NSLineBreakByTruncatingTail;
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.numberOfLines = 1;
  label.accessibilityIdentifier = identifier;
  label.font = CreateDynamicFont(UIFontTextStyleFootnote, UIFontWeightRegular);
  label.text = text;
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.textColor = [UIColor colorNamed:kTextSecondaryColor];
  return label;
}

// Creates a UIView configured as a horizontal line with rounded corners.
- (UIView*)createHorizontalLine {
  UIView* containerStackView = [[UIView alloc] init];
  containerStackView.translatesAutoresizingMaskIntoConstraints = NO;
  containerStackView.backgroundColor = [UIColor colorNamed:kGrey300Color];
  containerStackView.layer.cornerRadius = kBackgroundLineHeight / 2;
  return containerStackView;
}

// Creates and configures a horizontal UIStackView with equal spacing and center
// alignment.
- (UIStackView*)createHorizontalStackView {
  UIStackView* stackView = [[UIStackView alloc] init];
  stackView.axis = UILayoutConstraintAxisHorizontal;
  stackView.distribution = UIStackViewDistributionEqualSpacing;
  stackView.alignment = UIStackViewAlignmentCenter;
  stackView.translatesAutoresizingMaskIntoConstraints = NO;
  return stackView;
}

@end
