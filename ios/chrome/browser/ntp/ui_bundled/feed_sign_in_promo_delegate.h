// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_FEED_SIGN_IN_PROMO_DELEGATE_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_FEED_SIGN_IN_PROMO_DELEGATE_H_

// Protocol for actions relating to the feed sign-in promo.
@protocol FeedSignInPromoDelegate

// Shows a sign in promote UI for feed back of card sign-in promo.
// TODO(crbug.com/40245722): rename it as "showHalfSheetForFeedBoCSignInPromo"
// since it's not a promo UI but a message to let the user to continue to sign
// in.
- (void)showSignInPromoUI;

// Shows a sign in UI for feed bottom sign-in promo.
// TODO(crbug.com/40245722): rename it as "showSyncPromoUI" since it shows a
// sync flow.
- (void)showSignInUI;

@end

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_FEED_SIGN_IN_PROMO_DELEGATE_H_
