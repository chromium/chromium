// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_NAVIGATE_TOOL_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_NAVIGATE_TOOL_H_

#import <string>

#import "base/functional/callback.h"
#import "base/memory/weak_ptr.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool.h"

class ProfileIOS;
class UrlLoadingBrowserAgent;
struct UrlLoadParams;

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
  static base::expected<std::unique_ptr<NavigateTool>, ToolExecutionResult>
  Create(const optimization_guide::proto::NavigateAction& action,
         ProfileIOS* profile);

  ~NavigateTool() override;

  // ActorTool:
  void Execute(ToolExecutionCallback callback) override;
  base::WeakPtr<web::WebState> GetTargetWebState() const override;
  ToolType GetToolType() const override;

 private:
  NavigateTool(const std::string& url,
               base::WeakPtr<web::WebState> web_state,
               base::WeakPtr<UrlLoadingBrowserAgent> url_loader);

  const std::string url_;
  base::WeakPtr<web::WebState> web_state_;
  base::WeakPtr<UrlLoadingBrowserAgent> url_loader_;
};

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_NAVIGATE_TOOL_H_
