// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/web_actor_tool.h"

#import "base/memory/weak_ptr.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/action_target.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/action_target_java_script_feature.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/metrics.h"
#import "ios/web/public/web_state.h"

namespace actor {

WebActorTool::WebActorTool() = default;
WebActorTool::~WebActorTool() = default;

base::WeakPtr<web::WebFrame> WebActorTool::GetTargetWebFrame() const {
  return target_frame_;
}

void WebActorTool::ResolveTargetFrame(
    base::WeakPtr<web::WebState> web_state,
    base::WeakPtr<web::WebFrame> web_frame,
    const ActionTarget& target,
    ActionTargetJavaScriptFeature::TargetFrameCallback callback) {
  if (!web_state || !web_frame) {
    if (target.node_id().has_value()) {
      RecordActionTargetType(ActionTargetType::kDomNodeId);
    } else if (target.coordinate().has_value()) {
      RecordActionTargetType(ActionTargetType::kCoordinate);
    }
    mojom::ActionResultCode error_code =
        web_state ? mojom::ActionResultCode::kFrameWentAway
                  : mojom::ActionResultCode::kTabWentAway;
    RecordTargetFrameResolutionResult(error_code);
    std::move(callback).Run(base::unexpected(ToolExecutionResult(error_code)));
    return;
  }

  ActionTargetJavaScriptFeature::GetInstance()->GetTargetFrame(
      web_state.get(), web_frame.get(), target, std::move(callback));
}

}  // namespace actor
