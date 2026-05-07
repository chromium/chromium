// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_NAVIGATE_TOOL_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_NAVIGATE_TOOL_H_

#import <string>

#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool.h"

class ProfileIOS;
class UrlLoadingBrowserAgent;
class WebStateList;

namespace optimization_guide {
namespace proto {
class NavigateAction;
}  // namespace proto
}  // namespace optimization_guide

namespace web {
class WebState;
}  // namespace web

namespace actor {

struct ToolExecutionResult;

// Command to navigate to a URL.
class NavigateTool : public ActorTool {
 public:
  ~NavigateTool() override;

  static base::expected<std::unique_ptr<NavigateTool>, ToolExecutionResult>
  Create(const optimization_guide::proto::NavigateAction& action,
         ProfileIOS* profile);

  // ActorTool:
  void Execute(ToolExecutionCallback callback) override;
  base::WeakPtr<web::WebState> GetTargetWebState() const override;
  optimization_guide::proto::Action::ActionCase GetActionCase() const override;

 private:
  NavigateTool(const std::string& url,
               base::WeakPtr<web::WebState> web_state,
               WebStateList* web_state_list,
               UrlLoadingBrowserAgent* url_loader);

  const std::string url_;
  base::WeakPtr<web::WebState> web_state_;
  raw_ptr<WebStateList> web_state_list_;
  raw_ptr<UrlLoadingBrowserAgent> url_loader_;
};

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_NAVIGATE_TOOL_H_
