// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LEVEL_UP_UI_LEVEL_UP_PROFILE_CONSUMER_H_
#define IOS_CHROME_BROWSER_LEVEL_UP_UI_LEVEL_UP_PROFILE_CONSUMER_H_

#import <UIKit/UIKit.h>

// Consumer for the Level Up user profile welcome header.
@protocol LevelUpProfileConsumer <NSObject>

// Sets the signed-in user credentials.
- (void)setUserFullName:(NSString*)userFullName userAvatar:(UIImage*)userAvatar;

@end

#endif  // IOS_CHROME_BROWSER_LEVEL_UP_UI_LEVEL_UP_PROFILE_CONSUMER_H_
