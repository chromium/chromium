// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/ui/avatar/ai_tier_avatar_view.h"

#import "ios/chrome/browser/signin/model/constants.h"
#import "ios/chrome/browser/signin/ui/avatar/ai_tier_ring_image_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/image_util.h"
#import "ios/public/provider/chrome/browser/intelligence/signin/signin_ai_logo.h"

@implementation AITierAvatarView {
  // The avatar image view.
  UIImageView* _avatarImageView;

  // The ring image view, if visible.
  AITierRingImageView* _ringImageView;
}

- (instancetype)initWithAvatarImage:(UIImage*)avatarImage
                     avatarDiameter:(CGFloat)avatarDiameter
                    showsAITierRing:(BOOL)showsAITierRing {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    self.translatesAutoresizingMaskIntoConstraints = NO;
    self.clipsToBounds = YES;

    _avatarImageView = [[UIImageView alloc] initWithImage:avatarImage];
    _avatarImageView.layer.cornerRadius = avatarDiameter / 2.0;
    _avatarImageView.clipsToBounds = YES;
    _avatarImageView.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_avatarImageView];

    AddSquareConstraints(_avatarImageView, avatarDiameter);

    if (showsAITierRing) {
      CGFloat ringDiameter =
          avatarDiameter + 2.0 * (kAiTierRingWidth + kAiTierAndAvatarDistance);

      UIImage* ringImage = ios::provider::GetPremiumRingImage();
      CGSize ringSize = CGSizeMake(ringDiameter, ringDiameter);
      ringImage = ResizeImage(ringImage, ringSize, ProjectionMode::kAspectFit);
      _ringImageView = [[AITierRingImageView alloc] initWithImage:ringImage];
      _ringImageView.translatesAutoresizingMaskIntoConstraints = NO;
      _ringImageView.accessibilityIdentifier =
          kPremiumAvatarRingAccessibilityIdentifier;

      [self addSubview:_ringImageView];
      AddSameConstraints(_ringImageView, self);
      AddSameCenterConstraints(_avatarImageView, self);
    } else {
      AddSameConstraints(_avatarImageView, self);
    }
  }
  return self;
}

@end
