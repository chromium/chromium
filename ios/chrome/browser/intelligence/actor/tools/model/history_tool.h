// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_HISTORY_TOOL_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_HISTORY_TOOL_H_

#import "base/memory/weak_ptr.h"
#import "base/scoped_observation.h"
#import "base/timer/timer.h"
#import "base/types/expected.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool.h"
#import "ios/web/public/web_state_observer.h"

namespace optimization_guide::proto {
class HistoryBackAction;
class HistoryForwardAction;
}  // namespace optimization_guide::proto

namespace web {
class NavigationContext;
class WebState;
}  // namespace web

namespace actor {

// Tool to navigate back or forward in a tab's history.
class HistoryTool : public ActorTool, public web::WebStateObserver {
 public:
  ~HistoryTool() override;

  // Create the tool to handle "go back" action.
  static std::unique_ptr<HistoryTool> Create(
      base::WeakPtr<web::WebState> web_state,
      const optimization_guide::proto::HistoryBackAction& action);

  // Create the tool to handle "go forward" action.
  static std::unique_ptr<HistoryTool> Create(
      base::WeakPtr<web::WebState> web_state,
      const optimization_guide::proto::HistoryForwardAction& action);

  // ActorTool:
  void Validate(ToolExecutionCallback callback) override;
  void Execute(ToolExecutionCallback callback) override;
  void Cancel() override;
  base::WeakPtr<web::WebState> GetTargetWebState() const override;
  ToolType GetToolType() const override;
  std::string DebugString() const override;

  // web::WebStateObserver:
  void DidStartNavigation(web::WebState* web_state,
                          web::NavigationContext* navigation_context) override;
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void WebStateDestroyed(web::WebState* web_state) override;

 private:
  HistoryTool(base::WeakPtr<web::WebState> web_state, bool is_back_action);

  void OnNavigationTimeout();

  bool is_back_action_;
  base::WeakPtr<web::WebState> web_state_;

  ToolExecutionCallback invoke_callback_;
  std::optional<int64_t> pending_navigation_id_;
  base::OneShotTimer navigation_timeout_timer_;
  base::ScopedObservation<web::WebState, web::WebStateObserver>
      web_state_observation_{this};

  base::WeakPtrFactory<HistoryTool> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_HISTORY_TOOL_H_
