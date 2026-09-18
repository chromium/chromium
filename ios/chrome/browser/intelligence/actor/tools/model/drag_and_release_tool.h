// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_DRAG_AND_RELEASE_TOOL_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_DRAG_AND_RELEASE_TOOL_H_

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
class WebFrame;
class WebState;
}  // namespace web

namespace actor {

class DragAndReleaseToolJavaScriptFeature;

// A tool that performs a drag and release action from one target to another.
class DragAndReleaseTool : public WebActorTool {
 public:
  DragAndReleaseTool(const DragAndReleaseTool&) = delete;
  DragAndReleaseTool& operator=(const DragAndReleaseTool&) = delete;

  ~DragAndReleaseTool() override;

  // Creates a DragAndReleaseTool instance.
  static std::unique_ptr<DragAndReleaseTool> Create(
      base::WeakPtr<web::WebState> web_state,
      const optimization_guide::proto::DragAndReleaseAction& action);

  // ActorTool:
  void Validate(ToolExecutionCallback callback) override;
  void Execute(ToolExecutionCallback callback) override;
  base::WeakPtr<web::WebState> GetTargetWebState() const override;
  ToolType GetToolType() const override;

 private:
  DragAndReleaseTool(base::WeakPtr<web::WebState> web_state,
                     ActionTarget from_target,
                     ActionTarget to_target);

  // Callback invoked when the source target's WebFrame is resolved.
  void OnFromTargetFrameResolved(
      ToolExecutionCallback callback,
      base::expected<ActionTargetJavaScriptFeature::TargetFrameResult,
                     ToolExecutionResult> result);

  // Callback invoked when the destination target's WebFrame is resolved.
  void OnToTargetFrameResolved(
      ToolExecutionCallback callback,
      base::WeakPtr<web::WebFrame> from_frame,
      ActionTarget from_target,
      base::expected<ActionTargetJavaScriptFeature::TargetFrameResult,
                     ToolExecutionResult> result);

  ActionTarget from_target_;
  ActionTarget to_target_;
  base::WeakPtr<web::WebState> web_state_ = nullptr;
  raw_ptr<DragAndReleaseToolJavaScriptFeature> js_feature_ = nullptr;
  base::WeakPtrFactory<DragAndReleaseTool> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_DRAG_AND_RELEASE_TOOL_H_
