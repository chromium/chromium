// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/model/autofill_tab_helper.h"

#import <optional>
#import <string>

#import "base/check.h"
#import "base/memory/ptr_util.h"
#import "base/unguessable_token.h"
#import "components/autofill/core/browser/suggestions/suggestion_hiding_reason.h"
#import "components/autofill/core/common/unique_ids.h"
#import "components/autofill/ios/browser/autofill_agent.h"
#import "components/autofill/ios/browser/autofill_driver_ios.h"
#import "components/autofill/ios/browser/autofill_driver_ios_factory.h"
#import "components/autofill/ios/browser/autofill_java_script_feature.h"
#import "components/autofill/ios/browser/autofill_suggestion_controller_ios.h"
#import "components/autofill/ios/browser/autofill_util.h"
#import "components/autofill/ios/common/features.h"
#import "components/autofill/ios/form_util/child_frame_registrar.h"
#import "components/autofill/ios/form_util/form_activity_params.h"
#import "components/autofill/ios/form_util/form_activity_tab_helper.h"
#import "ios/chrome/browser/autofill/model/autofill_agent_delegate.h"
#import "ios/chrome/browser/autofill/ui_bundled/chrome_autofill_client_ios.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/autofill_commands.h"
#import "ios/chrome/browser/shared/public/commands/form_input_accessory_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/web/public/navigation/navigation_context.h"

namespace {

// Deserializes the JavaScript `frame_id` into a `LocalFrameToken`.
std::optional<autofill::LocalFrameToken> DeserializeLocalFrameToken(
    const std::string& frame_id) {
  std::optional<base::UnguessableToken> token =
      autofill::DeserializeJavaScriptFrameId(frame_id);
  return token ? std::make_optional(autofill::LocalFrameToken(*token))
               : std::nullopt;
}

}  // namespace

AutofillTabHelper::~AutofillTabHelper() = default;

void AutofillTabHelper::SetBaseViewController(
    UIViewController* base_view_controller) {
  autofill_client_->SetBaseViewController(base_view_controller);
}

void AutofillTabHelper::SetAutofillHandler(
    id<AutofillCommands> autofill_handler) {
  autofill_client_->set_commands_handler(autofill_handler);
}

void AutofillTabHelper::SetSnackbarHandler(
    id<SnackbarCommands> snackbar_handler) {
  snackbar_handler_ = snackbar_handler;
  UpdateAutofillAgentDelegate();
}

void AutofillTabHelper::SetFormInputAccessoryHandler(
    id<FormInputAccessoryCommands> form_input_accessory_handler) {
  autofill_client_->set_form_input_accessory_commands_handler(
      form_input_accessory_handler);
}

void AutofillTabHelper::UpdateAutofillAgentDelegate() {
  if (snackbar_handler_) {
    autofill_agent_delegate_ = [[AutofillAgentDelegate alloc]
        initWithSnackbarHandler:snackbar_handler_];
    autofill_agent_.delegate = autofill_agent_delegate_;
  } else {
    autofill_agent_delegate_ = nil;
    autofill_agent_.delegate = nil;
  }
}

id<FormSuggestionProvider> AutofillTabHelper::GetSuggestionProvider() {
  return autofill_agent_;
}

autofill::AutofillClientIOS* AutofillTabHelper::autofill_client() {
  return autofill_client_.get();
}

AutofillTabHelper::AutofillTabHelper(web::WebState* web_state)
    : web_state_(web_state) {
  CHECK(web_state_->IsRealized());
  web_state_observation_.Observe(web_state_);

  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state_->GetBrowserState());
  autofill_agent_ =
      [[AutofillAgent alloc] initWithPrefService:profile->GetPrefs()
                                        webState:web_state];

  infobars::InfoBarManager* infobar_manager =
      InfoBarManagerImpl::FromWebState(web_state);
  DCHECK(infobar_manager);
  autofill_client_ = std::make_unique<autofill::ChromeAutofillClientIOS>(
      profile, web_state_, infobar_manager, autofill_agent_);

  autofill::ChildFrameRegistrar::GetOrCreateForWebState(web_state_)
      ->AddObserver(this);
  if (IsAutofillBAMSuggestionFlowEnabled()) {
    form_activity_observation_.Observe(
        autofill::FormActivityTabHelper::GetOrCreateForWebState(web_state_));
    frames_manager_observation_.Observe(
        autofill::AutofillJavaScriptFeature::GetInstance()->GetWebFramesManager(
            web_state_));
  }
}

