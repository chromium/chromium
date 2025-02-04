// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_FEED_SIGN_IN_PROMO_DELEGATE_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_FEED_SIGN_IN_PROMO_DELEGATE_H_

// Where the feed sign-in promo comes from.
typedef NS_ENUM(NSInteger, FeedSignInPromoSource) {
  // The sign-in command was triggered from the bottom of the feed. A last card
  // invite the user to sign-in in order to show them more interesting content.
  FeedSignInCommandSourceBottom,
  // The sign-in command was triggered from a menu entry, such as "Not
  // interested in <topic>".
  // As the user is signed-out, we should first sign the user in to record this
  // lack of interest.
  FeedSignInCommandSourceCardMenu,
};

// Protocol for actions relating to the feed sign-in promo.
@protocol FeedSignInPromoDelegate

// Shows a sign in UI for this specific source.
- (void)showSignInUIFromSource:(FeedSignInPromoSource)source;

@end

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_FEED_SIGN_IN_PROMO_DELEGATE_H_
