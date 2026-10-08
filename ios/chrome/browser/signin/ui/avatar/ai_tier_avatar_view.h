// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_UI_AVATAR_AI_TIER_AVATAR_VIEW_H_
#define IOS_CHROME_BROWSER_SIGNIN_UI_AVATAR_AI_TIER_AVATAR_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/content_configuration/chrome_content_view.h"

@class AITierAvatarContentConfiguration;

// A view containing an avatar image view and, optionally, an AI tier outer
// ring.
@interface AITierAvatarView : UIView <ChromeContentView>

// Initializes the view with a content `configuration`.
- (instancetype)initWithConfiguration:
    (AITierAvatarContentConfiguration*)configuration NS_DESIGNATED_INITIALIZER;

// Convenience initializer with `avatarImage`, `avatarDiameter`, and
// `showsAITierRing`.
- (instancetype)initWithAvatarImage:(UIImage*)avatarImage
                     avatarDiameter:(CGFloat)avatarDiameter
                    showsAITierRing:(BOOL)showsAITierRing;

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_SIGNIN_UI_AVATAR_AI_TIER_AVATAR_VIEW_H_
