// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_TANGIBLE_SYNC_TANGIBLE_SYNC_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_TANGIBLE_SYNC_TANGIBLE_SYNC_CONSUMER_H_

#import <UIKit/UIKit.h>

// Handles tangible sync UI updates.
@protocol TangibleSyncConsumer

// Avatar image for the primary identity.
@property(nonatomic, strong) UIImage* primaryIdentityAvatarImage;

// Sets the UI as interactable or not.
- (void)setUIEnabled:(BOOL)UIEnabled;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_TANGIBLE_SYNC_TANGIBLE_SYNC_CONSUMER_H_