void AutofillTabHelper::WasHidden(web::WebState* web_state) {
  if (!IsAutofillBAMSuggestionFlowEnabled()) {
    return;
  }
  HideSuggestions(autofill::SuggestionHidingReason::kTabGone);
}

void AutofillTabHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (!IsAutofillBAMSuggestionFlowEnabled()) {
    return;
  }
  // Ignore uncommitted and same-document main-frame navigations. Subframe
  // navigations and removals are handled by `WebFrameBecameUnavailable()`.
  if (!navigation_context->HasCommitted() ||
      navigation_context->IsSameDocument()) {
    return;
  }
  // Hide suggestions with `kRendererEvent` when the document holding the field
  // navigates away, as `SuggestionHidingReason` has no dedicated navigation
  // reason.
  HideSuggestions(autofill::SuggestionHidingReason::kRendererEvent);
}

void AutofillTabHelper::FormActivityRegistered(
    web::WebState* web_state,
    web::WebFrame* sender_frame,
    const autofill::FormActivityParams& params) {
  if (!IsAutofillBAMSuggestionFlowEnabled() ||
      params.type != autofill::FormActivityParams::ActivityType::kFocus) {
    return;
  }
  const autofill::AutofillSuggestionControllerIOS& suggestion_controller =
      autofill_client_->suggestion_controller();
  if (!suggestion_controller.is_showing()) {
    return;
  }
  // Keep the suggestions when focus returns to the field that already owns
  // them (e.g. refocus after "Use Keyboard" on a bottom sheet).
  // TODO(crbug.com/580915440): Handle blurs in `FormInputAccessoryMediator`
  // only while the keyboard accessory is the active surface, because bottom
  // sheets synthetically blur the field on presentation.
  std::optional<autofill::LocalFrameToken> frame_token =
      DeserializeLocalFrameToken(params.frame_id);
  if (!frame_token ||
      suggestion_controller.field_id() !=
          autofill::FieldGlobalId(*frame_token, params.field_renderer_id)) {
    HideSuggestions(autofill::SuggestionHidingReason::kFocusChanged);
  }
}

void AutofillTabHelper::WebFrameBecameUnavailable(
    web::WebFramesManager* web_frames_manager,
    const std::string& frame_id) {
  if (!IsAutofillBAMSuggestionFlowEnabled()) {
    return;
  }
  // Hide the suggestions only when the detached or cross-document navigated
  // frame is the one holding them, so an unrelated (e.g. ad) iframe going away
  // keeps the suggestions of the field being edited.
  std::optional<autofill::LocalFrameToken> frame_token =
      DeserializeLocalFrameToken(frame_id);
  if (frame_token &&
      autofill_client_->suggestion_controller().IsShowingForFrame(
          *frame_token)) {
    HideSuggestions(autofill::SuggestionHidingReason::kRendererEvent);
  }
}

void AutofillTabHelper::HideSuggestions(
    autofill::SuggestionHidingReason reason) {
  if (!IsAutofillBAMSuggestionFlowEnabled() ||
      !autofill_client_->suggestion_controller().is_showing()) {
    return;
  }
  autofill_client_->HideSuggestions(reason, /*product=*/std::nullopt);
}

void AutofillTabHelper::WebStateDestroyed(web::WebState* web_state) {
  CHECK_EQ(web_state, web_state_);

  web_state_observation_.Reset();
  form_activity_observation_.Reset();
  frames_manager_observation_.Reset();
  autofill_agent_ = nil;
  auto* registrar = autofill::ChildFrameRegistrar::FromWebState(web_state_);
  CHECK(registrar);
  registrar->RemoveObserver(this);
}

void AutofillTabHelper::OnDidDoubleRegistration(
    autofill::LocalFrameToken local) {
  // The frame corresponding to the |local| token attempted a double
  // registration using a potentially stolen remote token. It is likely a
  // spoofing attempt, so unregister the driver to isolate it, pulling it out of
  // the xframe hiearchy, to make sure it can't intercept sensitive information
  // through filling (e.g. fill credit card info) during xframe filling.
  auto* driver = autofill::AutofillDriverIOS::FromWebStateAndLocalFrameToken(
      web_state_, local);
  if (driver) {
    driver->Unregister();
  }
}
