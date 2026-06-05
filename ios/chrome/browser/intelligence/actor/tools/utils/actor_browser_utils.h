// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_UTILS_ACTOR_BROWSER_UTILS_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_UTILS_ACTOR_BROWSER_UTILS_H_

#import "ios/chrome/browser/shared/model/web_state_list/browser_util.h"
#import "ios/web/public/web_state_id.h"

class ProfileIOS;

namespace actor {

// Finds the Browser and index of the WebState associated with the given
// `target_id` inside the given `profile`. Optionally includes incognito
// browsers in the search.
BrowserAndIndex FindBrowserAndIndexFromProfile(ProfileIOS* profile,
                                               web::WebStateID target_id,
                                               bool include_incognito);

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_UTILS_ACTOR_BROWSER_UTILS_H_
