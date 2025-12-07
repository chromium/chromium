// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/ui/home_cutomization_color_palette_cell.h"

#import <UIKit/UIKit.h>

#import "base/check.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_color_palette.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

// Define constants within the namespace.
namespace {

// Border width for the highlight state.
const CGFloat kHighlightBorderWidth = 3.0;

// Border width for the gap between content and borders.
const CGFloat kGapBorderWidth = 3.0;

}  // namespace

@implementation HomeCustomizationColorPaletteCell {
  // View representing the light tone color.
  UIView* _lightColorView;

  // View representing the dark tone color.
  UIView* _darkColorView;

  // View representing the medium tone color.
  UIView* _mediumColorView;

  // Container view that provides the outer highlight border.
  // Acts as a decorative wrapper for the inner content.
  UIView* _borderWrapperView;

  // Main content view rendered inside the border wrapper.
  // Displays the core visual element.
  UIView* _innerContentView;
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

    _lightColorView = [[UIView alloc] init];
    _darkColorView = [[UIView alloc] init];
    _mediumColorView = [[UIView alloc] init];

    _lightColorView.translatesAutoresizingMaskIntoConstraints = NO;
    _darkColorView.translatesAutoresizingMaskIntoConstraints = NO;
    _mediumColorView.translatesAutoresizingMaskIntoConstraints = NO;

    [_innerContentView addSubview:_lightColorView];
    [_innerContentView addSubview:_darkColorView];
    [_innerContentView addSubview:_mediumColorView];

    [NSLayoutConstraint activateConstraints:@[
      // Top half (light color).
      [_lightColorView.topAnchor
          constraintEqualToAnchor:_innerContentView.topAnchor],
      [_lightColorView.leadingAnchor
          constraintEqualToAnchor:_innerContentView.leadingAnchor],
      [_lightColorView.trailingAnchor
          constraintEqualToAnchor:_innerContentView.trailingAnchor],
      [_lightColorView.heightAnchor
          constraintEqualToAnchor:_innerContentView.heightAnchor
                       multiplier:0.5],

      // Bottom left quarter (dark color).
      [_darkColorView.bottomAnchor
          constraintEqualToAnchor:_innerContentView.bottomAnchor],
      [_darkColorView.leadingAnchor
          constraintEqualToAnchor:_innerContentView.leadingAnchor],
      [_darkColorView.widthAnchor
          constraintEqualToAnchor:_innerContentView.widthAnchor
                       multiplier:0.5],
      [_darkColorView.heightAnchor
          constraintEqualToAnchor:_innerContentView.heightAnchor
                       multiplier:0.5],

      // Bottom right quarter (medium color).
      [_mediumColorView.bottomAnchor
          constraintEqualToAnchor:_innerContentView.bottomAnchor],
      [_mediumColorView.trailingAnchor
          constraintEqualToAnchor:_innerContentView.trailingAnchor],
      [_mediumColorView.widthAnchor
          constraintEqualToAnchor:_innerContentView.widthAnchor
                       multiplier:0.5],
      [_mediumColorView.heightAnchor
          constraintEqualToAnchor:_innerContentView.heightAnchor
                       multiplier:0.5],
    ]];

    // Constraints for positioning the border wrapper view inside the cell.
    AddSameConstraints(_borderWrapperView, self.contentView);
    AddSameConstraintsWithInset(_innerContentView, _borderWrapperView,
                                kGapBorderWidth + kHighlightBorderWidth);

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

- (void)setColorPalette:(NewTabPageColorPalette*)colorPalette {
  _colorPalette = colorPalette;
  _lightColorView.backgroundColor = colorPalette.lightColor;
  _darkColorView.backgroundColor = colorPalette.darkColor;
  _mediumColorView.backgroundColor = colorPalette.mediumColor;
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
