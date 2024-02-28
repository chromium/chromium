// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/cells/central_account_view.h"

#import "base/apple/foundation_util.h"
#import "base/check_op.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/signin/model/constants.h"
#import "ios/chrome/browser/signin/model/signin_util.h"
#import "ios/chrome/browser/ui/settings/cells/settings_cells_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

@implementation CentralAccountView {
  // Rounded avatarImage used for the account user picture. Note: the image
  // doesn't need to be rounded as the cell configs create the image rounded
  // corners.
  UIImage* _avatarImage;
  // Name displayed in main label.
  NSString* _name;
  // Email subtitle displayed in secondary label.
  NSString* _email;
}

- (instancetype)initWithFrame:(CGRect)frame
                  avatarImage:(UIImage*)avatarImage
                         name:(NSString*)name
                        email:(NSString*)email {
  self = [super initWithFrame:frame];
  if (self) {
    CHECK(avatarImage);
    CHECK(email);
    _avatarImage = avatarImage;
    _name = name ? name : email;
    _email = name ? email : nil;

    UIImageView* imageView = [[UIImageView alloc] initWithImage:_avatarImage];
    // Creates the image rounded corners.
    imageView.layer.cornerRadius =
        GetSizeForIdentityAvatarSize(IdentityAvatarSize::Large).width / 2.0f;
    imageView.clipsToBounds = YES;
    imageView.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:imageView];

    UILabel* titleLabel = [[UILabel alloc] init];
    titleLabel.text = _name;
    titleLabel.textAlignment = NSTextAlignmentCenter;
    titleLabel.numberOfLines = 1;
    titleLabel.lineBreakMode = NSLineBreakByTruncatingTail;
    titleLabel.adjustsFontForContentSizeCategory = YES;
    titleLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
    titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
    titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:titleLabel];

    UILabel* subtitleLabel = [[UILabel alloc] init];
    subtitleLabel.text = _email;
    subtitleLabel.textAlignment = NSTextAlignmentCenter;
    subtitleLabel.numberOfLines = 1;
    subtitleLabel.adjustsFontForContentSizeCategory = YES;
    subtitleLabel.lineBreakMode = NSLineBreakByTruncatingTail;
    subtitleLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
    subtitleLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    subtitleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:subtitleLabel];

    [NSLayoutConstraint activateConstraints:@[
      [imageView.centerXAnchor constraintEqualToAnchor:self.centerXAnchor],
      [imageView.topAnchor
          constraintEqualToAnchor:self.topAnchor
                         constant:kTableViewLargeVerticalSpacing],
      [imageView.widthAnchor
          constraintEqualToConstant:GetSizeForIdentityAvatarSize(
                                        IdentityAvatarSize::Large)
                                        .width],
      [imageView.heightAnchor constraintEqualToAnchor:imageView.widthAnchor],

      [titleLabel.topAnchor constraintEqualToAnchor:imageView.bottomAnchor
                                           constant:kTableViewVerticalSpacing],
      [titleLabel.leadingAnchor
          constraintEqualToAnchor:self.leadingAnchor
                         constant:kTableViewHorizontalSpacing],
      [titleLabel.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor
                         constant:-kTableViewHorizontalSpacing],

      [subtitleLabel.topAnchor constraintEqualToAnchor:titleLabel.bottomAnchor],
      [subtitleLabel.leadingAnchor
          constraintEqualToAnchor:titleLabel.leadingAnchor],
      [subtitleLabel.trailingAnchor
          constraintEqualToAnchor:titleLabel.trailingAnchor],
      [self.bottomAnchor
          constraintEqualToAnchor:subtitleLabel.bottomAnchor
                         constant:2 * kTableViewLargeVerticalSpacing],
    ]];

    CGSize size =
        [self systemLayoutSizeFittingSize:self.frame.size
            withHorizontalFittingPriority:UILayoutPriorityRequired
                  verticalFittingPriority:UILayoutPriorityFittingSizeLevel];
    CGRect newFrame = CGRectZero;
    newFrame.size = size;
    self.frame = newFrame;
  }
  return self;
}

- (UIImage*)avatarImage {
  return _avatarImage;
}

- (NSString*)name {
  return _name;
}

- (NSString*)email {
  return _email;
}

@end
