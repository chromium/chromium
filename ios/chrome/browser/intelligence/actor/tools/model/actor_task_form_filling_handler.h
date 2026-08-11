// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_ACTOR_TASK_FORM_FILLING_HANDLER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_ACTOR_TASK_FORM_FILLING_HANDLER_H_

#import <memory>
#import <optional>
#import <vector>

#import "base/containers/flat_map.h"
#import "base/functional/callback_forward.h"
#import "base/memory/ptr_util.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/types/expected.h"
#import "base/types/pass_key.h"
#import "components/password_manager/core/browser/actor_login/actor_login_types.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "url/origin.h"

namespace actor_login {
class ActorLoginService;
}  // namespace actor_login

namespace autofill {
class ActorFormFillingService;
struct ActorFormFillingRequest;
struct ActorSuggestion;
}  // namespace autofill

@class ActorFormSuggestion;
@protocol ActorTaskInterventionDelegate;
@class FormSuggestion;

namespace actor {

class ActorTask;
class AggregatedJournal;

// Combines a `credential` and a user choice in the account picker, and
// whether user has permitted Chrome to always actuate with this credential.
struct CredentialWithPermission {
  actor_login::Credential credential;
  bool always_allow = false;
};

class ActorTaskFormFillingHandler {
 public:
  // Static public constructor.
  static std::unique_ptr<ActorTaskFormFillingHandler> Create(
      base::PassKey<ActorTask> pass_key,
      AggregatedJournal& journal,
      ActorTaskId task_id);

  // Static constructor for testing purposes; this allows tests to override the
  // services.
  static std::unique_ptr<ActorTaskFormFillingHandler> CreateForTesting(
      ActorTaskId task_id,
      std::unique_ptr<actor_login::ActorLoginService> login_service,
      std::unique_ptr<autofill::ActorFormFillingService> form_filling_service);

  ~ActorTaskFormFillingHandler();

  ActorTaskFormFillingHandler(const ActorTaskFormFillingHandler&) = delete;
  ActorTaskFormFillingHandler& operator=(const ActorTaskFormFillingHandler&) =
      delete;

  // Form filling service accessor.
  autofill::ActorFormFillingService* GetActorFormFillingService() {
    return form_filling_service_.get();
  }

  // Returns the ActorLoginService used to log into websites.
  actor_login::ActorLoginService* GetActorLoginService() {
    return login_service_.get();
  }

  // Sets the intervention delegate.
  void SetInterventionDelegate(
      base::PassKey<ActorTask> pass_key,
      id<ActorTaskInterventionDelegate> intervention_delegate) {
    intervention_delegate_ = intervention_delegate;
  }
  void SetInterventionDelegateForTesting(
      id<ActorTaskInterventionDelegate> intervention_delegate) {
    intervention_delegate_ = intervention_delegate;
  }

  // Prompts the user to select a credential from the list of credentials.
  // The callback is called with the selected credential and whether the
  // permission should be stored, or with std::nullopt / error if the user
  // closed the prompt or an error occurred.
  using CredentialSelectedCallback = base::OnceCallback<void(
      base::expected<std::optional<CredentialWithPermission>,
                     ToolExecutionResult>)>;
  void PromptToSelectCredential(
      const std::vector<actor_login::Credential>& credentials,
      CredentialSelectedCallback callback);

  // Registers the callback and parses the list of autofill suggestions to cache
  // them. Does not display the prompt.
  using AutofillSuggestionSelectedCallback = base::RepeatingCallback<void(
      size_t form_index,
      base::expected<std::optional<autofill::ActorSuggestion>,
                     ToolExecutionResult>)>;
  void RegisterAutofillSuggestionsAndCallback(
      const std::vector<autofill::ActorFormFillingRequest>& requests,
      AutofillSuggestionSelectedCallback callback);

  // Prompts the user to select from the registered suggestions for the form at
  // `index`. The callback registered in
  // `RegisterAutofillSuggestionsAndCallback` will be called with the selected
  // suggestion and `index`, or with std::nullopt if the user closed the prompt
  // without making a selection.
  void PromptToSelectAutofillSuggestion(size_t index);

  // Retrieves the credential that the user has chosen to allow the
  // actor to use. The selected credential can be used for multi-step login
  // within the same task.
  std::optional<CredentialWithPermission> GetUserSelectedCredential(
      const url::Origin& request_origin) const;

 private:
  ActorTaskFormFillingHandler(
      ActorTaskId task_id,
      std::unique_ptr<actor_login::ActorLoginService> login_service,
      std::unique_ptr<autofill::ActorFormFillingService> form_filling_service);

  // TODO(crbug.com/472291829): Implement affiliation service related logic to
  // fetch affiliated domains so we can reuse the permission.
  // Caches any user selected credential during task execution.
  void SetUserSelectedCredential(
      std::optional<actor_login::Credential> credential,
      bool should_store_permission);

  // Handles the user selection of a credential suggestion.
  void OnCredentialSelectedByUser(ActorFormSuggestion* selected_suggestion,
                                  bool always_allow);

  // Handles the user selection of an autofill suggestion.
  void OnAutofillSuggestionSelectedByUser(
      ActorFormSuggestion* selected_suggestion,
      size_t form_index);

  ActorTaskId task_id_;
  std::unique_ptr<actor_login::ActorLoginService> login_service_;
  std::unique_ptr<autofill::ActorFormFillingService> form_filling_service_;

  // For multi-step login, these are the credentials that the user has chosen to
  // allow the actor to use in this task, as well as whether the user has given
  // permission for this credential to always be used.
  base::flat_map<url::Origin, CredentialWithPermission>
      user_selected_credentials_;

  // Double-array storing the parsed suggestions for each form request from
  // `RegisterAutofillSuggestionsAndCallback`.
  NSArray<NSArray<ActorFormSuggestion*>*>* autofill_suggestions_ = nil;

  // The intervention delegate that handles UI interactions, e.g. showing a
  // picker.
  __weak id<ActorTaskInterventionDelegate> intervention_delegate_;

  // Success callback for credential selection.
  CredentialSelectedCallback credential_selected_callback_;
  // Success callback for autofill suggestion selection other than credentials.
  AutofillSuggestionSelectedCallback autofill_suggestion_selected_callback_;

  base::WeakPtrFactory<ActorTaskFormFillingHandler> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_ACTOR_TASK_FORM_FILLING_HANDLER_H_
