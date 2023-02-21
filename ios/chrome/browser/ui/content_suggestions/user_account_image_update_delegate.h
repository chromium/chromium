// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_USER_ACCOUNT_IMAGE_UPDATE_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_USER_ACCOUNT_IMAGE_UPDATE_DELEGATE_H_

// Delegate protocol to update UI with current user account avatar.
@protocol UserAccountImageUpdateDelegate

// Sets up an avatar for signed-out state.
- (void)setSignedOutAccountImage;
// Updates current signed-in user account avatar with supplied image.
// `image` and `email` must not be nil.
- (void)updateAccountImage:(UIImage*)image
                      name:(NSString*)name
                     email:(NSString*)email;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_USER_ACCOUNT_IMAGE_UPDATE_DELEGATE_H_
