// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_SCROLL_TOOL_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_SCROLL_TOOL_H_

#import <memory>

#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/types/expected.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/action_target.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/action_target_java_script_feature.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/web_actor_tool.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"

namespace web {
class WebState;
}  // namespace web

namespace actor {

class ScrollToolJavaScriptFeature;

// Tool that scrolls within an element on a page.
class ScrollTool : public WebActorTool {
 public:
  ~ScrollTool() override;

  static std::unique_ptr<ScrollTool> Create(
      base::WeakPtr<web::WebState> web_state,
      const optimization_guide::proto::ScrollAction& action);

  // ActorTool:
  void Validate(ToolExecutionCallback callback) override;
  void Execute(ToolExecutionCallback callback) override;
  base::WeakPtr<web::WebState> GetTargetWebState() const override;
  ToolType GetToolType() const override;

 private:
  ScrollTool(base::WeakPtr<web::WebState> web_state,
             const optimization_guide::proto::ScrollAction& action,
             ActionTarget target);

  void OnTargetFrameResolved(
      ToolExecutionCallback callback,
      base::expected<ActionTargetJavaScriptFeature::TargetFrameResult,
                     ToolExecutionResult> result);

  optimization_guide::proto::ScrollAction action_;
  ActionTarget target_;
  base::WeakPtr<web::WebState> web_state_;
  raw_ptr<ScrollToolJavaScriptFeature> js_feature_ = nullptr;
  base::WeakPtrFactory<ScrollTool> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_SCROLL_TOOL_H_
