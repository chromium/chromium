// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_MODEL_TAB_HELPER_UTIL_H_
#define IOS_CHROME_BROWSER_TABS_MODEL_TAB_HELPER_UTIL_H_

#import "ios/chrome/browser/tabs/model/tab_helper_filter.h"

namespace web {
class WebState;
}

// Attaches tab helpers to WebState. Filter the attached tab helpers with
// `filter_flags`. This function is idempotent, so it is okay to call it
// multiple times for the same WebState. When called with a different
// `filter_flags` value, the right thing (adding helpers that weren't added in
// the prior call) will happen, although tab helpers will never be removed.
void AttachTabHelpers(web::WebState* web_state,
                      TabHelperFilter filter_flags = TabHelperFilter::kEmpty);

#endif  // IOS_CHROME_BROWSER_TABS_MODEL_TAB_HELPER_UTIL_H_
