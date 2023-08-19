// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_WEB_STATE_TEST_UTIL_H_
#define IOS_WEB_PUBLIC_TEST_WEB_STATE_TEST_UTIL_H_

#include <string>
#include <vector>

#import "ios/web/public/web_state.h"
#include "url/gurl.h"

@class CRWWebController;

namespace web {
namespace test {

// Structure representing a page.
struct PageInfo {
  GURL url;
  std::string title;
};

// Synchronously executes JavaScript and returns result as id.
id ExecuteJavaScript(NSString* script, web::WebState* web_state);

// Returns CRWWebController for the given `web_state`.
CRWWebController* GetWebController(web::WebState* web_state);

// Loads the specified HTML content with URL into the WebState.
void LoadHtml(NSString* html, const GURL& url, web::WebState* web_state);

// Loads the specified HTML content into the WebState, using test url name.
void LoadHtml(NSString* html, web::WebState* web_state);

// Loads the specified HTML content with URL into the WebState. None of the
// subresources will be fetched.
bool LoadHtmlWithoutSubresources(NSString* html, web::WebState* web_state);

// Creates an unrealized WebState with a navigation session containing the
// items described by `items`.
std::unique_ptr<WebState> CreateUnrealizedWebStateWithItems(
    BrowserState* browser_state,
    size_t last_committed_item_index,
    const std::vector<PageInfo>& items);

}  // namespace test
}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_WEB_STATE_TEST_UTIL_H_
