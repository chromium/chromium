// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_ATTEMPT_LOGIN_TOOL_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_ATTEMPT_LOGIN_TOOL_H_

#import <memory>
#import <optional>
#import <vector>

#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/scoped_observation.h"
#import "base/time/time.h"
#import "base/types/expected.h"
#import "components/password_manager/core/browser/actor_login/actor_login_types.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool.h"
#import "ios/web/public/web_state_observer.h"

class ActorLoginQualityLogger;

namespace optimization_guide {
namespace proto {
class AttemptLoginAction;
}  // namespace proto
}  // namespace optimization_guide

namespace web {
class WebState;
}  // namespace web

namespace actor {

class ToolDelegate;

// Tool to attempt login on a page.
class AttemptLoginTool : public ActorTool, public web::WebStateObserver {
 public:
  static std::unique_ptr<AttemptLoginTool> Create(
      base::WeakPtr<web::WebState> web_state,
      const optimization_guide::proto::AttemptLoginAction& action,
      ToolDelegate* tool_delegate);

  ~AttemptLoginTool() override;

  // ActorTool:
  void Validate(ToolExecutionCallback callback) override;
  void Execute(ToolExecutionCallback callback) override;
  void Cancel() override;
  base::WeakPtr<web::WebState> GetTargetWebState() const override;
  ToolType GetToolType() const override;

  // web::WebStateObserver:
  void WasShown(web::WebState* web_state) override;
  void WebStateDestroyed(web::WebState* web_state) override;

 private:
  AttemptLoginTool(base::WeakPtr<web::WebState> web_state,
                   ToolDelegate* tool_delegate);

  void OnGetCredentials(actor_login::CredentialsOrError credentials);
  void OnCredentialSelected(
      std::optional<actor_login::Credential> selected_credential,
      bool should_store_permission);
  void OnAttemptLogin(actor_login::Credential selected_credential,
                      bool should_store_permission,
                      actor_login::LoginStatusResultOrError login_status);

  // The tab and navigation item this tool actuates on.
  base::WeakPtr<web::WebState> web_state_;
  std::optional<int> navigation_item_id_;

  // Delegate object for the tool to interact with its invoking task.
  raw_ptr<ToolDelegate> tool_delegate_;

  // The time when the tool is created.
  base::TimeTicks attempt_login_tool_start_time_;

  // Manager that logs model quality and uploads logs to the server.
  std::unique_ptr<ActorLoginQualityLogger> quality_logger_;

  // Callback that signals the tool execution result.
  ToolExecutionCallback execute_callback_;

  // Temporarily stores the parameters to the last call to `AttemptLogin` in
  // case the tab is not in focus when authentication is needed.
  std::optional<actor_login::Credential> selected_credential_;
  bool should_store_permission_ = false;

  // Observes events of the web state being actuated on by the tool.
  base::ScopedObservation<web::WebState, web::WebStateObserver>
      web_state_observation_{this};

  base::WeakPtrFactory<AttemptLoginTool> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_ATTEMPT_LOGIN_TOOL_H_
