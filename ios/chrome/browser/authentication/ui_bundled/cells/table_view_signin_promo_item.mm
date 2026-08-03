// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/cells/table_view_signin_promo_item.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/authentication/ui_bundled/cells/signin_promo_view.h"
#import "ios/chrome/browser/authentication/ui_bundled/cells/signin_promo_view_configurator.h"
#import "ios/chrome/browser/authentication/ui_bundled/cells/signin_promo_view_constants.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {
// The inner insets of the View content.
const CGFloat kMargin = 16;
}  // namespace

#pragma mark - TableViewSigninPromoItem

@implementation TableViewSigninPromoItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [TableViewSigninPromoCell class];
  }
  return self;
}

- (void)configureCell:(LegacyTableViewCell*)tableCell {
  [super configureCell:tableCell];
  TableViewSigninPromoCell* cell =
      base::apple::ObjCCastStrict<TableViewSigninPromoCell>(tableCell);
  cell.signinPromoView.delegate = self.delegate;
  cell.signinPromoView.textLabel.text = self.text;
  [cell setSelectionStyle:UITableViewCellSelectionStyleNone];
  [self.configurator configureSigninPromoView:cell.signinPromoView
                                    withStyle:SigninPromoViewStyleStandard];
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
    AddSameConstraintsWithInset(self.signinPromoView, self.contentView,
                                kMargin);
  }
  return self;
}

- (void)prepareForReuse {
  [super prepareForReuse];
  [self.signinPromoView prepareForReuse];
}

@end
