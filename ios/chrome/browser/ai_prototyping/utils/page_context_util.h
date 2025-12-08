// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_UTILS_PAGE_CONTEXT_UTIL_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_UTILS_PAGE_CONTEXT_UTIL_H_

#import "base/functional/callback.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper.h"

namespace base {
class TimeDelta;
}
@class PageContextWrapper;
namespace web {
class WebState;
}

PageContextWrapper* CreatePageContextWrapper(
    web::WebState* web_state,
    base::OnceCallback<void(PageContextWrapperCallbackResponse)>
        completion_callback);

// Populates the page context from the `wrapper` for the given `web_state`.
// If the page is still loading, it waits for it to finish before populating.
void PopulatePageContext(PageContextWrapper* wrapper, web::WebState* web_state);

// Same as `PopulatePageContext` but with a `timeout`.
void PopulatePageContextWithTimeout(PageContextWrapper* wrapper,
                                    web::WebState* web_state,
                                    base::TimeDelta timeout);

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_UTILS_PAGE_CONTEXT_UTIL_H_
