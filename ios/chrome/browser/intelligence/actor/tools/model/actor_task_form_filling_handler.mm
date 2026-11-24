// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/actor_task_form_filling_handler.h"

#import <utility>

#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/memory/ptr_util.h"
#import "base/task/bind_post_task.h"
#import "base/task/sequenced_task_runner.h"
#import "components/actor/core/aggregated_journal.h"
#import "components/autofill/core/browser/actor/actor_form_filling_service_impl.h"
#import "components/autofill/core/browser/integrators/actor/actor_form_filling_types.h"
#import "components/autofill/core/browser/suggestions/suggestion_type.h"
#import "components/password_manager/core/browser/actor_login/actor_login_service_impl.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_task_intervention_delegate.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_form_suggestion.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_form_suggestion_favicon_loader.h"

namespace actor {

// static
std::unique_ptr<ActorTaskFormFillingHandler>
ActorTaskFormFillingHandler::Create(base::PassKey<ActorEngine> pass_key,
                                    AggregatedJournal& journal,
                                    ActorTaskId task_id,
                                    FaviconLoader* favicon_loader) {
  return base::WrapUnique(new ActorTaskFormFillingHandler(
      task_id, std::make_unique<actor_login::ActorLoginServiceImpl>(),
      std::make_unique<autofill::ActorFormFillingServiceImpl>(
          journal.GetSafeRef(), task_id),
      favicon_loader));
}

// static
std::unique_ptr<ActorTaskFormFillingHandler>
ActorTaskFormFillingHandler::CreateForTesting(
    ActorTaskId task_id,
    std::unique_ptr<actor_login::ActorLoginService> login_service,
    std::unique_ptr<autofill::ActorFormFillingService> form_filling_service,
    FaviconLoader* favicon_loader) {
  return base::WrapUnique(new ActorTaskFormFillingHandler(
      task_id, std::move(login_service), std::move(form_filling_service),
      favicon_loader));
}

ActorTaskFormFillingHandler::ActorTaskFormFillingHandler(
    ActorTaskId task_id,
    std::unique_ptr<actor_login::ActorLoginService> login_service,
    std::unique_ptr<autofill::ActorFormFillingService> form_filling_service,
    FaviconLoader* favicon_loader)
    : task_id_(task_id),
      login_service_(std::move(login_service)),
      form_filling_service_(std::move(form_filling_service)),
      favicon_loader_([[ActorFormSuggestionFaviconLoader alloc]
          initWithFaviconLoader:favicon_loader]) {}

ActorTaskFormFillingHandler::~ActorTaskFormFillingHandler() {
  [favicon_loader_ disconnect];
}

void ActorTaskFormFillingHandler::PromptToSelectCredential(
    const std::vector<actor_login::Credential>& credentials,
    CredentialSelectedCallback callback) {
  CHECK(!credential_selected_callback_);
  CHECK(!autofill_suggestion_selected_callback_);
  CHECK(!credentials.empty());

  if (!intervention_delegate_) {
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
    ActorFormSuggestion* suggestion =
        [[ActorFormSuggestion alloc] initWithCredential:credential];
    suggestion.faviconDataSource = favicon_loader_;
    [suggestions addObject:suggestion];
  }

  credential_selected_callback_ = std::move(callback);
  has_pending_prompt_ = true;
  // Posted so that a delegate completing synchronously never re-enters the
  // handler or the tool.
  ActorSuggestionSelectionCallback on_selected =
      base::CallbackToBlock(base::BindPostTaskToCurrentDefault(base::BindOnce(
          &ActorTaskFormFillingHandler::OnCredentialSelectedByUser,
          weak_ptr_factory_.GetWeakPtr())));

  [intervention_delegate_ actorTask:task_id_
              selectFromSuggestions:suggestions
                           callback:on_selected];
}

void ActorTaskFormFillingHandler::RegisterAutofillSuggestionsAndCallback(
    const std::vector<autofill::ActorFormFillingRequest>& requests,
    AutofillSuggestionSelectedCallback callback) {
  CHECK(!credential_selected_callback_);
  CHECK(!autofill_suggestion_selected_callback_);
  CHECK(!requests.empty());

  // Retrieve all suggestions in `requests`.
  NSMutableArray<NSArray<ActorFormSuggestion*>*>* all_suggestions =
      [NSMutableArray array];
  for (const auto& request : requests) {
    CHECK(!request.suggestions.empty());
    autofill::ActorFormFillingRequestedData requested_data =
        request.requested_data;
    if (requested_data == autofill::ActorFormFillingRequestedData::kUnknown) {
      ToolExecutionResult error_result(
          mojom::ActionResultCode::kFormFillingUnknownAutofillError,
          /*requires_page_stabilization=*/false, "Data type unknown.");
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), 0,
                                    base::unexpected(error_result)));
      return;
    }
    NSMutableArray<ActorFormSuggestion*>* suggestions = [NSMutableArray array];
    for (const auto& suggestion : request.suggestions) {
      [suggestions addObject:[[ActorFormSuggestion alloc]
                                 initWithActorSuggestion:suggestion
                                                dataType:requested_data]];
    }
    [all_suggestions addObject:suggestions];
  }

  autofill_suggestions_ = all_suggestions;
  autofill_suggestion_selected_callback_ = std::move(callback);
}

