// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_HISTORY_SYNC_HISTORY_SYNC_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_HISTORY_SYNC_HISTORY_SYNC_CONSUMER_H_

#import <UIKit/UIKit.h>

// Handles history sync UI updates
@protocol HistorySyncConsumer

// Set the avatar image for the primary identity
- (void)setPrimaryIdentityAvatarImage:(UIImage*)primaryIdentityAvatarImage;

// Set the avatar label for the primary identity
- (void)setPrimaryIdentityAvatarAccessibilityLabel:
    (NSString*)primaryIdentityAvatarAccessibilityLabel;

// Set the text for the disclaimer footer.
- (void)setFooterText:(NSString*)footerText;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_HISTORY_SYNC_HISTORY_SYNC_CONSUMER_H_
