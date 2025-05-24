// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/ui/home_customization_background_preset_header_view.h"

#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

@implementation HomeCustomizationBackgroundPresetHeaderView {
  // Text label.
  UILabel* _textLabel;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _textLabel = [self createTextLabel];

    [self addSubview:_textLabel];

    AddSameConstraints(self, _textLabel);
  }
  return self;
}

#pragma mark - Setters

- (void)setText:(NSString*)text {
  _textLabel.text = text;
}

#pragma mark - Private

// Returns the text label view.
- (UILabel*)createTextLabel {
  UILabel* textLabel = [[UILabel alloc] init];
  textLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  textLabel.textAlignment = NSTextAlignmentLeft;
  textLabel.font =
      PreferredFontForTextStyle(UIFontTextStyleHeadline, UIFontWeightSemibold);
  textLabel.adjustsFontForContentSizeCategory = YES;
  textLabel.translatesAutoresizingMaskIntoConstraints = NO;
  return textLabel;
}

@end
