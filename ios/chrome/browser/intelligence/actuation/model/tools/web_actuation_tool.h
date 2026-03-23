// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTUATION_MODEL_TOOLS_WEB_ACTUATION_TOOL_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTUATION_MODEL_TOOLS_WEB_ACTUATION_TOOL_H_

#import "base/memory/weak_ptr.h"
#import "base/types/expected.h"
#import "ios/chrome/browser/intelligence/actuation/model/actuation_error.h"
#import "ios/chrome/browser/intelligence/actuation/model/tools/actuation_target_java_script_feature.h"
#import "ios/chrome/browser/intelligence/actuation/model/tools/actuation_tool.h"

namespace optimization_guide {
namespace proto {
class ActionTarget;
}
}  // namespace optimization_guide

namespace web {
class WebState;
class WebFrame;
}  // namespace web

// Base class for actuation tools that interact with web content.
//
// Handles common logic for tab resolution, validation, and processing JS
// results.
class WebActuationTool : public ActuationTool {
 protected:
  // Finds the target WebFrame for the `target` and returns it via `callback`.
  void ResolveTargetFrame(
      base::WeakPtr<web::WebState> web_state,
      base::WeakPtr<web::WebFrame> web_frame,
      const optimization_guide::proto::ActionTarget& target,
      ActuationTargetJavaScriptFeature::TargetFrameCallback callback);
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTUATION_MODEL_TOOLS_WEB_ACTUATION_TOOL_H_
