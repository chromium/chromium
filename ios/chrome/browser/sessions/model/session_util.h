// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_MODEL_SESSION_UTIL_H_
#define IOS_CHROME_BROWSER_SESSIONS_MODEL_SESSION_UTIL_H_

#import <memory>
#import <string>
#import <vector>

#import "base/functional/callback.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

class Browser;

namespace sessions {
class SerializedNavigationEntry;
}

namespace web {
class WebState;
}

// Utility method that allows to access the iOS SessionService from C++ code.
namespace session_util {

// Creates a WebState initialized with `profile` and serialized
// navigation. The returned WebState has web usage enabled.
std::unique_ptr<web::WebState> CreateWebStateWithNavigationEntries(
    ProfileIOS* profile,
    int last_committed_item_index,
    const std::vector<sessions::SerializedNavigationEntry>& navigations);

// Returns the recommended session identifier for `browser`.
std::string GetSessionIdentifier(Browser* browser);

// Returns the recommended session identifier that would have been used for
// a possibly `inactive_browser` Browser attached to a SceneState with the
// given `scene_session_identifier`.
std::string GetSessionIdentifier(const std::string& scene_session_identifier,
                                 bool inactive_browser);

}  // namespace session_util

#endif  // IOS_CHROME_BROWSER_SESSIONS_MODEL_SESSION_UTIL_H_
