// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_USER_ACCOUNT_IMAGE_UPDATE_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_USER_ACCOUNT_IMAGE_UPDATE_DELEGATE_H_

// Delegate protocol to update UI with current user account avatar.
@protocol UserAccountImageUpdateDelegate

// Updates current user account avatar with supplied image.
- (void)updateAccountImage:(UIImage*)image;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_USER_ACCOUNT_IMAGE_UPDATE_DELEGATE_H_
