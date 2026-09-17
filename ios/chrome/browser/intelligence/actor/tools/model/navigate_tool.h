// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_NAVIGATE_TOOL_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_NAVIGATE_TOOL_H_

#import <optional>
#import <string>

#import "base/feature_list.h"
#import "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool.h"

class GURL;
class UrlLoadingBrowserAgent;
struct UrlLoadParams;

namespace web {
class WebState;
}  // namespace web

namespace origin_gating {
class GatingDecisionContext;
class OriginGatingChecker;
struct GatingDecision;
}  // namespace origin_gating

namespace actor {

// Command to navigate to a URL.
class NavigateTool : public ActorTool {
 public:
  static std::unique_ptr<NavigateTool> Create(
      base::WeakPtr<web::WebState> web_state,
      const optimization_guide::proto::NavigateAction& action,
      base::WeakPtr<UrlLoadingBrowserAgent> url_loader,
      origin_gating::OriginGatingChecker* gating_checker);

  ~NavigateTool() override;

  // ActorTool:
  void Validate(ToolExecutionCallback callback) override;
  void Execute(ToolExecutionCallback callback) override;
  base::WeakPtr<web::WebState> GetTargetWebState() const override;
  ToolType GetToolType() const override;

 private:
  NavigateTool(base::WeakPtr<web::WebState> web_state,
               std::optional<std::string> url,
               base::WeakPtr<UrlLoadingBrowserAgent> url_loader,
               origin_gating::OriginGatingChecker* gating_checker);

  void OnGatingDecisionComputed(
      const GURL& destination_url,
      ToolExecutionCallback callback,
      std::unique_ptr<origin_gating::GatingDecisionContext> context,
      origin_gating::GatingDecision decision);
  void LoadUrl(const GURL& destination_url, ToolExecutionCallback callback);

  std::optional<std::string> url_;
  base::WeakPtr<web::WebState> web_state_;
  base::WeakPtr<UrlLoadingBrowserAgent> url_loader_;
  raw_ptr<origin_gating::OriginGatingChecker> gating_checker_ = nullptr;

  base::WeakPtrFactory<NavigateTool> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_NAVIGATE_TOOL_H_
