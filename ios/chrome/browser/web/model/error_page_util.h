// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_MODEL_ERROR_PAGE_UTIL_H_
#define IOS_CHROME_BROWSER_WEB_MODEL_ERROR_PAGE_UTIL_H_

class GURL;
@class NSError;
@class NSString;

namespace web {
class WebState;
}  // namespace web

// Returns whether `prefs::kAllowDinosaurEasterEgg` allows the dinosaur easter
// egg on error pages for `web_state`'s profile. `web_state` must be non-null
// and have a profile.
bool IsDinoEasterEggEnabled(web::WebState* web_state);

// Returns error page HTML to display in WebState if the page load has failed.
NSString* GetErrorPage(const GURL& url,
                       NSError* error,
                       bool is_post,
                       bool is_off_the_record,
                       bool is_dino_easter_egg_enabled);

#endif  // IOS_CHROME_BROWSER_WEB_MODEL_ERROR_PAGE_UTIL_H_
