// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_CHANGE_PROFILE_CHANGE_PROFILE_AUTHENTICATION_CONTINUATION_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_CHANGE_PROFILE_CHANGE_PROFILE_AUTHENTICATION_CONTINUATION_H_

#import "ios/chrome/app/change_profile_continuation.h"
#import "ios/chrome/browser/shared/coordinator/scene/url_context.h"

// Returns a ChangeProfileContinuation that starts the sign-in or sign-out flow.
ChangeProfileContinuation CreateChangeProfileAuthenticationContinuation(
    URLContext* context,
    NSSet<UIOpenURLContext*>* contexts,
    BOOL openURL);

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_CHANGE_PROFILE_CHANGE_PROFILE_AUTHENTICATION_CONTINUATION_H_
