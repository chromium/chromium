// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_RESIZED_AVATAR_CACHE_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_RESIZED_AVATAR_CACHE_H_

#import <UIKit/UIKit.h>

#include "ios/chrome/browser/signin/model/constants.h"

@protocol SystemIdentity;

// This class manages an image cache for resized avatar images.
@interface ResizedAvatarCache : NSObject

// Initializes a new object with width and height of resized avatar.
- (instancetype)initWithSize:(CGSize)size NS_DESIGNATED_INITIALIZER;

// Initializes a new object based on `IdentityAvatarSize`.
- (instancetype)initWithIdentityAvatarSize:(IdentityAvatarSize)avatarSize;

- (instancetype)init NS_UNAVAILABLE;

// Returns cached resized image, if it exists. If the identity avatar has not
// yet been fetched, this method triggers a fetch and returns the default
// avatar image. The user of this class should be an observer of identity
// updates. When notified of identity updates, this method should be called
// again to obtain an updated resized image.
- (UIImage*)resizedAvatarForIdentity:(id<SystemIdentity>)identity;
@end

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_RESIZED_AVATAR_CACHE_H_
