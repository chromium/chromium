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
#import "ios/chrome/browser/intelligence/actor/tools/model/action_target_java_script_feature.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/web_actor_tool.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"

class ProfileIOS;

namespace web {
class WebState;
}  // namespace web

namespace actor {

class ScrollToolJavaScriptFeature;

// Tool that scrolls within an element on a page.
class ScrollTool : public WebActorTool {
 public:
  ~ScrollTool() override;

  static base::expected<std::unique_ptr<ScrollTool>, ToolExecutionResult>
  Create(const optimization_guide::proto::ScrollAction& action,
         ProfileIOS* profile);

  // ActorTool:
  void Execute(ToolExecutionCallback callback) override;
  base::WeakPtr<web::WebState> GetTargetWebState() const override;
  optimization_guide::proto::Action::ActionCase GetActionCase() const override;

 private:
  ScrollTool(const optimization_guide::proto::ScrollAction& action,
             base::WeakPtr<web::WebState> web_state);

  void OnTargetFrameResolved(
      optimization_guide::proto::ScrollAction action,
      ToolExecutionCallback callback,
      base::expected<ActionTargetJavaScriptFeature::TargetFrameResult,
                     ToolExecutionResult> result);

  optimization_guide::proto::ScrollAction action_;
  base::WeakPtr<web::WebState> web_state_;
  raw_ptr<ScrollToolJavaScriptFeature> js_feature_ = nullptr;
  base::WeakPtrFactory<ScrollTool> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_SCROLL_TOOL_H_
