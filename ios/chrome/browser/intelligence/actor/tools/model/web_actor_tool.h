// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_WEB_ACTOR_TOOL_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_WEB_ACTOR_TOOL_H_

#import "base/memory/weak_ptr.h"
#import "base/types/expected.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/action_target_java_script_feature.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_error.h"

namespace optimization_guide {
namespace proto {
class ActionTarget;
}  // namespace proto
}  // namespace optimization_guide

namespace web {
class WebState;
class WebFrame;
}  // namespace web

namespace actor {

// Base class for actor tools that interact with web content.
//
// Handles common logic for tab resolution, validation, and processing JS
// results.
class WebActorTool : public ActorTool {
 protected:
  // Finds the target WebFrame for the `target` and returns it via `callback`.
  void ResolveTargetFrame(
      base::WeakPtr<web::WebState> web_state,
      base::WeakPtr<web::WebFrame> web_frame,
      const optimization_guide::proto::ActionTarget& target,
      ActionTargetJavaScriptFeature::TargetFrameCallback callback);
};

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_WEB_ACTOR_TOOL_H_
