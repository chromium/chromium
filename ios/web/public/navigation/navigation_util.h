// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_NAVIGATION_NAVIGATION_UTIL_H_
#define IOS_WEB_PUBLIC_NAVIGATION_NAVIGATION_UTIL_H_

#include <string>

#include "base/time/time.h"
#include "ios/web/common/user_agent.h"
#include "ios/web/public/navigation/navigation_manager.h"

namespace web {
namespace proto {
class WebStateStorage;
}  // namespace proto

// Helper function returning a WebStateStorage that can be used to create
// an unrealized WebState that would behave as if `LoadURLWithParams()`
// with `params` was called on it after becoming realised.
proto::WebStateStorage CreateWebStateStorage(
    const NavigationManager::WebLoadParams& params,
    const std::u16string& title,
    bool created_with_opener,
    UserAgentType user_agent,
    base::Time creation_time);

}  // namespace web

#endif  // IOS_WEB_PUBLIC_NAVIGATION_NAVIGATION_UTIL_H_
