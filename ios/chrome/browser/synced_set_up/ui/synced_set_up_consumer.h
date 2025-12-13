// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNCED_SET_UP_UI_SYNCED_SET_UP_CONSUMER_H_
#define IOS_CHROME_BROWSER_SYNCED_SET_UP_UI_SYNCED_SET_UP_CONSUMER_H_

#import <UIKit/UIKit.h>

// Consumer for the model to push user details (name, avatar) to the Synced Set
// Up welcome screen.
@protocol SyncedSetUpConsumer

// Sets the main welcome `message` to be displayed.
- (void)setWelcomeMessage:(NSString*)message;

// Sets the avatar `image` to be displayed.
- (void)setAvatarImage:(UIImage*)image;

@end

#endif  // IOS_CHROME_BROWSER_SYNCED_SET_UP_UI_SYNCED_SET_UP_CONSUMER_H_
