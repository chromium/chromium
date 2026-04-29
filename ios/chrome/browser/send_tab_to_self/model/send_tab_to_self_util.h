// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEND_TAB_TO_SELF_MODEL_SEND_TAB_TO_SELF_UTIL_H_
#define IOS_CHROME_BROWSER_SEND_TAB_TO_SELF_MODEL_SEND_TAB_TO_SELF_UTIL_H_

#include "base/supports_user_data.h"
#include "components/send_tab_to_self/page_context.h"
#include "url/origin.h"

@class OpenNewTabCommand;

namespace web {
class WebState;
}

namespace send_tab_to_self {

class SendTabToSelfEntry;

// Creates an OpenNewTabCommand for the given `entry`.
OpenNewTabCommand* CreateOpenNewTabCommand(const SendTabToSelfEntry* entry);

// Creates a PageContext for the given `web_state` by extracting form data
// from all frames.
PageContext ExtractFormFieldsFromWebState(web::WebState* web_state);

// Fills the forms in the given `web_state` with the data from `page_context` if
// its origin matches `origin`.
void FillWebState(web::WebState* web_state,
                  const url::Origin& origin,
                  const PageContext& page_context);

}  // namespace send_tab_to_self

#endif  // IOS_CHROME_BROWSER_SEND_TAB_TO_SELF_MODEL_SEND_TAB_TO_SELF_UTIL_H_
