// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/ui/avatar/ai_tier_avatar_view.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/signin/model/constants.h"
#import "ios/chrome/browser/signin/ui/avatar/ai_tier_avatar_content_configuration.h"
#import "ios/chrome/browser/signin/ui/avatar/ai_tier_ring_image_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/image_util.h"
#import "ios/public/provider/chrome/browser/intelligence/signin/signin_ai_logo.h"

@implementation AITierAvatarView {
  AITierAvatarContentConfiguration* _configuration;

  // The avatar image view.
  UIImageView* _avatarImageView;

  // The ring image view, if visible.
  AITierRingImageView* _ringImageView;
}

- (instancetype)initWithConfiguration:
    (AITierAvatarContentConfiguration*)configuration {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    self.translatesAutoresizingMaskIntoConstraints = NO;
    self.clipsToBounds = YES;

    _configuration = [configuration copy];
    [self applyConfiguration];
  }
  return self;
}

- (instancetype)initWithAvatarImage:(UIImage*)avatarImage
                     avatarDiameter:(CGFloat)avatarDiameter
                    showsAITierRing:(BOOL)showsAITierRing {
  AITierAvatarContentConfiguration* configuration =
      [[AITierAvatarContentConfiguration alloc]
          initWithAvatarImage:avatarImage
                     diameter:avatarDiameter
                     sizeType:AITierAvatarSizeType::kAvatar
              showsAITierRing:showsAITierRing];
  return [self initWithConfiguration:configuration];
}

#pragma mark - ChromeContentView

- (BOOL)hasCustomAccessibilityActivationPoint {
  return NO;
}

#pragma mark - UIContentView

- (id<UIContentConfiguration>)configuration {
  return _configuration;
}

- (void)setConfiguration:(id<UIContentConfiguration>)configuration {
  _configuration =
      [base::apple::ObjCCastStrict<AITierAvatarContentConfiguration>(
          configuration) copy];
  [self applyConfiguration];
}

- (BOOL)supportsConfiguration:(id<UIContentConfiguration>)configuration {
  return [configuration isMemberOfClass:AITierAvatarContentConfiguration.class];
}

#pragma mark - Private

// Updates the view with the current `_configuration`.
- (void)applyConfiguration {
  [_avatarImageView removeFromSuperview];
  [_ringImageView removeFromSuperview];
  _ringImageView = nil;

  BOOL showsAITierRing = _configuration.showsAITierRing;
  CGFloat avatarDiameter = _configuration.diameter;
  CGFloat ringDiameter = 0;
  if (showsAITierRing) {
    switch (_configuration.sizeType) {
      case AITierAvatarSizeType::kAvatar:
        ringDiameter = avatarDiameter +
                       2.0 * (kAiTierRingWidth + kAiTierAndAvatarDistance);
        break;
      case AITierAvatarSizeType::kFullView:
        ringDiameter = avatarDiameter;
        avatarDiameter =
            ringDiameter - 2.0 * (kAiTierRingWidth + kAiTierAndAvatarDistance);
        break;
    }
  }

  _avatarImageView =
      [[UIImageView alloc] initWithImage:_configuration.avatarImage];
  _avatarImageView.layer.cornerRadius = avatarDiameter / 2.0;
  _avatarImageView.clipsToBounds = YES;
  _avatarImageView.translatesAutoresizingMaskIntoConstraints = NO;
  _avatarImageView.accessibilityIdentifier =
      kIdentityAvatarImageAccessibilityIdentifier;
  [self addSubview:_avatarImageView];

  AddSquareConstraints(_avatarImageView, avatarDiameter);

  if (showsAITierRing) {
    UIImage* discImage = ios::provider::GetPremiumDiscImage();
    CGSize ringSize = CGSizeMake(ringDiameter, ringDiameter);
    discImage = ResizeImage(discImage, ringSize, ProjectionMode::kAspectFit);
    _ringImageView = [[AITierRingImageView alloc] initWithImage:discImage];
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

@end
