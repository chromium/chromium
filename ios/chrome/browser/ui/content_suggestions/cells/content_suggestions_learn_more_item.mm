// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_learn_more_item.h"

#import <MaterialComponents/MaterialPalettes.h>
#import <MaterialComponents/MaterialTypography.h>

#include "base/check_op.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kLabelLineSpacing = 4;
const CGFloat kTopLabelMargin = 24;
const CGFloat kBottomLabelMargin = 8;
}

#pragma mark - ContentSuggestionsLearnMoreItem

@implementation ContentSuggestionsLearnMoreItem

@synthesize suggestionIdentifier;
@synthesize metricsRecorded;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [ContentSuggestionsLearnMoreCell class];
  }
  return self;
}

- (void)configureCell:(ContentSuggestionsLearnMoreCell*)cell {
  [super configureCell:cell];
  [cell setText:[self text]];
  cell.accessibilityIdentifier = kContentSuggestionsLearnMoreIdentifier;
}

- (NSString*)text {
  return l10n_util::GetNSString(
      IDS_IOS_NEW_TAB_LEARN_MORE_ABOUT_SUGGESTED_CONTENT);
}

- (CGFloat)cellHeightForWidth:(CGFloat)width {
  return [ContentSuggestionsLearnMoreCell heightForWidth:width
                                                withText:[self text]];
}

@end

#pragma mark - ContentSuggestionsLearnMoreCell

@interface ContentSuggestionsLearnMoreCell ()

@property(nonatomic, strong) UILabel* label;

@end

@implementation ContentSuggestionsLearnMoreCell

@synthesize label = _label;

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _label = [[UILabel alloc] init];
    _label.translatesAutoresizingMaskIntoConstraints = NO;
    [self.contentView addSubview:_label];
    _label.accessibilityTraits = UIAccessibilityTraitLink;
    [NSLayoutConstraint activateConstraints:@[
      [_label.centerXAnchor
          constraintEqualToAnchor:self.contentView.centerXAnchor],
      [_label.widthAnchor
          constraintLessThanOrEqualToAnchor:self.contentView.widthAnchor]
    ]];
    ApplyVisualConstraintsWithMetrics(
        @[ @"V:|-(top)-[label]-(bottom)-|" ], @{@"label" : _label},
        @{ @"top" : @(kTopLabelMargin),
           @"bottom" : @(kBottomLabelMargin) });
  }
  return self;
}

+ (CGFloat)heightForWidth:(CGFloat)width withText:(NSString*)text {
  UILabel* label = [[UILabel alloc] init];
  [self configureLabel:label withText:text];
  CGFloat labelHeight =
      [label sizeThatFits:CGSizeMake(width, CGFLOAT_MAX)].height;
  return kTopLabelMargin + labelHeight + kBottomLabelMargin;
}

- (void)setText:(NSString*)text {
  [[self class] configureLabel:self.label withText:text];
}

#pragma mark Private

// Configures the |label| with the |text| containing a link.
+ (void)configureLabel:(UILabel*)label withText:(NSString*)text {
  label.numberOfLines = 0;
  label.textColor = [[MDCPalette greyPalette] tint700];
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];

  // Sets the line spacing on the attributed string.
  NSMutableParagraphStyle* style = [[NSMutableParagraphStyle alloc] init];
  [style setLineSpacing:kLabelLineSpacing];

  NSDictionary* textAttributes = @{
    NSParagraphStyleAttributeName : style,
  };
  // Sets the styling to mimic a link.
  NSDictionary* linkAttributes =
      @{NSForegroundColorAttributeName : [UIColor colorNamed:kBlueColor]};

  NSAttributedString* attributedText =
      AttributedStringFromStringWithLink(text, textAttributes, linkAttributes);
  [label setAttributedText:attributedText];
}

@end
