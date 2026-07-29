// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_USER_ACCOUNT_IMAGE_UPDATE_DELEGATE_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_USER_ACCOUNT_IMAGE_UPDATE_DELEGATE_H_

// Delegate protocol to update UI with current user account avatar.
@protocol UserAccountImageUpdateDelegate

// Sets up an avatar for signed-out state.
- (void)setSignedOutAccountImage;
// Updates current signed-in user account avatar with the supplied images.
// `avatarWithoutAITier` is the normal-sized avatar image to be displayed when
// the AI tier ring is not shown.
// `avatarForAITier` is the smaller-sized avatar image to be displayed when
// the AI tier ring is shown, so that the ring fits within the normal bounds.
// `name` and `email` must not be nil.
- (void)updateAccountWithName:(NSString*)name
                        email:(NSString*)email
          avatarWithoutAITier:(UIImage*)avatarWithoutAITier
              avatarForAITier:(UIImage*)avatarForAITier;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_USER_ACCOUNT_IMAGE_UPDATE_DELEGATE_H_
