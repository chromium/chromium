// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UTIL_ACTOR_TEST_UTILS_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UTIL_ACTOR_TEST_UTILS_H_

#import <memory>

#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_request.h"
#import "ios/web/public/web_state_id.h"

namespace actor {

// Creates a successful tool request. Selects the `Wait` action arbitrarily
// as a representative successful action, optionally targeting `identifier`.
std::unique_ptr<ActorToolRequest> MakeSuccessfulActorToolRequest(
    web::WebStateID identifier = web::WebStateID());

// Creates a tool request that fails execution. Selects the `Click` action
// arbitrarily because it fails execution when no target tab is registered.
std::unique_ptr<ActorToolRequest> MakeFailingActorToolRequest();

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UTIL_ACTOR_TEST_UTILS_H_
