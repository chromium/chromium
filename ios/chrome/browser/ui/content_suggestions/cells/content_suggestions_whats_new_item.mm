// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_whats_new_item.h"

#import <MaterialComponents/MaterialTypography.h>

#include "base/check_op.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_cells_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_whats_new_view.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kLabelMargin = 14;
const CGFloat kLabelLineSpacing = 4;
const CGFloat kLabelIconMargin = 8;
const CGFloat kLabelFontSize = 14;
const CGFloat kIconSize = 24;
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
  return [ContentSuggestionsWhatsNewCell heightForWidth:width
                                               withText:self.text];
}

+ (NSString*)accessibilityIdentifier {
  return kContentSuggestionsWhatsNewIdentifier;
}

@end

#pragma mark - ContentSuggestionsWhatsNewCell

@interface ContentSuggestionsWhatsNewCell ()

// View containing all UI elements
@property(nonatomic, strong) ContentSuggestionsWhatsNewView* whatsNewView;

@end

@implementation ContentSuggestionsWhatsNewCell

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _whatsNewView =
        [[ContentSuggestionsWhatsNewView alloc] initWithFrame:frame];
    [self.contentView addSubview:_whatsNewView];
    _whatsNewView.translatesAutoresizingMaskIntoConstraints = NO;
    AddSameConstraints(self.contentView, _whatsNewView);
  }
  return self;
}

- (void)setIcon:(UIImage*)icon {
  self.whatsNewView.iconView.image = icon;
}

- (void)setText:(NSString*)text {
  [[self class] configureLabel:self.whatsNewView.promoLabel withText:text];
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

  self.whatsNewView.promoLabel.preferredMaxLayoutWidth =
      parentWidth - kIconSize - kLabelIconMargin;

  // Re-layout with the new preferred width to allow the label to adjust its
  // height.
  [super layoutSubviews];
}

#pragma mark Private

// Configures the `promoLabel` with the `text`.
+ (void)configureLabel:(UILabel*)promoLabel withText:(NSString*)text {
  promoLabel.font = [UIFont systemFontOfSize:kLabelFontSize
                                      weight:UIFontWeightRegular];
  promoLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  promoLabel.numberOfLines = 0;

  // Sets the line spacing on the attributed string.
  NSMutableParagraphStyle* style = [[NSMutableParagraphStyle alloc] init];
  [style setLineSpacing:kLabelLineSpacing];
  NSDictionary* textAttributes = @{
    NSParagraphStyleAttributeName : style,
  };

  // Sets the styling to mimic a link.
  NSDictionary* linkAttributes = @{
    NSForegroundColorAttributeName : [UIColor colorNamed:kBlueColor],
    NSUnderlineStyleAttributeName : @(NSUnderlineStyleSingle),
    NSUnderlineColorAttributeName : [UIColor colorNamed:kBlueColor],
  };

  NSAttributedString* attributedText =
      AttributedStringFromStringWithLink(text, textAttributes, linkAttributes);

  [promoLabel setAttributedText:attributedText];
}

@end
