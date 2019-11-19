// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/unified_consent/identity_chooser/identity_chooser_header_item.h"

#import "ios/chrome/common/colors/UIColor+cr_semantic_colors.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Top margin for the label.
CGFloat kTopMargin = 20.;
CGFloat kBottomMargin = 15;
// Leading margin for the label.
CGFloat kLeadingMargin = 24.;
}  // namespace

@interface IdentityChooserHeaderView : UITableViewHeaderFooterView
@end

@implementation IdentityChooserHeaderView

- (instancetype)initWithReuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithReuseIdentifier:reuseIdentifier];
  if (self) {
    UILabel* label = [[UILabel alloc] init];
    label.translatesAutoresizingMaskIntoConstraints = NO;
    label.numberOfLines = 2;
    label.text =
        l10n_util::GetNSString(IDS_IOS_ACCOUNT_IDENTITY_CHOOSER_CHOOSE_ACCOUNT);
    label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
    label.textColor = UIColor.cr_labelColor;
    [self.contentView addSubview:label];
    NSDictionary* views = @{
      @"label" : label,
    };
    NSDictionary* metrics = @{
      @"LeadingMargin" : @(kLeadingMargin),
      @"TopMargin" : @(kTopMargin),
      @"BottomMargin" : @(kBottomMargin),
    };
    NSArray* constraints = @[
      // Horitizontal constraints.
      @"H:|-(LeadingMargin)-[label]-(>=0)-|",
      // Vertical constraints.
      @"V:|-(TopMargin)-[label]-(BottomMargin)-|",
    ];
    ApplyVisualConstraintsWithMetrics(constraints, views, metrics);

    self.isAccessibilityElement = YES;
    self.accessibilityLabel =
        l10n_util::GetNSString(IDS_IOS_ACCOUNT_IDENTITY_CHOOSER_CHOOSE_ACCOUNT);
  }
  return self;
}

@end

@implementation IdentityChooserHeaderItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [IdentityChooserHeaderView class];
  }
  return self;
}

@end
