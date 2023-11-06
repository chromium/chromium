// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/cells/table_view_signin_promo_item.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_configurator.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_constants.h"

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
      base::apple::ObjCCastStrict<TableViewSigninPromoCell>(tableCell);
  cell.signinPromoView.delegate = self.delegate;
  cell.signinPromoView.textLabel.text = self.text;
  [cell setSelectionStyle:UITableViewCellSelectionStyleNone];
  [self.configurator configureSigninPromoView:cell.signinPromoView
                                    withStyle:SigninPromoViewStyleStandard];
  if (styler.cellTitleColor)
    cell.signinPromoView.textLabel.textColor = styler.cellTitleColor;
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

- (void)prepareForReuse {
  [super prepareForReuse];
  [self.signinPromoView prepareForReuse];
}

@end
