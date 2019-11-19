// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_RESIZED_AVATAR_CACHE_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_RESIZED_AVATAR_CACHE_H_

#import <UIKit/UIKit.h>

@class ChromeIdentity;
@class UIImage;

// This class manages an image cache for resized avatar images.
@interface ResizedAvatarCache : NSObject

// Initializes a new object with width and height of resized avatar.
- (instancetype)initWithSize:(CGSize)size NS_DESIGNATED_INITIALIZER;

// Initializes a new object with default size.
- (instancetype)init;

// Returns cached resized image, if it exists. If the identity avatar has not
// yet been fetched, this method triggers a fetch and returns the default
// avatar image. The user of this class should be an observer of identity
// updates. When notified of identity updates, this method should be called
// again to obtain an updated resized image.
- (UIImage*)resizedAvatarForIdentity:(ChromeIdentity*)identity;
@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_RESIZED_AVATAR_CACHE_H_
