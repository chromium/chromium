// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_MODEL_WEB_NAVIGATION_UTIL_H_
#define IOS_CHROME_BROWSER_WEB_MODEL_WEB_NAVIGATION_UTIL_H_

#import <Foundation/Foundation.h>

#include "components/search_engines/template_url.h"
#import "ios/web/public/navigation/navigation_manager.h"
#include "ui/base/page_transition_types.h"

namespace web {
class WebState;
}

class GURL;

namespace web_navigation_util {

// Creates a WebLoadParams object for loading `url` with `transition_type`. If
// `post_data` is non-null, the data and content-type of the post data will be
// added to the return value as well.
web::NavigationManager::WebLoadParams CreateWebLoadParams(
    const GURL& url,
    ui::PageTransition transition_type,
    TemplateURLRef::PostContent* post_data);

// Creates ChromeVariations headers for the given `url` and `is_incognito`
// state. The returned dictionary will be empty if the url is a non-Google
// url.
NSDictionary<NSString*, NSString*>* VariationHeadersForURL(const GURL& url,
                                                           bool is_incognito);

// Navigates to the previous item on the navigation stack for `web_state`.
// `web_state` can't be null. This method is for user initiated navigations as
// it logs "Back" user action.
void GoBack(web::WebState* web_state);

// Navigates to the next item on the navigation stack for `web_state`.
// `web_state` can't be null. This method is for user initiated navigations as
// it logs "Forward" user action.
void GoForward(web::WebState* web_state);

}  // namespace web_navigation_util

#endif  // IOS_CHROME_BROWSER_WEB_MODEL_WEB_NAVIGATION_UTIL_H_
