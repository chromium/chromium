// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_UI_AVATAR_AI_TIER_AVATAR_VIEW_H_
#define IOS_CHROME_BROWSER_SIGNIN_UI_AVATAR_AI_TIER_AVATAR_VIEW_H_

#import <UIKit/UIKit.h>

// A view containing an avatar image view and, optionally, an AI tier outer
// ring.
@interface AITierAvatarView : UIView

// The inner avatar image view.
@property(nonatomic, readonly) UIImageView* avatarImageView;

// The ring image view, if visible.
@property(nonatomic, readonly) UIImageView* ringImageView;

- (instancetype)initWithAvatarImage:(UIImage*)avatarImage
                          outerSize:(CGFloat)outerSize
                    showsAITierRing:(BOOL)showsAITierRing
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_SIGNIN_UI_AVATAR_AI_TIER_AVATAR_VIEW_H_
