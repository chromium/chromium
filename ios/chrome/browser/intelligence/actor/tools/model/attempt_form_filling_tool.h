// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_ATTEMPT_FORM_FILLING_TOOL_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_ATTEMPT_FORM_FILLING_TOOL_H_

#import <memory>

#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/types/expected.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool.h"

namespace web {
class WebState;
}  // namespace web

namespace actor {

class ToolDelegate;
struct ToolExecutionResult;

// Tool to attempt form filling on iOS.
class AttemptFormFillingTool : public ActorTool {
 public:
  static base::expected<std::unique_ptr<AttemptFormFillingTool>,
                        ToolExecutionResult>
  Create(base::WeakPtr<web::WebState> web_state,
         const optimization_guide::proto::AttemptFormFillingAction& action,
         ToolDelegate* tool_delegate);

  ~AttemptFormFillingTool() override;

  // ActorTool:
  void Execute(ToolExecutionCallback callback) override;
  base::WeakPtr<web::WebState> GetTargetWebState() const override;
  ToolType GetToolType() const override;
  void Validate(ToolExecutionCallback callback) override;

 private:
  AttemptFormFillingTool(
      base::WeakPtr<web::WebState> web_state,
      const optimization_guide::proto::AttemptFormFillingAction& action,
      ToolDelegate* tool_delegate);

  optimization_guide::proto::AttemptFormFillingAction action_;
  base::WeakPtr<web::WebState> web_state_;
  raw_ptr<ToolDelegate> tool_delegate_;
};

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_ATTEMPT_FORM_FILLING_TOOL_H_
