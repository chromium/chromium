// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/ui/avatar/ai_tier_avatar_content_configuration.h"

#import "ios/chrome/browser/signin/model/constants.h"
#import "ios/chrome/browser/signin/ui/avatar/ai_tier_avatar_view.h"

@implementation AITierAvatarContentConfiguration

- (instancetype)initWithAvatarImage:(UIImage*)avatarImage
                           diameter:(CGFloat)diameter
                           sizeType:(AITierAvatarSizeType)sizeType
                    showsAITierRing:(BOOL)showsAITierRing {
  self = [super init];
  if (self) {
    _avatarImage = avatarImage;
    _diameter = diameter;
    _sizeType = sizeType;
    _showsAITierRing = showsAITierRing;
  }
  return self;
}

#pragma mark - ChromeContentConfiguration

- (UIView<ChromeContentView>*)makeChromeContentView {
  return [[AITierAvatarView alloc] initWithConfiguration:self];
}

- (CGSize)contentSize {
  CGFloat viewDiameter = self.diameter;
  if (self.showsAITierRing && self.sizeType == AITierAvatarSizeType::kAvatar) {
    viewDiameter += 2.0 * (kAiTierRingWidth + kAiTierAndAvatarDistance);
  }
  return CGSizeMake(viewDiameter, viewDiameter);
}

#pragma mark - UIContentConfiguration

- (id<UIContentView>)makeContentView {
  return [self makeChromeContentView];
}

- (instancetype)updatedConfigurationForState:(id<UIConfigurationState>)state {
  return self;
}

#pragma mark - NSCopying

- (id)copyWithZone:(NSZone*)zone {
  // Class is immutable, -copy is just a refcount increase.
  return self;
}

@end
