// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEND_TAB_TO_SELF_MODEL_SEND_TAB_TO_SELF_UTIL_H_
#define IOS_CHROME_BROWSER_SEND_TAB_TO_SELF_MODEL_SEND_TAB_TO_SELF_UTIL_H_

#include "components/send_tab_to_self/page_context.h"

namespace web {
class WebState;
}

namespace send_tab_to_self {

// Creates a PageContext for the given `web_state` by extracting form data
// from all frames.
PageContext ExtractFormFieldsFromWebState(web::WebState* web_state);

}  // namespace send_tab_to_self

#endif  // IOS_CHROME_BROWSER_SEND_TAB_TO_SELF_MODEL_SEND_TAB_TO_SELF_UTIL_H_
