// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WELCOME_BACK_UI_WELCOME_BACK_SCREEN_CONSUMER_H_
#define IOS_CHROME_BROWSER_WELCOME_BACK_UI_WELCOME_BACK_SCREEN_CONSUMER_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

@class BestFeaturesItem;

// Defines methods to set the contents of the Welcome Back Screen.
@protocol WelcomeBackScreenConsumer <NSObject>

// Sets the list of items for the WelcomeBackScreen Screen.
- (void)setWelcomeBackItems:(NSArray<BestFeaturesItem*>*)items;

// The title string for the Welcome Back Promo.
- (void)setTitle:(NSString*)titleString;

// Sets the avatar of the user.
- (void)setAvatar:(UIImage*)userAvatar;

@end

#endif  // IOS_CHROME_BROWSER_WELCOME_BACK_UI_WELCOME_BACK_SCREEN_CONSUMER_H_
