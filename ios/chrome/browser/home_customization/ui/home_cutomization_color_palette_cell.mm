// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/ui/home_cutomization_color_palette_cell.h"

#import <UIKit/UIKit.h>

#import "base/check.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_color_palette_configuration.h"

@implementation HomeCustomizationColorPaletteCell {
  // View representing the light tone color.
  UIView* _lightColorView;

  // View representing the dark tone color.
  UIView* _darkColorView;

  // View representing the medium tone color.
  UIView* _mediumColorView;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.backgroundColor = UIColor.clearColor;

    _lightColorView = [[UIView alloc] init];
    _darkColorView = [[UIView alloc] init];
    _mediumColorView = [[UIView alloc] init];

    _lightColorView.translatesAutoresizingMaskIntoConstraints = NO;
    _darkColorView.translatesAutoresizingMaskIntoConstraints = NO;
    _mediumColorView.translatesAutoresizingMaskIntoConstraints = NO;

    [self addSubview:_lightColorView];
    [self addSubview:_darkColorView];
    [self addSubview:_mediumColorView];

    [NSLayoutConstraint activateConstraints:@[
      // Top half (light color).
      [_lightColorView.topAnchor constraintEqualToAnchor:self.topAnchor],
      [_lightColorView.leadingAnchor
          constraintEqualToAnchor:self.leadingAnchor],
      [_lightColorView.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor],
      [_lightColorView.heightAnchor constraintEqualToAnchor:self.heightAnchor
                                                 multiplier:0.5],

      // Bottom left quarter (dark color).
      [_darkColorView.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
      [_darkColorView.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
      [_darkColorView.widthAnchor constraintEqualToAnchor:self.widthAnchor
                                               multiplier:0.5],
      [_darkColorView.heightAnchor constraintEqualToAnchor:self.heightAnchor
                                                multiplier:0.5],

      // Bottom right quarter (medium color).
      [_mediumColorView.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
      [_mediumColorView.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor],
      [_mediumColorView.widthAnchor constraintEqualToAnchor:self.widthAnchor
                                                 multiplier:0.5],
      [_mediumColorView.heightAnchor constraintEqualToAnchor:self.heightAnchor
                                                  multiplier:0.5],
    ]];
  }
  return self;
}

- (void)layoutSubviews {
  [super layoutSubviews];

  // Circle shape by rounding the corners
  self.layer.cornerRadius = self.bounds.size.width / 2.0;
  self.clipsToBounds = YES;
}

- (void)setConfiguration:
    (HomeCustomizationColorPaletteConfiguration*)configuration {
  _configuration = configuration;
  _lightColorView.backgroundColor = configuration.lightColor;
  _darkColorView.backgroundColor = configuration.darkColor;
  _mediumColorView.backgroundColor = configuration.mediumColor;
}

@end
