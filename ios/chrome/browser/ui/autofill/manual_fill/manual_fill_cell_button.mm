// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_cell_button.h"

#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_cell_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Top and bottom margins for buttons.
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
  self.titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  self.titleLabel.adjustsFontForContentSizeCategory = YES;
  self.contentHorizontalAlignment = UIControlContentHorizontalAlignmentLeading;

  // TODO(crbug.com/1418068): Remove after minimum version required is >=
  // iOS 15.
#if __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_15_0
  self.configuration.contentInsets = NSDirectionalEdgeInsetsMake(
      kButtonVerticalMargin, kButtonHorizontalMargin, kButtonVerticalMargin,
      kButtonHorizontalMargin);
#else
  self.contentEdgeInsets =
      UIEdgeInsetsMake(kButtonVerticalMargin, kButtonHorizontalMargin,
                       kButtonVerticalMargin, kButtonHorizontalMargin);
#endif  // __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_15_0

  self.titleLabel.lineBreakMode = NSLineBreakByTruncatingTail;
}

@end
