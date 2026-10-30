// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_TYPE_TOOL_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_TYPE_TOOL_H_

#import <memory>

#import "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/types/expected.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/action_target.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/action_target_java_script_feature.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/web_actor_tool.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"

struct AutoSubmissionState;

namespace autofill {
class FieldRendererId;
}  // namespace autofill

namespace web {
class WebState;
}  // namespace web

namespace actor {

class TypeToolJavaScriptFeature;

// Tool to type text into an element on a page.
class TypeTool : public WebActorTool {
 public:
  ~TypeTool() override;

  static std::unique_ptr<TypeTool> Create(
      base::WeakPtr<web::WebState> web_state,
      const optimization_guide::proto::TypeAction& action);

  // ActorTool:
  void Validate(ToolExecutionCallback callback) override;
  void Execute(ToolExecutionCallback callback) override;
  base::WeakPtr<web::WebState> GetTargetWebState() const override;
  ToolType GetToolType() const override;
  std::string DebugString() const override;

 private:
  TypeTool(base::WeakPtr<web::WebState> web_state,
           const optimization_guide::proto::TypeAction& action,
           ActionTarget target);

  void OnTargetFrameResolved(
      base::expected<ActionTargetJavaScriptFeature::TargetFrameResult,
                     ToolExecutionResult> result);

  // Invoked when the JavaScript typing phase completes. Triggers native
  // keystroke auto-submission if `action_.follow_by_enter()` is true.
  void OnTypeCompleted(ToolExecutionResult result,
                       autofill::FieldRendererId field_id);

  // Invoked when native keystroke auto-submission finishes. Falls back to
  // JavaScript `Enter` + `requestSubmit()` if `auto_submission_state` indicates
  // that no native keystroke was dispatched.
  void OnAutoSubmissionCompleted(ToolExecutionResult result,
                                 autofill::FieldRendererId field_id,
                                 AutoSubmissionState auto_submission_state);

  optimization_guide::proto::TypeAction action_;
  ActionTarget target_;
  ToolExecutionCallback callback_;
  base::WeakPtr<web::WebState> web_state_;
  raw_ptr<TypeToolJavaScriptFeature> js_feature_ = nullptr;
  base::WeakPtrFactory<TypeTool> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_TYPE_TOOL_H_
