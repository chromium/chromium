// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_cell_button.h"

#import "base/ios/ios_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_cell_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

namespace {

// Top and bottom margins for the button content.
static const CGFloat kButtonVerticalMargin = 12;

}  // namespace

@implementation ManualFillCellButton

- (id)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    [self initializeStyling];
  }
  return self;
}

- (id)initWithCoder:(NSCoder*)aDecoder {
  self = [super initWithCoder:aDecoder];
  if (self) {
    [self initializeStyling];
  }
  return self;
}

- (void)awakeFromNib {
  [super awakeFromNib];
  [self initializeStyling];
}

- (void)setHighlighted:(BOOL)highlighted {
  [super setHighlighted:highlighted];
  CGFloat alpha = highlighted ? 0.07 : 0;
  self.backgroundColor =
      [[UIColor colorNamed:kTextPrimaryColor] colorWithAlphaComponent:alpha];
}

#pragma mark - Private

- (void)initializeStyling {
  [self setTitleColor:[UIColor colorNamed:kBlueColor]
             forState:UIControlStateNormal];
  self.translatesAutoresizingMaskIntoConstraints = NO;
  self.contentHorizontalAlignment = UIControlContentHorizontalAlignmentLeading;

  UIButtonConfiguration* buttonConfiguration =
      [UIButtonConfiguration plainButtonConfiguration];
  buttonConfiguration.contentInsets = NSDirectionalEdgeInsetsMake(
      kButtonVerticalMargin, kCellMargin, kButtonVerticalMargin, kCellMargin);
  buttonConfiguration.titleLineBreakMode = NSLineBreakByTruncatingTail;
  self.configuration = buttonConfiguration;
}

@end
