// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_NAVIGATE_TOOL_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_NAVIGATE_TOOL_H_

#import <optional>
#import <string>

#import "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/scoped_observation.h"
#import "base/timer/timer.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool.h"
#import "ios/web/public/web_state_observer.h"

class UrlLoadingBrowserAgent;

namespace web {
class NavigationContext;
class WebState;
}  // namespace web

namespace actor {

class ToolDelegate;

// Command to navigate to a URL.
class NavigateTool : public ActorTool, public web::WebStateObserver {
 public:
  static std::unique_ptr<NavigateTool> Create(
      base::WeakPtr<web::WebState> web_state,
      const optimization_guide::proto::NavigateAction& action,
      base::WeakPtr<UrlLoadingBrowserAgent> url_loader,
      ToolDelegate* tool_delegate);

  ~NavigateTool() override;

  // ActorTool:
  void Validate(ToolExecutionCallback callback) override;
  void Execute(ToolExecutionCallback callback) override;
  base::WeakPtr<web::WebState> GetTargetWebState() const override;
  ToolType GetToolType() const override;
  std::string DebugString() const override;

  void Cancel() override;

  // web::WebStateObserver:
  void DidStartNavigation(web::WebState* web_state,
                          web::NavigationContext* navigation_context) override;
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void WebStateDestroyed(web::WebState* web_state) override;

 private:
  NavigateTool(base::WeakPtr<web::WebState> web_state,
               std::optional<std::string> url,
               base::WeakPtr<UrlLoadingBrowserAgent> url_loader,
               ToolDelegate* tool_delegate);

  void OnNavigationTimeout();

  std::optional<std::string> url_;
  base::WeakPtr<web::WebState> web_state_;
  base::WeakPtr<UrlLoadingBrowserAgent> url_loader_;
  raw_ptr<ToolDelegate> tool_delegate_ = nullptr;

  ToolExecutionCallback invoke_callback_;
  std::optional<int64_t> pending_navigation_id_;
  base::OneShotTimer navigation_timeout_timer_;
  base::ScopedObservation<web::WebState, web::WebStateObserver>
      web_state_observation_{this};

  base::WeakPtrFactory<NavigateTool> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_NAVIGATE_TOOL_H_
