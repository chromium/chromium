// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FOLLOW_FOLLOW_UTIL_H_
#define IOS_CHROME_BROWSER_FOLLOW_FOLLOW_UTIL_H_

#import "ios/chrome/browser/follow/follow_action_state.h"

namespace web {
class WebState;
}

// Key used to store the last shown time of follow in-product help (IPH).
extern NSString* const kFollowIPHLastShownTime;

// Returns the Follow action state for |webState|.
FollowActionState GetFollowActionState(web::WebState* webState);

#pragma mark - For Follow IPH
// Returns true if the time between the last time a Follow IPH was shown and now
// is long enough for another Follow IPH appearance.
bool IsFollowIPHShownFrequencyEligible(NSURL* RSSLink);
// Stores the Follow IPH presenting time for website with `RSSLink`.
void StoreFollowIPHPresentingTime(NSURL* RSSLink);

#endif  // IOS_CHROME_BROWSER_FOLLOW_FOLLOW_UTIL_H_
