// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/views/identity_view.h"

#import "base/check.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/UIColor+cr_semantic_colors.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Sizes.
const CGFloat kAvatarSize = 40.;
// Distances/margins.
const CGFloat kTitleOffset = 4;
const CGFloat kHorizontalAvatarMargin = 16.;
const CGFloat kVerticalMargin = 12.;

}  // namespace

@interface IdentityView ()

// Avatar.
@property(nonatomic, strong) UIImageView* avatarView;
// Contains the name if it exists, otherwise it contains the email.
@property(nonatomic, strong) UILabel* title;
// Contains the email if the name exists, otherwise it is hidden.
@property(nonatomic, strong) UILabel* subtitle;
// Constraints if the name exists.
@property(nonatomic, strong) NSLayoutConstraint* titleConstraintForNameAndEmail;
// Constraints if the name doesn't exist.
@property(nonatomic, strong) NSLayoutConstraint* titleConstraintForEmailOnly;
// Constraints to update when |self.minimumTopMargin| is updated.
@property(nonatomic, strong) NSArray<NSLayoutConstraint*>* topConstraints;
// Constraints to update when |self.minimumBottomMargin| is updated.
@property(nonatomic, strong) NSArray<NSLayoutConstraint*>* bottomConstraints;
// Constraints for the avatar size.
@property(nonatomic, strong)
    NSArray<NSLayoutConstraint*>* avatarSizeConstraints;
// Constraints to update when |self.avatarTitleMargin| is updated.
@property(nonatomic, strong)
    NSLayoutConstraint* avatarTitleHorizontalConstraint;

@end

@implementation IdentityView

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.userInteractionEnabled = NO;
    // Avatar view.
    _avatarView = [[UIImageView alloc] init];
    _avatarView.translatesAutoresizingMaskIntoConstraints = NO;
    _avatarView.clipsToBounds = YES;
    [self addSubview:_avatarView];

    // Title.
    _title = [[UILabel alloc] init];
    _title.adjustsFontForContentSizeCategory = YES;
    _title.translatesAutoresizingMaskIntoConstraints = NO;
    _title.numberOfLines = 0;
    _title.textColor = UIColor.cr_labelColor;
    _title.font = [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
    [self addSubview:_title];

    // Subtitle.
    _subtitle = [[UILabel alloc] init];
    _subtitle.adjustsFontForContentSizeCategory = YES;
    _subtitle.translatesAutoresizingMaskIntoConstraints = NO;
    _subtitle.numberOfLines = 0;
    _subtitle.textColor = UIColor.cr_secondaryLabelColor;
    _subtitle.font = [UIFont preferredFontForTextStyle:UIFontTextStyleCaption1];
    [self addSubview:_subtitle];

    // Text container.
    UILayoutGuide* textContainerGuide = [[UILayoutGuide alloc] init];
    [self addLayoutGuide:textContainerGuide];

    // Layout constraints.
    _avatarTitleHorizontalConstraint = [textContainerGuide.leadingAnchor
        constraintEqualToAnchor:_avatarView.trailingAnchor
                       constant:kHorizontalAvatarMargin];
    [NSLayoutConstraint activateConstraints:@[
      _avatarTitleHorizontalConstraint,
      [_avatarView.leadingAnchor
          constraintEqualToAnchor:self.leadingAnchor
                         constant:kHorizontalAvatarMargin]
    ]];
    _avatarSizeConstraints = @[
      [_avatarView.heightAnchor constraintEqualToConstant:kAvatarSize],
      [_avatarView.widthAnchor constraintEqualToConstant:kAvatarSize],
    ];
    [NSLayoutConstraint activateConstraints:_avatarSizeConstraints];
    AddSameCenterYConstraint(self, _avatarView);
    AddSameConstraintsToSides(_title, _subtitle,
                              LayoutSides::kLeading | LayoutSides::kTrailing);
    _titleConstraintForNameAndEmail =
        [_subtitle.topAnchor constraintEqualToAnchor:_title.bottomAnchor
                                            constant:kTitleOffset];
    _titleConstraintForEmailOnly = [textContainerGuide.bottomAnchor
        constraintEqualToAnchor:_title.bottomAnchor];

    [NSLayoutConstraint activateConstraints:@[
      [self.centerYAnchor
          constraintEqualToAnchor:textContainerGuide.centerYAnchor],
      [textContainerGuide.leadingAnchor
          constraintEqualToAnchor:_title.leadingAnchor],
      [textContainerGuide.leadingAnchor
          constraintEqualToAnchor:_subtitle.leadingAnchor],
      [textContainerGuide.trailingAnchor
          constraintEqualToAnchor:_title.trailingAnchor],
      [textContainerGuide.trailingAnchor
          constraintEqualToAnchor:_subtitle.trailingAnchor],
      [textContainerGuide.topAnchor constraintEqualToAnchor:_title.topAnchor],
      [textContainerGuide.bottomAnchor
          constraintEqualToAnchor:_subtitle.bottomAnchor],
    ]];

    _topConstraints = @[
      [_avatarView.topAnchor
          constraintGreaterThanOrEqualToAnchor:self.topAnchor
                                      constant:kVerticalMargin],
      [_title.topAnchor constraintGreaterThanOrEqualToAnchor:self.topAnchor
                                                    constant:kVerticalMargin],
    ];
    [NSLayoutConstraint activateConstraints:_topConstraints];
    _bottomConstraints = @[
      [self.bottomAnchor
          constraintGreaterThanOrEqualToAnchor:_avatarView.bottomAnchor
                                      constant:kVerticalMargin],
      [self.bottomAnchor
          constraintGreaterThanOrEqualToAnchor:_subtitle.bottomAnchor
                                      constant:kVerticalMargin],
    ];
    [NSLayoutConstraint activateConstraints:_bottomConstraints];
  }
  return self;
}

