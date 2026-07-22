// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_NAVIGATE_TOOL_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_NAVIGATE_TOOL_H_

#import <optional>
#import <string>

#import "base/functional/callback.h"
#import "base/memory/weak_ptr.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool.h"

class UrlLoadingBrowserAgent;
struct UrlLoadParams;

namespace web {
class WebState;
}  // namespace web

namespace actor {

// Command to navigate to a URL.
class NavigateTool : public ActorTool {
 public:
  static std::unique_ptr<NavigateTool> Create(
      base::WeakPtr<web::WebState> web_state,
      const optimization_guide::proto::NavigateAction& action,
      base::WeakPtr<UrlLoadingBrowserAgent> url_loader);

  ~NavigateTool() override;

  // ActorTool:
  void Validate(ToolExecutionCallback callback) override;
  void Execute(ToolExecutionCallback callback) override;
  base::WeakPtr<web::WebState> GetTargetWebState() const override;
  ToolType GetToolType() const override;

 private:
  NavigateTool(base::WeakPtr<web::WebState> web_state,
               std::optional<std::string> url,
               base::WeakPtr<UrlLoadingBrowserAgent> url_loader);

  std::optional<std::string> url_;
  base::WeakPtr<web::WebState> web_state_;
  base::WeakPtr<UrlLoadingBrowserAgent> url_loader_;
};

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_NAVIGATE_TOOL_H_
