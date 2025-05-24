// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARE_KIT_MODEL_SHARE_KIT_AVATAR_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_SHARE_KIT_MODEL_SHARE_KIT_AVATAR_CONFIGURATION_H_

#import <Foundation/Foundation.h>

// Configuration object to get an avatar image from ShareKit.
@interface ShareKitAvatarConfiguration : NSObject

// The URL of the avatar image.
@property(nonatomic, copy) NSURL* avatarUrl;

// The name of the user or the email if the name is empty.
@property(nonatomic, copy) NSString* displayName;

// The size of the avatar image.
@property(nonatomic, assign) CGSize avatarSize;

@end

#endif  // IOS_CHROME_BROWSER_SHARE_KIT_MODEL_SHARE_KIT_AVATAR_CONFIGURATION_H_
