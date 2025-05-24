// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARE_KIT_MODEL_SHARE_KIT_AVATAR_PRIMITIVE_H_
#define IOS_CHROME_BROWSER_SHARE_KIT_MODEL_SHARE_KIT_AVATAR_PRIMITIVE_H_

#import <UIKit/UIKit.h>

// Protocol to provide an avatar image.
@protocol ShareKitAvatarPrimitive <NSObject>

// Returns the UIView that contains an avatar image.
- (UIView*)view;

// Resolves an image backing the avatar view.
- (void)resolve;

@end

#endif  // IOS_CHROME_BROWSER_SHARE_KIT_MODEL_SHARE_KIT_AVATAR_PRIMITIVE_H_
