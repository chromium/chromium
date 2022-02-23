// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FOLLOW_FOLLOW_UTIL_H_
#define IOS_CHROME_BROWSER_UI_FOLLOW_FOLLOW_UTIL_H_

#import "ios/chrome/browser/ui/follow/follow_action_state.h"

namespace web {
class WebState;
}

class ChromeBrowserState;

// Returns the Follow action state for |webState| and |browserState|.
FollowActionState GetFollowActionState(web::WebState* webState,
                                       ChromeBrowserState* browserState);

#endif  // IOS_CHROME_BROWSER_UI_FOLLOW_FOLLOW_UTIL_H_
