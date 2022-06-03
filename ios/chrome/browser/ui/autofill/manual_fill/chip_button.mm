// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/chip_button.h"

#include "base/mac/foundation_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Top and bottom padding for the button.
static const CGFloat kChipVerticalPadding = 14;
// Left and right padding for the button.
static const CGFloat kChipHorizontalPadding = 14;
// Vertical margins for the button. How much bigger the tap target is.
static const CGFloat kChipVerticalMargin = 4;
}  // namespace

@interface ChipButton ()

// Gray rounded background view which gives the aspect of a chip.
@property(strong, nonatomic) UIView* backgroundView;

@end

@implementation ChipButton

- (id)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    [self initializeStyling];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(updateTitleLabelFont)
               name:UIContentSizeCategoryDidChangeNotification
             object:nil];
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

- (void)layoutSubviews {
  [super layoutSubviews];
  self.backgroundView.layer.cornerRadius =
      self.backgroundView.bounds.size.height / 2.0;
}

- (void)setHighlighted:(BOOL)highlighted {
  [super setHighlighted:highlighted];
  self.backgroundView.backgroundColor =
      highlighted ? [UIColor colorNamed:kGrey300Color]
                  : [UIColor colorNamed:kGrey100Color];
}

- (void)setEnabled:(BOOL)enabled {
  [super setEnabled:enabled];
  self.backgroundView.hidden = !enabled;
  self.contentEdgeInsets = enabled ? [self chipEdgeInsets] : UIEdgeInsetsZero;
}

#pragma mark - Private

- (UIEdgeInsets)chipEdgeInsets {
  return UIEdgeInsetsMake(kChipVerticalPadding, kChipHorizontalPadding,
                          kChipVerticalPadding, kChipHorizontalPadding);
}

- (void)initializeStyling {
  _backgroundView = [[UIView alloc] init];
  _backgroundView.userInteractionEnabled = NO;
  _backgroundView.backgroundColor = [UIColor colorNamed:kGrey100Color];
  _backgroundView.translatesAutoresizingMaskIntoConstraints = NO;

  [self addSubview:_backgroundView];
  [self sendSubviewToBack:_backgroundView];
  [NSLayoutConstraint activateConstraints:@[
    [_backgroundView.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
    [_backgroundView.trailingAnchor
        constraintEqualToAnchor:self.trailingAnchor],
    [_backgroundView.topAnchor constraintEqualToAnchor:self.topAnchor
                                              constant:kChipVerticalMargin],
    [_backgroundView.bottomAnchor constraintEqualToAnchor:self.bottomAnchor
                                                 constant:-kChipVerticalMargin]
  ]];

  self.translatesAutoresizingMaskIntoConstraints = NO;

  [self setTitleColor:[UIColor colorNamed:kTextPrimaryColor]
             forState:UIControlStateNormal];
  self.titleLabel.adjustsFontForContentSizeCategory = YES;

  [self updateTitleLabelFont];
  self.contentEdgeInsets = [self chipEdgeInsets];
}

- (void)updateTitleLabelFont {
  UIFont* font = [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
  UIFontDescriptor* boldFontDescriptor = [font.fontDescriptor
      fontDescriptorWithSymbolicTraits:UIFontDescriptorTraitBold];
  DCHECK(boldFontDescriptor);
  self.titleLabel.font = [UIFont fontWithDescriptor:boldFontDescriptor size:0];
}

@end
