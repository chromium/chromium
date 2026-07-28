// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/ui/avatar/ai_tier_avatar_view.h"

#import "ios/chrome/browser/signin/model/constants.h"
#import "ios/chrome/browser/signin/ui/avatar/ai_tier_ring_image_view.h"
#import "ios/public/provider/chrome/browser/intelligence/signin/signin_ai_logo.h"

@implementation AITierAvatarView

- (instancetype)initWithAvatarImage:(UIImage*)avatarImage
                          outerSize:(CGFloat)outerSize
                    showsAITierRing:(BOOL)showsAITierRing {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    self.translatesAutoresizingMaskIntoConstraints = NO;

    CGFloat avatarDiameter = outerSize;
    if (showsAITierRing) {
      avatarDiameter =
          outerSize - 2.0 * (kAiTierRingWidth + kAiTierAndAvatarDistance);
    }

    _avatarImageView = [[UIImageView alloc] initWithImage:avatarImage];
    _avatarImageView.layer.cornerRadius = avatarDiameter / 2.0;
    _avatarImageView.clipsToBounds = YES;
    _avatarImageView.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_avatarImageView];

    [NSLayoutConstraint activateConstraints:@[
      [_avatarImageView.centerXAnchor
          constraintEqualToAnchor:self.centerXAnchor],
      [_avatarImageView.centerYAnchor
          constraintEqualToAnchor:self.centerYAnchor],
    ]];

    if (showsAITierRing) {
      [NSLayoutConstraint activateConstraints:@[
        [_avatarImageView.widthAnchor constraintEqualToConstant:avatarDiameter],
        [_avatarImageView.heightAnchor
            constraintEqualToAnchor:_avatarImageView.widthAnchor],
      ]];

      UIImage* ringImage = ios::provider::GetPremiumRingImage();
      _ringImageView = [[AITierRingImageView alloc] initWithImage:ringImage];
      _ringImageView.translatesAutoresizingMaskIntoConstraints = NO;
      _ringImageView.accessibilityIdentifier =
          kPremiumAvatarRingAccessibilityIdentifier;
      [self addSubview:_ringImageView];

      [NSLayoutConstraint activateConstraints:@[
        [_ringImageView.centerXAnchor
            constraintEqualToAnchor:self.centerXAnchor],
        [_ringImageView.centerYAnchor
            constraintEqualToAnchor:self.centerYAnchor],
        [_ringImageView.widthAnchor constraintEqualToAnchor:self.widthAnchor],
        [_ringImageView.heightAnchor constraintEqualToAnchor:self.heightAnchor],
      ]];
    } else {
      [NSLayoutConstraint activateConstraints:@[
        [_avatarImageView.widthAnchor constraintEqualToAnchor:self.widthAnchor],
        [_avatarImageView.heightAnchor
            constraintEqualToAnchor:self.heightAnchor],
      ]];
    }
  }
  return self;
}

@end
