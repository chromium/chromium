// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_FEED_SIGN_IN_PROMO_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_NTP_FEED_SIGN_IN_PROMO_DELEGATE_H_

// Protocol for actions relating to the feed sign-in promo.
@protocol FeedSignInPromoDelegate

// Shows a sign in promote UI.
- (void)showSignInPromoUI;

// Shows a sign in flow.
- (void)showSignInUI;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_FEED_SIGN_IN_PROMO_DELEGATE_H_
