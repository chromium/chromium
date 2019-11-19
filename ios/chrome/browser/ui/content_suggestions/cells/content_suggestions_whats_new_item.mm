// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_whats_new_item.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#include "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const CGFloat kLabelMargin = 14;
const CGFloat kLabelLineSpacing = 4;
const CGFloat kLabelIconMargin = 8;
const CGFloat kLabelFontSize = 14;
const CGFloat kIconSize = 24;
const CGFloat kIconTopMargin = 10;

}  // namespace

#pragma mark - ContentSuggestionsWhatsNewItem

@implementation ContentSuggestionsWhatsNewItem

@synthesize text = _text;
@synthesize icon = _icon;
@synthesize suggestionIdentifier = _suggestionIdentifier;
@synthesize metricsRecorded = _metricsRecorded;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [ContentSuggestionsWhatsNewCell class];
  }
  return self;
}

- (void)configureCell:(ContentSuggestionsWhatsNewCell*)cell {
  [super configureCell:cell];
  [cell setIcon:self.icon];
  [cell setText:self.text];
  cell.accessibilityIdentifier = [[self class] accessibilityIdentifier];
}

- (CGFloat)cellHeightForWidth:(CGFloat)width {
  return [self.cellClass heightForWidth:width withText:self.text];
}

+ (NSString*)accessibilityIdentifier {
  return @"ContentSuggestionsWhatsNewIdentifier";
}

@end

#pragma mark - ContentSuggestionsWhatsNewCell

@interface ContentSuggestionsWhatsNewCell ()

@property(nonatomic, strong) UIImageView* iconView;
@property(nonatomic, strong) UILabel* promoLabel;
@property(nonatomic, strong) UIView* containerView;

@end

@implementation ContentSuggestionsWhatsNewCell

@synthesize iconView = _iconView;
@synthesize promoLabel = _promoLabel;
@synthesize containerView = _containerView;

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _iconView = [[UIImageView alloc] init];
    _promoLabel = [[UILabel alloc] init];
    _containerView = [[UIView alloc] init];

    _iconView.translatesAutoresizingMaskIntoConstraints = NO;
    _promoLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _containerView.translatesAutoresizingMaskIntoConstraints = NO;

    [self.contentView addSubview:_containerView];
    [_containerView addSubview:_iconView];
    [_containerView addSubview:_promoLabel];

    ApplyVisualConstraintsWithMetrics(
        @[
          @"V:|-margin-[promo]-margin-|", @"V:|-iconMargin-[icon(==iconSize)]",
          @"V:|[container]|", @"H:|[icon(==iconSize)]-spacing-[promo]|",
          @"H:|->=0-[container]->=0-|"
        ],
        @{
          @"icon" : _iconView,
          @"promo" : _promoLabel,
          @"container" : _containerView
        },
        @{
          @"margin" : @(kLabelMargin),
          @"iconMargin" : @(kIconTopMargin),
          @"iconSize" : @(kIconSize),
          @"spacing" : @(kLabelIconMargin)
        });
    [NSLayoutConstraint activateConstraints:@[
      [_containerView.centerXAnchor
          constraintEqualToAnchor:self.contentView.centerXAnchor]
    ]];
  }
  return self;
}

- (void)setIcon:(UIImage*)icon {
  self.iconView.image = icon;
}

- (void)setText:(NSString*)text {
  [[self class] configureLabel:self.promoLabel withText:text];
}

+ (CGFloat)heightForWidth:(CGFloat)width withText:(NSString*)text {
  UILabel* label = [[UILabel alloc] init];
  [self configureLabel:label withText:text];
  CGSize sizeForLabel = CGSizeMake(width - kLabelIconMargin - kIconSize, 500);

  return 2 * kLabelMargin + [label sizeThatFits:sizeForLabel].height;
}

#pragma mark UIView

// Implements -layoutSubviews as per instructions in documentation for
// +[MDCCollectionViewCell cr_preferredHeightForWidth:forItem:].
- (void)layoutSubviews {
  [super layoutSubviews];

  // Adjust the text label preferredMaxLayoutWidth when the parent's width
  // changes, for instance on screen rotation.
  CGFloat parentWidth = CGRectGetWidth(self.contentView.bounds);

  self.promoLabel.preferredMaxLayoutWidth =
      parentWidth - kIconSize - kLabelIconMargin;

  // Re-layout with the new preferred width to allow the label to adjust its
  // height.
  [super layoutSubviews];
}

#pragma mark Private

// Configures the |promoLabel| with the |text|.
+ (void)configureLabel:(UILabel*)promoLabel withText:(NSString*)text {
  promoLabel.font =
      [[MDCTypography fontLoader] regularFontOfSize:kLabelFontSize];
  promoLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  promoLabel.numberOfLines = 0;

  NSRange linkRange;
  NSString* strippedText = ParseStringWithLink(text, &linkRange);
  DCHECK_NE(NSNotFound, static_cast<NSInteger>(linkRange.location));
  DCHECK_NE(0u, linkRange.length);

  NSMutableAttributedString* attributedText =
      [[NSMutableAttributedString alloc] initWithString:strippedText];

  // Sets the styling to mimic a link.
  UIColor* linkColor = [UIColor colorNamed:kBlueColor];
  [attributedText addAttribute:NSForegroundColorAttributeName
                         value:linkColor
                         range:linkRange];
  [attributedText addAttribute:NSUnderlineStyleAttributeName
                         value:@(NSUnderlineStyleSingle)
                         range:linkRange];
  [attributedText addAttribute:NSUnderlineColorAttributeName
                         value:linkColor
                         range:linkRange];

  // Sets the line spacing on the attributed string.
  NSInteger strLength = [strippedText length];
  NSMutableParagraphStyle* style = [[NSMutableParagraphStyle alloc] init];
  [style setLineSpacing:kLabelLineSpacing];
  [attributedText addAttribute:NSParagraphStyleAttributeName
                         value:style
                         range:NSMakeRange(0, strLength)];

  [promoLabel setAttributedText:attributedText];
}

@end