void ActorTaskFormFillingHandler::PromptToSelectAutofillSuggestion(
    size_t index) {
  CHECK(autofill_suggestions_);
  CHECK_LT(index, autofill_suggestions_.count);

  if (!autofill_suggestion_selected_callback_) {
    return;
  }

  if (!intervention_delegate_) {
    ToolExecutionResult error_result(
        mojom::ActionResultCode::kFormFillingDialogError,
        /*requires_page_stabilization=*/false,
        "ActorTaskInterventionDelegate unset.");
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(autofill_suggestion_selected_callback_), index,
                       base::unexpected(error_result)));
    return;
  }

  NSArray<ActorFormSuggestion*>* suggestions = autofill_suggestions_[index];
  has_pending_prompt_ = true;
  // Posted so that a delegate completing synchronously never re-enters the
  // handler or the tool.
  ActorSuggestionSelectionCallback on_selected =
      base::CallbackToBlock(base::BindPostTaskToCurrentDefault(base::BindOnce(
          &ActorTaskFormFillingHandler::OnAutofillSuggestionSelectedByUser,
          weak_ptr_factory_.GetWeakPtr(), index)));

  [intervention_delegate_ actorTask:task_id_
              selectFromSuggestions:suggestions
                           callback:on_selected];
}

void ActorTaskFormFillingHandler::CancelPendingPrompt() {
  credential_selected_callback_.Reset();
  autofill_suggestion_selected_callback_.Reset();
  autofill_suggestions_ = nil;
  if (!std::exchange(has_pending_prompt_, false)) {
    return;
  }
  // Ignore late answers to the dropped prompt.
  weak_ptr_factory_.InvalidateWeakPtrs();
  [intervention_delegate_ actorTaskDidCancelSuggestionSelection:task_id_];
}

void ActorTaskFormFillingHandler::OnCredentialSelectedByUser(
    InterventionDelegateResult<SuggestionSelection> result) {
  has_pending_prompt_ = false;
  CHECK(credential_selected_callback_);
  if (!result.has_value()) {
    std::move(credential_selected_callback_)
        .Run(base::unexpected(
            ToolExecutionResult(mojom::ActionResultCode::kActorUiError,
                                /*requires_page_stabilization=*/false,
                                "Showing credentials to the user failed.")));
    return;
  }
  ActorFormSuggestion* selected_suggestion = result.value().suggestion;
  if (!selected_suggestion.formSuggestion) {
    std::move(credential_selected_callback_).Run(std::nullopt);
    return;
  }
  CHECK(selected_suggestion.type == autofill::SuggestionType::kPasswordEntry);
  SetUserSelectedCredential(selected_suggestion.credential,
                            result.value().should_store_permission);
}

void ActorTaskFormFillingHandler::OnAutofillSuggestionSelectedByUser(
    size_t form_index,
    InterventionDelegateResult<SuggestionSelection> result) {
  has_pending_prompt_ = false;
  CHECK(autofill_suggestion_selected_callback_);
  if (!result.has_value()) {
    autofill_suggestion_selected_callback_.Run(
        form_index, base::unexpected(ToolExecutionResult(
                        mojom::ActionResultCode::kFormFillingDialogError,
                        /*requires_page_stabilization=*/false,
                        "Showing suggestions to the user failed.")));
    return;
  }
  ActorFormSuggestion* selected_suggestion = result.value().suggestion;
  if (!selected_suggestion.formSuggestion) {
    autofill_suggestion_selected_callback_.Run(form_index, std::nullopt);
    return;
  }
  CHECK(selected_suggestion.type == autofill::SuggestionType::kAddressEntry ||
        selected_suggestion.type == autofill::SuggestionType::kCreditCardEntry);
  CHECK(selected_suggestion.autofillSuggestion.has_value());

  autofill_suggestion_selected_callback_.Run(
      form_index, *selected_suggestion.autofillSuggestion);
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
  std::move(credential_selected_callback_).Run(credential_with_permission);
}

}  // namespace actor
