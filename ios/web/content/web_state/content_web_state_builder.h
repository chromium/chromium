// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_CONTENT_WEB_STATE_CONTENT_WEB_STATE_BUILDER_H_
#define IOS_WEB_CONTENT_WEB_STATE_CONTENT_WEB_STATE_BUILDER_H_

@class CRWSessionStorage;
namespace content {
class NavigationController;
}
namespace web {
class BrowserState;
class ContentNavigationManager;
class ContentWebState;
}  // namespace web

namespace web {

// Populates `web_state` and its `controller` with `session_storage`'s
// session information.
void ExtractContentSessionStorage(ContentWebState* web_state,
                                  content::NavigationController& controller,
                                  web::BrowserState* browser_state,
                                  CRWSessionStorage* session_storage);

// Creates a serializable session storage from `web_state` and
// `navigation_manager`.
CRWSessionStorage* BuildContentSessionStorage(
    const ContentWebState* web_state,
    ContentNavigationManager* navigation_manager);

}  // namespace web

#endif  // IOS_WEB_CONTENT_WEB_STATE_CONTENT_WEB_STATE_BUILDER_H_
