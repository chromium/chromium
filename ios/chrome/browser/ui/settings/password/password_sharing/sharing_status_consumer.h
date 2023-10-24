// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_SHARING_STATUS_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_SHARING_STATUS_CONSUMER_H_

class GURL;

// Provides profile images of the sender, recipients and strings to be displayed
// in the success view.
@protocol SharingStatusConsumer <NSObject>

// Sets circular profile image of the password sender (current user).
- (void)setSenderImage:(UIImage*)senderImage;

// Sets circular image of the recipient (or merged images of multiple
// recipients).
- (void)setRecipientImage:(UIImage*)recipientImage;

// Sets subtitle string to be displayed in the success status view.
- (void)setSubtitleString:(NSString*)subtitleString;

// Sets footer string to be displayed in the success status view.
- (void)setFooterString:(NSString*)footerString;

// Sets url of the site for which the passwords are being shared.
- (void)setURL:(const GURL&)URL;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_SHARING_STATUS_CONSUMER_H_
