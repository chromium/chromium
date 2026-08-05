// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_CLICK_TOOL_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_CLICK_TOOL_H_

#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/types/expected.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/action_target.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/action_target_java_script_feature.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/web_actor_tool.h"

namespace web {
class WebState;
}  // namespace web

namespace actor {

class ClickToolJavaScriptFeature;

// Tool to click an element on a page.
class ClickTool : public WebActorTool {
 public:
  ~ClickTool() override;

  static std::unique_ptr<ClickTool> Create(
      base::WeakPtr<web::WebState> web_state,
      const optimization_guide::proto::ClickAction& action);

  // ActorTool:
  void Validate(ToolExecutionCallback callback) override;
  void Execute(ToolExecutionCallback callback) override;
  base::WeakPtr<web::WebState> GetTargetWebState() const override;
  ToolType GetToolType() const override;

 private:
  void OnTargetFrameResolved(
      ToolExecutionCallback callback,
      base::expected<ActionTargetJavaScriptFeature::TargetFrameResult,
                     ToolExecutionResult> result);

  ClickTool(base::WeakPtr<web::WebState> web_state,
            const optimization_guide::proto::ClickAction& action,
            ActionTarget target);

  optimization_guide::proto::ClickAction action_;
  ActionTarget target_;
  base::WeakPtr<web::WebState> web_state_ = nullptr;
  raw_ptr<ClickToolJavaScriptFeature> js_feature_ = nullptr;
  base::WeakPtrFactory<ClickTool> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_CLICK_TOOL_H_
