// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_CONTENT_WEB_STATE_CONTENT_WEB_STATE_BUILDER_H_
#define IOS_WEB_CONTENT_WEB_STATE_CONTENT_WEB_STATE_BUILDER_H_

namespace content {
class NavigationController;
}
namespace web {
class BrowserState;
class ContentNavigationManager;
class ContentWebState;
namespace proto {
class WebStateStorage;
}  // namespace proto

// Populates `web_state` and its `controller` from `storage` information.
void ExtractContentSessionStorage(ContentWebState* web_state,
                                  content::NavigationController& controller,
                                  BrowserState* browser_state,
                                  proto::WebStateStorage storage);

// Serializes the state from `web_state` and `navigation_manager` to `storage`.
void SerializeContentStorage(const ContentWebState* web_state,
                             const ContentNavigationManager* navigation_manager,
                             proto::WebStateStorage& storage);

}  // namespace web

#endif  // IOS_WEB_CONTENT_WEB_STATE_CONTENT_WEB_STATE_BUILDER_H_
