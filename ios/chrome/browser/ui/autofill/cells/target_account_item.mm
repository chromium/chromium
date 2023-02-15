// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/cells/target_account_item.h"

#import "base/mac/foundation_util.h"
#import "build/branding_buildflags.h"
#import "components/grit/components_scaled_resources.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_cell.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Size of the avatar and Google pay badges.
const CGFloat kAccountCellBadgeSize = 18;
// Spacing between each of the 3 elements (avatar, email and Google pay icon).
const CGFloat kAccountCellSpacing = 7;

}  // namespace

#pragma mark - TargetAccountItem

@implementation TargetAccountItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [TargetAccountCell class];
  }
  return self;
}

- (void)configureCell:(TableViewCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];
  TargetAccountCell* accountCell =
      base::mac::ObjCCastStrict<TargetAccountCell>(cell);
  accountCell.avatarBadge.image = self.avatar;
  accountCell.emailLabel.text = self.email;
}

@end

#pragma mark - TargetAccountCell

@implementation TargetAccountCell

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];

  if (!self) {
    return nil;
  }

  _avatarBadge = [[UIImageView alloc] init];
  _avatarBadge.translatesAutoresizingMaskIntoConstraints = NO;
  _avatarBadge.contentMode = UIViewContentModeScaleAspectFit;
  _avatarBadge.layer.cornerRadius = kAccountCellBadgeSize / 2;
  _avatarBadge.clipsToBounds = YES;
  [self.contentView addSubview:_avatarBadge];

  _emailLabel = [[UILabel alloc] init];
  _emailLabel.translatesAutoresizingMaskIntoConstraints = NO;
  _emailLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  _emailLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  [self.contentView addSubview:_emailLabel];

  UIImageView* googlePayBadge = [[UIImageView alloc] init];
  googlePayBadge.translatesAutoresizingMaskIntoConstraints = NO;
  googlePayBadge.contentMode = UIViewContentModeScaleAspectFit;
  googlePayBadge.image = NativeImage(IDR_AUTOFILL_GOOGLE_PAY);
// IDR_AUTOFILL_GOOGLE_PAY_DARK only exists in official builds.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (UITraitCollection.currentTraitCollection.userInterfaceStyle ==
      UIUserInterfaceStyleDark) {
    googlePayBadge.image = NativeImage(IDR_AUTOFILL_GOOGLE_PAY_DARK);
  }
#endif
  [self.contentView addSubview:googlePayBadge];

  [NSLayoutConstraint activateConstraints:@[
    [_avatarBadge.centerYAnchor
        constraintEqualToAnchor:self.contentView.centerYAnchor],
    [_avatarBadge.heightAnchor constraintEqualToConstant:kAccountCellBadgeSize],
    [_avatarBadge.widthAnchor constraintEqualToConstant:kAccountCellBadgeSize],
    [_avatarBadge.leftAnchor
        constraintEqualToAnchor:self.contentView.leftAnchor
                       constant:kTableViewHorizontalSpacing],
    [_emailLabel.centerYAnchor
        constraintEqualToAnchor:_avatarBadge.centerYAnchor],
    [_emailLabel.leftAnchor constraintEqualToAnchor:_avatarBadge.rightAnchor
                                           constant:kAccountCellSpacing],
    [googlePayBadge.centerYAnchor
        constraintEqualToAnchor:_emailLabel.centerYAnchor],
    [googlePayBadge.heightAnchor
        constraintEqualToConstant:kAccountCellBadgeSize],
    [googlePayBadge.leftAnchor constraintEqualToAnchor:_emailLabel.rightAnchor
                                              constant:kAccountCellSpacing],
    [googlePayBadge.rightAnchor
        constraintEqualToAnchor:self.contentView.rightAnchor
                       constant:-kTableViewHorizontalSpacing],
  ]];

  return self;
}
@end
