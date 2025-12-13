// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/ui/home_customization_custom_color_cell.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

// Define constants within the namespace.
namespace {

// Border width for the highlight state.
const CGFloat kHighlightBorderWidth = 3.0;

// Border width for the gap between content and borders.
const CGFloat kGapBorderWidth = 3.0;

// Font size for the eyedropper symbol.
const CGFloat kEyedropperSymbolSize = 15.0;

}  // namespace

@implementation HomeCustomizationCustomColorCell {
  // Container view that provides the outer highlight border.
  // Acts as a decorative wrapper for the inner content.
  UIView* _borderWrapperView;

  // Main content view rendered inside the border wrapper.
  // Displays the core visual element.
  UIView* _innerContentView;

  // Image view to display the symbol.
  UIImageView* _symbolImageView;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.contentView.backgroundColor = UIColor.clearColor;
    self.isAccessibilityElement = YES;
    self.accessibilityTraits |= UIAccessibilityTraitButton;

    // Outer container view that holds the highlight border.
    _borderWrapperView = [[UIView alloc] init];
    _borderWrapperView.translatesAutoresizingMaskIntoConstraints = NO;
    _borderWrapperView.backgroundColor = UIColor.clearColor;
    _borderWrapperView.layer.masksToBounds = YES;
    [self.contentView addSubview:_borderWrapperView];

    // Inner content view, placed with a gap inside the border wrapper view.
    // This holds the actual content.
    _innerContentView = [[UIView alloc] init];
    _innerContentView.translatesAutoresizingMaskIntoConstraints = NO;
    _innerContentView.layer.masksToBounds = YES;
    _innerContentView.layer.borderWidth = 1;
    [_borderWrapperView addSubview:_innerContentView];

    // Eye dropper icon.
    UIImage* eyeDropperIcon = DefaultSymbolTemplateWithPointSize(
        kEyedropperSymbol, kEyedropperSymbolSize);

    _symbolImageView = [[UIImageView alloc] initWithImage:eyeDropperIcon];
    _symbolImageView.translatesAutoresizingMaskIntoConstraints = NO;
    _symbolImageView.tintColor = [UIColor colorNamed:kTextPrimaryColor];

    [_innerContentView addSubview:_symbolImageView];

    // Constraints for positioning the border wrapper view inside the cell.
    AddSameConstraints(_borderWrapperView, self.contentView);
    AddSameConstraintsWithInset(_innerContentView, _borderWrapperView,
                                kGapBorderWidth + kHighlightBorderWidth);
    [NSLayoutConstraint activateConstraints:@[
      [_symbolImageView.centerXAnchor
          constraintEqualToAnchor:self.contentView.centerXAnchor],
      [_symbolImageView.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],
    ]];

    [self registerForTraitChanges:@[ UITraitUserInterfaceStyle.class ]
                       withAction:@selector(updateCGColors)];
    [self updateCGColors];
  }
  return self;
}

- (void)layoutSubviews {
  [super layoutSubviews];

  CGFloat diameter = self.bounds.size.width;

  // Circle shape by rounding the corners.
  _borderWrapperView.layer.cornerRadius = diameter / 2.0;
  _innerContentView.layer.cornerRadius =
      (diameter - 2 * (kGapBorderWidth + kHighlightBorderWidth)) / 2.0;

  _borderWrapperView.clipsToBounds = YES;
  _innerContentView.clipsToBounds = YES;
}

- (void)setColor:(UIColor*)color {
  _color = color;
  _innerContentView.backgroundColor = _color;
}

- (void)setSelected:(BOOL)selected {
  if (self.selected == selected) {
    return;
  }

  [super setSelected:selected];

  if (selected) {
    _borderWrapperView.layer.borderColor =
        [UIColor colorNamed:kStaticBlueColor].CGColor;
    _borderWrapperView.layer.borderWidth = kHighlightBorderWidth;
  } else {
    _borderWrapperView.layer.borderColor = nil;
    _borderWrapperView.layer.borderWidth = 0;
    self.accessibilityValue = nil;
  }
}

// Updates CGColors when the user interface style changes, as they do not
// update automatically.
- (void)updateCGColors {
  _innerContentView.layer.borderColor =
      [UIColor colorNamed:kGrey200Color].CGColor;
}

@end
