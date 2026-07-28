// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/attempt_login_tool.h"

#import <algorithm>
#import <utility>

#import "base/functional/bind.h"
#import "components/actor/public/mojom/actor_types.mojom.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "components/password_manager/core/browser/actor_login/actor_login_quality_logger.h"
#import "components/password_manager/core/browser/actor_login/actor_login_service.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_task_form_filling_handler.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/tool_delegate.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "ios/chrome/browser/passwords/model/actor_login/ios_chrome_actor_login_delegate_client.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"

namespace actor {

// static
std::unique_ptr<AttemptLoginTool> AttemptLoginTool::Create(
    base::WeakPtr<web::WebState> web_state,
    const optimization_guide::proto::AttemptLoginAction& action,
    ToolDelegate* tool_delegate) {
  return std::unique_ptr<AttemptLoginTool>(
      new AttemptLoginTool(web_state, tool_delegate));
}

AttemptLoginTool::AttemptLoginTool(base::WeakPtr<web::WebState> web_state,
                                   ToolDelegate* tool_delegate)
    : web_state_(web_state),
      tool_delegate_(tool_delegate),
      attempt_login_tool_start_time_(base::TimeTicks::Now()),
      quality_logger_(std::make_unique<ActorLoginQualityLogger>(
          GetApplicationContext()->GetVariationsService())) {}

AttemptLoginTool::~AttemptLoginTool() = default;

void AttemptLoginTool::Cancel() {
  tool_delegate_ = nullptr;
  web_state_observation_.Reset();
  // TODO(crbug.com/472291829): If `selected_credential_` exists, consider
  // uninterrupt from tool depending on Desktop handling.
  selected_credential_.reset();
  ActorTool::Cancel();
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void AttemptLoginTool::Validate(ToolExecutionCallback callback) {
  std::move(callback).Run(ToolExecutionResult::Ok());
}

void AttemptLoginTool::Execute(ToolExecutionCallback callback) {
  if (!web_state_) {
    std::move(callback).Run(
        ToolExecutionResult(mojom::ActionResultCode::kTabWentAway));
    return;
  }
  execute_callback_ = std::move(callback);
  web_state_observation_.Observe(web_state_.get());

  web::NavigationItem* navigation_item =
      web_state_->GetNavigationManager()->GetVisibleItem();
  CHECK(navigation_item, base::NotFatalUntil::M156);
  navigation_item_id_ = navigation_item->GetUniqueID();

  IOSChromeActorLoginDelegateClient* client =
      IOSChromeActorLoginDelegateClient::FromWebState(web_state_.get());
  CHECK(client);

  ActorTaskFormFillingHandler* form_filling_handler =
      tool_delegate_->GetActorTaskFormFillingHandler();

  const url::Origin& current_origin =
      client->GetLastCommittedOriginForMainFrame();
  std::optional<CredentialWithPermission>
      user_selected_credential_and_permission =
          form_filling_handler->GetUserSelectedCredential(current_origin);

  actor_login::ActorLoginService* login_service =
      form_filling_handler->GetActorLoginService();
  CHECK(login_service);

  if (user_selected_credential_and_permission.has_value()) {
    // The task has gained permission to log into the current or an affiliated
    // origin previously. Re-use.
    const actor_login::Credential& credential =
        user_selected_credential_and_permission->credential;
    const bool should_store_permission =
        user_selected_credential_and_permission->always_allow;
    login_service->AttemptLogin(
        client, credential, should_store_permission,
        quality_logger_->AsWeakPtr(), attempt_login_tool_start_time_,
        /*frame_filling_started_cb=*/{},
        base::BindOnce(&AttemptLoginTool::OnAttemptLogin,
                       weak_ptr_factory_.GetWeakPtr(), credential,
                       should_store_permission),
        /*action_sequence_delegate=*/{});
    return;
  }

  login_service->GetCredentials(
      client,
      /*has_sign_in_with_google_button=*/false, quality_logger_->AsWeakPtr(),
      base::BindOnce(&AttemptLoginTool::OnGetCredentials,
                     weak_ptr_factory_.GetWeakPtr()));
}

base::WeakPtr<web::WebState> AttemptLoginTool::GetTargetWebState() const {
  return web_state_;
}

ToolType AttemptLoginTool::GetToolType() const {
  return ToolType::kAttemptLogin;
}

void AttemptLoginTool::OnGetCredentials(
    actor_login::CredentialsOrError credentials) {
  if (!credentials.has_value()) {
    std::move(execute_callback_)
        .Run(ToolExecutionResult(
            actor_login::LoginErrorToActorResult(credentials.error())));
    return;
  }
  std::vector<actor_login::Credential> creds = std::move(credentials.value());
  if (creds.empty()) {
    std::move(execute_callback_)
        .Run(ToolExecutionResult(
            mojom::ActionResultCode::kLoginNoCredentialsAvailable));
    return;
  }
  // If the user has granted persistent permission for any credential to be used
  // on the requested origin, directly select it.
  for (const actor_login::Credential& cred : creds) {
    if (cred.has_persistent_permission) {
      OnCredentialSelected(CredentialWithPermission{
          .credential = cred,
          .always_allow = true,
      });
      return;
    }
  }
  tool_delegate_->GetActorTaskFormFillingHandler()->PromptToSelectCredential(
      creds, base::BindOnce(&AttemptLoginTool::OnCredentialSelected,
                            weak_ptr_factory_.GetWeakPtr()));
}

void AttemptLoginTool::OnCredentialSelected(
    base::expected<std::optional<CredentialWithPermission>, ToolExecutionResult>
        result) {
  if (!result.has_value()) {
    std::move(execute_callback_).Run(result.error());
    return;
  }

  const std::optional<CredentialWithPermission>& selection = result.value();
  if (!selection.has_value()) {
    // We don't need to distinguish between no credentials being available and a
    // user declining the usage of a credential.
    std::move(execute_callback_)
        .Run(ToolExecutionResult(
            mojom::ActionResultCode::kLoginNoCredentialsAvailable));
    return;
  }
  if (web::NavigationItem* visible_item =
          web_state_->GetNavigationManager()->GetVisibleItem();
      !visible_item ||
      navigation_item_id_.value() != visible_item->GetUniqueID()) {
    std::move(execute_callback_)
        .Run(ToolExecutionResult(
            mojom::ActionResultCode::kLoginPageChangedDuringSelection));
    return;
  }

  IOSChromeActorLoginDelegateClient* client =
      IOSChromeActorLoginDelegateClient::FromWebState(web_state_.get());
  CHECK(client);

  const actor_login::Credential credential = selection->credential;
  actor_login::ActorLoginService* login_service =
      tool_delegate_->GetActorTaskFormFillingHandler()->GetActorLoginService();
  CHECK(login_service);
  login_service->AttemptLogin(
      client, credential, selection->always_allow, quality_logger_->AsWeakPtr(),
      attempt_login_tool_start_time_,
      /*frame_filling_started_cb=*/{},
      base::BindOnce(&AttemptLoginTool::OnAttemptLogin,
                     weak_ptr_factory_.GetWeakPtr(), credential,
                     selection->always_allow),
      /*action_sequence_delegate=*/{});
}

void AttemptLoginTool::OnAttemptLogin(
    actor_login::Credential selected_credential,
    bool should_store_permission,
    actor_login::LoginStatusResultOrError login_status) {
  if (!login_status.has_value()) {
    std::move(execute_callback_)
        .Run(ToolExecutionResult(
            actor_login::LoginErrorToActorResult(login_status.error())));
    return;
  }
  if (login_status.value() ==
      actor_login::LoginStatusResult::kErrorDeviceReauthRequired) {
    // This only happens when the task is NOT in focus, otherwise the
    // ActorLoginService would have handled re-auth. Wait for the task to get
    // back into focus.
    selected_credential_ = selected_credential;
    should_store_permission_ = should_store_permission;
    tool_delegate_->InterruptFromTool();
    return;
  }
  std::move(execute_callback_)
      .Run(ToolExecutionResult(
          actor_login::LoginResultToActorResult(login_status.value())));
}

void AttemptLoginTool::WasShown(web::WebState* web_state) {
  CHECK_EQ(web_state_.get(), web_state);

  if (!selected_credential_.has_value()) {
    return;
  }

  tool_delegate_->UninterruptFromTool();
  const actor_login::Credential credential = selected_credential_.value();
  selected_credential_.reset();
  OnCredentialSelected(CredentialWithPermission{
      .credential = credential,
      .always_allow = should_store_permission_,
  });
}

void AttemptLoginTool::WebStateDestroyed(web::WebState* web_state) {
  CHECK_EQ(web_state_.get(), web_state);
  if (execute_callback_) {
    std::move(execute_callback_)
        .Run(ToolExecutionResult(mojom::ActionResultCode::kTabWentAway));
  }
  Cancel();
}

}  // namespace actor
