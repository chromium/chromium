// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/cells/table_view_signin_promo_item.h"

#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_configurator.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The inner insets of the View content.
const CGFloat kMargin = 16;
}

#pragma mark - TableViewSigninPromoItem

@implementation TableViewSigninPromoItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [TableViewSigninPromoCell class];
  }
  return self;
}

- (void)configureCell:(TableViewCell*)tableCell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:tableCell withStyler:styler];
  TableViewSigninPromoCell* cell =
      base::mac::ObjCCastStrict<TableViewSigninPromoCell>(tableCell);
  cell.signinPromoView.delegate = self.delegate;
  cell.signinPromoView.textLabel.text = self.text;
  [cell setSelectionStyle:UITableViewCellSelectionStyleNone];
  [self.configurator configureSigninPromoView:cell.signinPromoView];
  if (styler.cellTitleColor)
    cell.signinPromoView.textLabel.textColor = styler.cellTitleColor;
  if (styler.tintColor) {
    cell.signinPromoView.primaryButton.backgroundColor = styler.tintColor;
    [cell.signinPromoView.secondaryButton setTitleColor:styler.tintColor
                                               forState:UIControlStateNormal];
  }
  if (styler.solidButtonTextColor) {
    [cell.signinPromoView.primaryButton
        setTitleColor:styler.solidButtonTextColor
             forState:UIControlStateNormal];
  }
}

@end

#pragma mark - TableViewSigninPromoCell

@implementation TableViewSigninPromoCell
@synthesize signinPromoView = _signinPromoView;

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    SigninPromoView* signinPromoView =
        [[SigninPromoView alloc] initWithFrame:CGRectZero];
    self.signinPromoView = signinPromoView;
    self.signinPromoView.translatesAutoresizingMaskIntoConstraints = NO;
    [self.contentView addSubview:self.signinPromoView];

    // Set and activate constraints.
    [NSLayoutConstraint activateConstraints:@[
      [self.signinPromoView.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:kMargin],
      [self.signinPromoView.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-kMargin],
      [self.signinPromoView.topAnchor
          constraintEqualToAnchor:self.contentView.topAnchor
                         constant:kMargin],
      [self.signinPromoView.bottomAnchor
          constraintEqualToAnchor:self.contentView.bottomAnchor
                         constant:-kMargin],
    ]];
  }
  return self;
}

@end
