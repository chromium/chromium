// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actuation/model/tools/web_actuation_tool.h"

#import "base/memory/weak_ptr.h"
#import "ios/chrome/browser/intelligence/actuation/model/actuation_error.h"
#import "ios/chrome/browser/intelligence/actuation/model/tools/actuation_target_java_script_feature.h"
#import "ios/web/public/web_state.h"

void WebActuationTool::ResolveTargetFrame(
    base::WeakPtr<web::WebState> web_state,
    base::WeakPtr<web::WebFrame> web_frame,
    const optimization_guide::proto::ActionTarget& target,
    ActuationTargetJavaScriptFeature::TargetFrameCallback callback) {
  if (!web_state || !web_frame) {
    std::move(callback).Run(base::unexpected(
        ActuationError{ActuationErrorCode::kExecutionMissingDependencies}));
    return;
  }

  ActuationTargetJavaScriptFeature::GetInstance()->GetTargetFrame(
      web_state.get(), web_frame.get(), target, std::move(callback));
}
