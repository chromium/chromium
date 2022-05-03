// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FOLLOW_FOLLOW_UTIL_H_
#define IOS_CHROME_BROWSER_FOLLOW_FOLLOW_UTIL_H_

#import "ios/chrome/browser/follow/follow_action_state.h"

namespace web {
class WebState;
}

// Returns the Follow action state for |webState|.
FollowActionState GetFollowActionState(web::WebState* webState);

#endif  // IOS_CHROME_BROWSER_FOLLOW_FOLLOW_UTIL_H_