#pragma mark - Setter

- (void)setAvatar:(UIImage*)avatarImage {
  if (avatarImage) {
    self.avatarView.image = avatarImage;
    self.avatarView.layer.cornerRadius = self.avatarSize / 2.0;
  } else {
    self.avatarView.image = nil;
  }
}

- (void)setTitle:(NSString*)title subtitle:(NSString*)subtitle {
  DCHECK(title);
  self.title.text = title;
  if (!subtitle.length) {
    self.titleConstraintForNameAndEmail.active = NO;
    self.titleConstraintForEmailOnly.active = YES;
    self.subtitle.hidden = YES;
  } else {
    self.titleConstraintForEmailOnly.active = NO;
    self.titleConstraintForNameAndEmail.active = YES;
    self.subtitle.hidden = NO;
    self.subtitle.text = subtitle;
  }
}

#pragma mark - properties

- (void)setMinimumTopMargin:(CGFloat)minimumTopMargin {
  for (NSLayoutConstraint* constraint in self.topConstraints) {
    constraint.constant = minimumTopMargin;
  }
}

- (CGFloat)minimumTopMargin {
  DCHECK(self.topConstraints.count);
  return self.topConstraints[0].constant;
}

- (void)setMinimumBottomMargin:(CGFloat)minimumBottomMargin {
  for (NSLayoutConstraint* constraint in self.bottomConstraints) {
    constraint.constant = minimumBottomMargin;
  }
}

- (CGFloat)minimumBottomMargin {
  DCHECK(self.bottomConstraints.count);
  return self.bottomConstraints[0].constant;
}

- (void)setAvatarSize:(CGFloat)avatarSize {
  for (NSLayoutConstraint* constraint in self.avatarSizeConstraints) {
    constraint.constant = avatarSize;
  }
  self.avatarView.layer.cornerRadius = avatarSize / 2.0;
}

- (CGFloat)avatarSize {
  DCHECK(self.avatarSizeConstraints.count);
  return self.avatarSizeConstraints[0].constant;
}

- (void)setAvatarTitleMargin:(CGFloat)avatarTitleMargin {
  self.avatarTitleHorizontalConstraint.constant = avatarTitleMargin;
}

- (CGFloat)avatarTitleMargin {
  return self.avatarTitleHorizontalConstraint.constant;
}

- (void)setTitleSubtitleMargin:(CGFloat)titleSubtitleMargin {
  self.titleConstraintForNameAndEmail.constant = titleSubtitleMargin;
}

- (CGFloat)titleSubtitleMargin {
  return self.titleConstraintForNameAndEmail.constant;
}

- (void)setTitleFont:(UIFont*)titleFont {
  self.title.font = titleFont;
}

- (UIFont*)titleFont {
  return self.title.font;
}

- (void)setSubtitleFont:(UIFont*)subtitleFont {
  self.subtitle.font = subtitleFont;
}

- (UIFont*)subtitleFont {
  return self.subtitleFont;
}

@end
