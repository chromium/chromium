// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_whats_new_view.h"

#import <MaterialComponents/MaterialTypography.h>

#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_cells_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_whats_new_item.h"
#include "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {
const CGFloat kLabelMargin = 14;
const CGFloat kLabelLineSpacing = 4;
const CGFloat kLabelIconMargin = 8;
const CGFloat kLabelFontSize = 14;
const CGFloat kIconSize = 24;
const CGFloat kIconTopMargin = 10;
}  // namespace

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ContentSuggestionsWhatsNewView ()

// Container view for all subviews.
@property(nonatomic, strong) UIView* containerView;

@end

@implementation ContentSuggestionsWhatsNewView

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _iconView = [[UIImageView alloc] init];
    _promoLabel = [[UILabel alloc] init];
    _containerView = [[UIView alloc] init];

    _iconView.translatesAutoresizingMaskIntoConstraints = NO;
    _promoLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _containerView.translatesAutoresizingMaskIntoConstraints = NO;

    [self addSubview:_containerView];
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
    [NSLayoutConstraint
        activateConstraints:@[ [_containerView.centerXAnchor
                                constraintEqualToAnchor:self.centerXAnchor] ]];
  }
  return self;
}

- (instancetype)initWithConfiguration:(ContentSuggestionsWhatsNewItem*)config {
  self = [self initWithFrame:CGRectZero];
  if (self) {
    [_iconView setImage:config.icon];
    [self configureLabelWithText:config.text];
    self.isAccessibilityElement = YES;
    self.accessibilityIdentifier = kContentSuggestionsWhatsNewIdentifier;
  }
  return self;
}

// Configures `promoLabel` with `text`.
- (void)configureLabelWithText:(NSString*)text {
  _promoLabel.font = [UIFont systemFontOfSize:kLabelFontSize
                                       weight:UIFontWeightRegular];
  _promoLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  _promoLabel.numberOfLines = 0;

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

  [_promoLabel setAttributedText:attributedText];
}

@end
