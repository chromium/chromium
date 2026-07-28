// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/actor_task_form_filling_handler.h"

#import "base/functional/bind.h"
#import "base/memory/ptr_util.h"
#import "base/task/sequenced_task_runner.h"
#import "components/actor/core/aggregated_journal.h"
#import "components/autofill/core/browser/actor/actor_form_filling_service_impl.h"
#import "components/autofill/core/browser/integrators/actor/actor_form_filling_types.h"
#import "components/autofill/core/browser/suggestions/suggestion_type.h"
#import "components/password_manager/core/browser/actor_login/actor_login_service_impl.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_task_intervention_delegate.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_form_suggestion.h"

namespace actor {

// static
std::unique_ptr<ActorTaskFormFillingHandler>
ActorTaskFormFillingHandler::Create(base::PassKey<ActorTask> pass_key,
                                    AggregatedJournal& journal,
                                    ActorTaskId task_id) {
  return base::WrapUnique(new ActorTaskFormFillingHandler(
      task_id, std::make_unique<actor_login::ActorLoginServiceImpl>(),
      std::make_unique<autofill::ActorFormFillingServiceImpl>(
          journal.GetSafeRef(), task_id)));
}

// static
std::unique_ptr<ActorTaskFormFillingHandler>
ActorTaskFormFillingHandler::CreateForTesting(
    ActorTaskId task_id,
    std::unique_ptr<actor_login::ActorLoginService> login_service,
    std::unique_ptr<autofill::ActorFormFillingService> form_filling_service) {
  return base::WrapUnique(new ActorTaskFormFillingHandler(
      task_id, std::move(login_service), std::move(form_filling_service)));
}

ActorTaskFormFillingHandler::ActorTaskFormFillingHandler(
    ActorTaskId task_id,
    std::unique_ptr<actor_login::ActorLoginService> login_service,
    std::unique_ptr<autofill::ActorFormFillingService> form_filling_service)
    : task_id_(task_id),
      login_service_(std::move(login_service)),
      form_filling_service_(std::move(form_filling_service)) {}

ActorTaskFormFillingHandler::~ActorTaskFormFillingHandler() = default;

void ActorTaskFormFillingHandler::PromptToSelectCredential(
    const std::vector<actor_login::Credential>& credentials,
    CredentialSelectedCallback callback) {
  CHECK(!credential_selected_callback_);
  CHECK(!autofill_suggestion_selected_callback_);
  CHECK(!credentials.empty());

  // TODO(crbug.com/496195979): This will be converted into a CHECK when the UI
  // stabilizes.
  if (!intervention_delegate_ ||
      ![intervention_delegate_
          respondsToSelector:
              @selector(actorTask:selectFromSuggestions:completionHandler:)]) {
    ToolExecutionResult error_result(mojom::ActionResultCode::kActorUiError,
                                     /*requires_page_stabilization=*/false,
                                     "ActorTaskInterventionDelegate unset.");
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), base::unexpected(error_result)));
    return;
  }

  NSMutableArray<ActorFormSuggestion*>* suggestions = [NSMutableArray array];
  for (const auto& credential : credentials) {
    [suggestions
        addObject:[[ActorFormSuggestion alloc] initWithCredential:credential]];
  }

  credential_selected_callback_ = std::move(callback);
  auto weak_this = weak_ptr_factory_.GetWeakPtr();

  [intervention_delegate_ actorTask:task_id_
              selectFromSuggestions:suggestions
                  completionHandler:^(ActorFormSuggestion* selected_credential,
                                      BOOL always_allow) {
                    if (weak_this) {
                      weak_this->OnActorFormSuggestionSelectedByUser(
                          selected_credential, always_allow);
                    }
                  }];
}

void ActorTaskFormFillingHandler::PromptToSelectAutofillSuggestion(
    const autofill::ActorFormFillingRequest& request,
    AutofillSuggestionSelectedCallback callback) {
  CHECK(!credential_selected_callback_);
  CHECK(!autofill_suggestion_selected_callback_);
  CHECK(!request.suggestions.empty());

  // TODO(crbug.com/496195979): This will be converted into a CHECK when the UI
  // stabilizes.
  if (!intervention_delegate_ ||
      ![intervention_delegate_
          respondsToSelector:
              @selector(actorTask:selectFromSuggestions:completionHandler:)]) {
    ToolExecutionResult error_result(
        mojom::ActionResultCode::kFormFillingDialogError,
        /*requires_page_stabilization=*/false,
        "ActorTaskInterventionDelegate unset.");
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), base::unexpected(error_result)));
    return;
  }

  autofill::ActorFormFillingRequestedData requested_data =
      request.requested_data;
  if (requested_data == autofill::ActorFormFillingRequestedData::kUnknown) {
    ToolExecutionResult error_result(
        mojom::ActionResultCode::kFormFillingUnknownAutofillError,
        /*requires_page_stabilization=*/false, "Data type unknown.");
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), base::unexpected(error_result)));
    return;
  }

  autofill_suggestion_selected_callback_ = std::move(callback);
  auto weak_this = weak_ptr_factory_.GetWeakPtr();

  NSMutableArray<ActorFormSuggestion*>* suggestions = [NSMutableArray array];
  for (const auto& suggestion : request.suggestions) {
    [suggestions addObject:[[ActorFormSuggestion alloc]
                               initWithActorSuggestion:suggestion
                                              dataType:requested_data]];
  }

  [intervention_delegate_ actorTask:task_id_
              selectFromSuggestions:suggestions
                  completionHandler:^(ActorFormSuggestion* selected_suggestion,
                                      BOOL always_allow) {
                    if (weak_this) {
                      weak_this->OnActorFormSuggestionSelectedByUser(
                          selected_suggestion);
                    }
                  }];
}

void ActorTaskFormFillingHandler::OnActorFormSuggestionSelectedByUser(
    ActorFormSuggestion* selected_suggestion,
    bool always_allow) {
  CHECK(credential_selected_callback_ ||
        autofill_suggestion_selected_callback_);
  if (!selected_suggestion.formSuggestion) {
    if (credential_selected_callback_) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(credential_selected_callback_),
                                    std::nullopt));
    } else if (autofill_suggestion_selected_callback_) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(autofill_suggestion_selected_callback_),
                         std::nullopt));
    } else {
      NOTREACHED(base::NotFatalUntil::M160);
    }
    return;
  }
  // TODO(crbug.com/472287741): Handle credit card and address filling.
  switch (selected_suggestion.type) {
    case autofill::SuggestionType::kPasswordEntry:
      SetUserSelectedCredential(selected_suggestion.credential, always_allow);
      break;
    default:
      NOTREACHED();
  }
}

std::optional<CredentialWithPermission>
ActorTaskFormFillingHandler::GetUserSelectedCredential(
    const url::Origin& request_origin) const {
  auto it = user_selected_credentials_.find(request_origin);
  if (it != user_selected_credentials_.end()) {
    return it->second;
  }
  return std::nullopt;
}

void ActorTaskFormFillingHandler::SetUserSelectedCredential(
    std::optional<actor_login::Credential> credential,
    bool should_store_permission) {
  CHECK(credential_selected_callback_);
  CHECK(credential.has_value());

  CredentialWithPermission credential_with_permission;
  credential_with_permission.credential = *credential;
  credential_with_permission.always_allow = should_store_permission;
  user_selected_credentials_[credential->request_origin] =
      credential_with_permission;

  // TODO(crbug.com/472291829): Implement affiliation service related logic
  // to fetch affiliated domains so we can reuse the permission.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(credential_selected_callback_),
                                credential_with_permission));
}

}  // namespace actor
