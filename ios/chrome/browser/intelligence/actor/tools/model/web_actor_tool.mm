// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/web_actor_tool.h"

#import "base/memory/weak_ptr.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/action_target_java_script_feature.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_error.h"
#import "ios/web/public/web_state.h"

namespace actor {

void WebActorTool::ResolveTargetFrame(
    base::WeakPtr<web::WebState> web_state,
    base::WeakPtr<web::WebFrame> web_frame,
    const optimization_guide::proto::ActionTarget& target,
    ActionTargetJavaScriptFeature::TargetFrameCallback callback) {
  if (!web_state || !web_frame) {
    std::move(callback).Run(base::unexpected(
        ActorToolError{ActorToolErrorCode::kExecutionMissingDependencies}));
    return;
  }

  ActionTargetJavaScriptFeature::GetInstance()->GetTargetFrame(
      web_state.get(), web_frame.get(), target, std::move(callback));
}

}  // namespace actor
