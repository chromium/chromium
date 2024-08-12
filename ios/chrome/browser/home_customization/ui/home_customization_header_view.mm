// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/ui/home_customization_header_view.h"

#import "ios/chrome/browser/home_customization/utils/home_customization_helper.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

@implementation HomeCustomizationHeaderView {
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

- (void)setPage:(CustomizationMenuPage)page {
  if (_page == page) {
    return;
  }
  _page = page;
  _textLabel.text = [HomeCustomizationHelper headerTextForPage:page];
}

#pragma mark - Private

// Returns the text label view.
- (UILabel*)createTextLabel {
  UILabel* textLabel = [[UILabel alloc] init];
  textLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  textLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  textLabel.numberOfLines = 3;
  textLabel.textAlignment = NSTextAlignmentLeft;
  textLabel.adjustsFontForContentSizeCategory = YES;
  textLabel.translatesAutoresizingMaskIntoConstraints = NO;
  return textLabel;
}

@end
