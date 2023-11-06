// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_MODEL_TAB_HELPER_UTIL_H_
#define IOS_CHROME_BROWSER_TABS_MODEL_TAB_HELPER_UTIL_H_

namespace web {
class WebState;
}

// Attaches tab helpers to WebState. If `for_prerender` is true, then only
// the tab helpers that must be attached even for pre-rendered WebStates
// are created.
void AttachTabHelpers(web::WebState* web_state, bool for_prerender);

#endif  // IOS_CHROME_BROWSER_TABS_MODEL_TAB_HELPER_UTIL_H_
