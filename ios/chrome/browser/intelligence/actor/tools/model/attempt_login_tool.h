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
#import "base/timer/timer.h"
#import "base/types/expected.h"
#import "components/password_manager/core/browser/actor_login/actor_login_types.h"
#import "components/password_manager/core/browser/password_form_cache.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool.h"
#import "ios/web/public/web_state_observer.h"

class ActorLoginQualityLogger;

namespace optimization_guide {
namespace proto {
class AttemptLoginAction;
}  // namespace proto
}  // namespace optimization_guide

namespace password_manager {
class PasswordFormManager;
}  // namespace password_manager

namespace web {
class WebState;
}  // namespace web

namespace actor {

class ToolDelegate;
struct CredentialWithPermission;

// Tool to attempt login on a page.
class AttemptLoginTool : public ActorTool,
                         public web::WebStateObserver,
                         public password_manager::PasswordFormManagerObserver {
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

  // password_manager::PasswordFormManagerObserver:
  void OnPasswordFormParsed(
      password_manager::PasswordFormManager* form_manager) override;

  // web::WebStateObserver:
  void WasShown(web::WebState* web_state) override;
  void WebStateDestroyed(web::WebState* web_state) override;

 private:
  AttemptLoginTool(base::WeakPtr<web::WebState> web_state,
                   ToolDelegate* tool_delegate);

  void OnGetCredentials(actor_login::CredentialsOrError credentials);
  void OnCredentialSelected(
      base::expected<std::optional<CredentialWithPermission>,
                     ToolExecutionResult> result);
  void OnAttemptLogin(actor_login::Credential selected_credential,
                      bool should_store_permission,
                      actor_login::LoginStatusResultOrError login_status);

  // Called when the initial DOM extraction step from the rescan attempt
  // completes. If no forms were found in the DOM, completes rescanning with
  // failure immediately without waiting for `rescan_timer_`.
  void OnDomRescanComplete(bool forms_found_in_dom);

  // Called when password forms in `web_state_` have finished rescanning and are
  // registered with the password manager. Continues the login attempt if
  // `forms_found` is true.
  void OnWebStateRescanComplete(bool forms_found);

  // Temporarily stores the parameters to the last call to `AttemptLogin`.
  void SaveCredentialsAndPermission(actor_login::Credential selected_credential,
                                    bool should_store_permission);

  // Retries login with temporarily stored parameters, if available.
  void RetryLoginWithSavedCredentials();

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

  // Temporarily saved credential and permission status.
  std::optional<actor_login::Credential> selected_credential_;
  bool should_store_permission_ = false;

  // If `true`, the tool execution is currently blocked on a device
  // reauthentication.
  bool waiting_for_reauth_ = false;

  // Whether a rescan of the web state to find a login form is attempted.
  bool web_state_rescan_attempted_ = false;

  // Timer for timing out form rescan if password extraction takes too long, as
  // well as a helper flag to track whether the rescan is in progress when the
  // timer fires.
  base::OneShotTimer rescan_timer_;
  bool rescan_in_progress_ = false;

  // Observes events of the web state being actuated on by the tool.
  base::ScopedObservation<web::WebState, web::WebStateObserver>
      web_state_observation_{this};

  // Whether this tool is currently observing PasswordFormCache for parsed
  // forms.
  bool observing_form_cache_ = false;

  base::WeakPtrFactory<AttemptLoginTool> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_ATTEMPT_LOGIN_TOOL_H_
