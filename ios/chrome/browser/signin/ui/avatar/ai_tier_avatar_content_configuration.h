// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_UI_AVATAR_AI_TIER_AVATAR_CONTENT_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_SIGNIN_UI_AVATAR_AI_TIER_AVATAR_CONTENT_CONFIGURATION_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/content_configuration/chrome_content_configuration.h"

// Defines whether the configured diameter is for the avatar image or for the
// full view.
enum class AITierAvatarSizeType {
  // The diameter is for the avatar image; the ring is bigger.
  kAvatar,
  // The diameter is for the full view (including the ring if displayed).
  kFullView,
};

// A content configuration for an `AITierAvatarView`.
@interface AITierAvatarContentConfiguration
    : NSObject <ChromeContentConfiguration>

- (instancetype)initWithAvatarImage:(UIImage*)avatarImage
                           diameter:(CGFloat)diameter
                           sizeType:(AITierAvatarSizeType)sizeType
                    showsAITierRing:(BOOL)showsAITierRing
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// The avatar image to be displayed.
@property(nonatomic, readonly) UIImage* avatarImage;

// The diameter of the avatar or the full view, depending on `sizeType`.
@property(nonatomic, readonly) CGFloat diameter;

// Whether `diameter` is for the avatar (and the ring is bigger) or for the
// full view.
@property(nonatomic, readonly) AITierAvatarSizeType sizeType;

// Whether the AI tier ring should be displayed around the avatar.
@property(nonatomic, readonly) BOOL showsAITierRing;

@end

#endif  // IOS_CHROME_BROWSER_SIGNIN_UI_AVATAR_AI_TIER_AVATAR_CONTENT_CONFIGURATION_H_
