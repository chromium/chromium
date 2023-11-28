// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/bottom_sheet/autofill_bottom_sheet_tab_helper.h"

#import "base/containers/contains.h"
#import "base/feature_list.h"
#import "base/metrics/histogram_functions.h"
#import "base/ranges/algorithm.h"
#import "components/autofill/core/browser/form_structure.h"
#import "components/autofill/core/browser/personal_data_manager.h"
#import "components/autofill/ios/browser/autofill_driver_ios.h"
#import "components/autofill/ios/form_util/form_activity_params.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/password_manager/ios/password_account_storage_notice_handler.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/autofill/bottom_sheet/autofill_bottom_sheet_java_script_feature.h"
#import "ios/chrome/browser/autofill/bottom_sheet/autofill_bottom_sheet_observer.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/commands/autofill_bottom_sheet_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/web/public/js_messaging/script_message.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/navigation/navigation_context.h"

namespace {

// Whether the provided field type is one which can trigger the Payments Bottom
// Sheet.
bool IsPaymentsBottomSheetTriggeringField(autofill::ServerFieldType type) {
  switch (type) {
    case autofill::CREDIT_CARD_NAME_FULL:
    case autofill::CREDIT_CARD_NUMBER:
    case autofill::CREDIT_CARD_EXP_MONTH:
    case autofill::CREDIT_CARD_EXP_2_DIGIT_YEAR:
    case autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR:
    case autofill::CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR:
    case autofill::CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR:
      return true;
    default:
      return false;
  }
}

}  // namespace

AutofillBottomSheetTabHelper::~AutofillBottomSheetTabHelper() = default;

AutofillBottomSheetTabHelper::AutofillBottomSheetTabHelper(
    web::WebState* web_state,
    id<PasswordsAccountStorageNoticeHandler>
        password_account_storage_notice_handler)
    : password_account_storage_notice_handler_(
          password_account_storage_notice_handler),
      web_state_(web_state) {
  frames_manager_observation_.Observe(
      AutofillBottomSheetJavaScriptFeature::GetInstance()->GetWebFramesManager(
          web_state));
  web_state->AddObserver(this);
}

// Public methods

void AutofillBottomSheetTabHelper::SetAutofillBottomSheetHandler(
    id<AutofillBottomSheetCommands> commands_handler) {
  commands_handler_ = commands_handler;
}

void AutofillBottomSheetTabHelper::AddObserver(
    autofill::AutofillBottomSheetObserver* observer) {
  observers_.AddObserver(observer);
}

void AutofillBottomSheetTabHelper::RemoveObserver(
    autofill::AutofillBottomSheetObserver* observer) {
  observers_.RemoveObserver(observer);
}

void AutofillBottomSheetTabHelper::OnFormMessageReceived(
    const web::ScriptMessage& message) {
  autofill::FormActivityParams params;
  if (!commands_handler_ || !password_account_storage_notice_handler_ ||
      !autofill::FormActivityParams::FromMessage(message, &params)) {
    return;
  }

  const autofill::FieldRendererId renderer_id = params.unique_field_id;
  std::string& frame_id = params.frame_id;
  bool is_password_related =
      base::Contains(registered_password_renderer_ids_[frame_id], renderer_id);
  bool is_payments_related =
      base::Contains(registered_payments_renderer_ids_[frame_id], renderer_id);

  if (is_password_related) {
    ShowPasswordBottomSheet(params);
  } else if (is_payments_related) {
    ShowPaymentsBottomSheet(params);
  }
}

void AutofillBottomSheetTabHelper::ShowPasswordBottomSheet(
    const autofill::FormActivityParams params) {
  if (![password_account_storage_notice_handler_
          shouldShowAccountStorageNotice]) {
    [commands_handler_ showPasswordBottomSheet:params];
    return;
  }

  __weak id<AutofillBottomSheetCommands> weak_commands_handler =
      commands_handler_;
  [password_account_storage_notice_handler_ showAccountStorageNotice:^{
    [weak_commands_handler showPasswordBottomSheet:params];
  }];
}

void AutofillBottomSheetTabHelper::ShowPaymentsBottomSheet(
    const autofill::FormActivityParams params) {
  for (auto& observer : observers_) {
    observer.WillShowPaymentsBottomSheet(params);
  }
  [commands_handler_ showPaymentsBottomSheet:params];
}

void AutofillBottomSheetTabHelper::AttachPasswordListeners(
    const std::vector<autofill::FieldRendererId>& renderer_ids,
    const std::string& frame_id) {
  // Verify that the password bottom sheet feature is enabled and that it hasn't
  // been dismissed too many times.
  if (!base::FeatureList::IsEnabled(
          password_manager::features::kIOSPasswordBottomSheet) ||
      HasReachedDismissLimit()) {
    return;
  }

  AttachListeners(renderer_ids, registered_password_renderer_ids_[frame_id],
                  frame_id);
}

void AutofillBottomSheetTabHelper::AttachListeners(
    const std::vector<autofill::FieldRendererId>& renderer_ids,
    std::set<autofill::FieldRendererId>& registered_renderer_ids,
    const std::string& frame_id) {
  if (!web_state_) {
    return;
  }

  web::WebFramesManager* webFramesManager =
      AutofillBottomSheetJavaScriptFeature::GetInstance()->GetWebFramesManager(
          web_state_);

  web::WebFrame* frame = webFramesManager->GetFrameWithId(frame_id);
  if (!frame) {
    return;
  }

  // Transfer the renderer IDs to a set so that they are sorted and unique.
  std::set<autofill::FieldRendererId> sorted_renderer_ids(renderer_ids.begin(),
                                                          renderer_ids.end());
  // Get vector of new renderer IDs which aren't already registered.
  std::vector<autofill::FieldRendererId> new_renderer_ids;
  base::ranges::set_difference(sorted_renderer_ids, registered_renderer_ids,
                               std::back_inserter(new_renderer_ids));

  if (!new_renderer_ids.empty()) {
    // Enable the bottom sheet on the new renderer IDs.
    AutofillBottomSheetJavaScriptFeature::GetInstance()->AttachListeners(
        new_renderer_ids, frame);

    // Add new renderer IDs to the list of registered renderer IDs.
    std::copy(
        new_renderer_ids.begin(), new_renderer_ids.end(),
        std::inserter(registered_renderer_ids, registered_renderer_ids.end()));
  }
}

void AutofillBottomSheetTabHelper::DetachPasswordListeners(
    const std::string& frame_id,
    bool refocus) {
  // Verify that the password bottom sheet feature is enabled.
  if (!base::FeatureList::IsEnabled(
          password_manager::features::kIOSPasswordBottomSheet) ||
      !web_state_) {
    return;
  }

  web::WebFramesManager* webFramesManager =
      AutofillBottomSheetJavaScriptFeature::GetInstance()->GetWebFramesManager(
          web_state_);
  web::WebFrame* frame = webFramesManager->GetFrameWithId(frame_id);

  AutofillBottomSheetJavaScriptFeature::GetInstance()->DetachListeners(
      registered_password_renderer_ids_[frame_id], frame, refocus);
}

void AutofillBottomSheetTabHelper::DetachPasswordListenersForAllFrames(
    bool refocus) {
  // Verify that the password bottom sheet feature is enabled.
  if (!base::FeatureList::IsEnabled(
          password_manager::features::kIOSPasswordBottomSheet)) {
    return;
  }

  for (auto& registered_renderer_ids : registered_password_renderer_ids_) {
    DetachListenersForFrame(registered_renderer_ids.first,
                            registered_renderer_ids.second, refocus);
  }
}

void AutofillBottomSheetTabHelper::DetachPaymentsListeners(
    const std::string& frame_id,
    bool refocus) {
  // Verify that the payments bottom sheet feature is enabled
  if (!base::FeatureList::IsEnabled(kIOSPaymentsBottomSheet) || !web_state_) {
    return;
  }

  web::WebFramesManager* webFramesManager =
      AutofillBottomSheetJavaScriptFeature::GetInstance()->GetWebFramesManager(
          web_state_);
  web::WebFrame* frame = webFramesManager->GetFrameWithId(frame_id);

  AutofillBottomSheetJavaScriptFeature::GetInstance()->DetachListeners(
      registered_payments_renderer_ids_[frame_id], frame, refocus);
}

void AutofillBottomSheetTabHelper::DetachPaymentsListenersForAllFrames(
    bool refocus) {
  // Verify that the payments bottom sheet feature is enabled
  if (!base::FeatureList::IsEnabled(kIOSPaymentsBottomSheet)) {
    return;
  }

  for (auto& registered_renderer_ids : registered_payments_renderer_ids_) {
    DetachListenersForFrame(registered_renderer_ids.first,
                            registered_renderer_ids.second, refocus);
  }
}

void AutofillBottomSheetTabHelper::DetachListenersForFrame(
    const std::string& frame_id,
    const std::set<autofill::FieldRendererId>& renderer_ids,
    bool refocus) {
  if (!web_state_) {
    return;
  }

  web::WebFramesManager* webFramesManager =
      AutofillBottomSheetJavaScriptFeature::GetInstance()->GetWebFramesManager(
          web_state_);
  web::WebFrame* frame = webFramesManager->GetFrameWithId(frame_id);
  AutofillBottomSheetJavaScriptFeature::GetInstance()->DetachListeners(
      renderer_ids, frame, refocus);
}

// WebStateObserver

void AutofillBottomSheetTabHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (navigation_context->IsSameDocument()) {
    return;
  }

  // Clear all registered renderer ids
  registered_password_renderer_ids_.clear();
  registered_payments_renderer_ids_.clear();
}

void AutofillBottomSheetTabHelper::WebStateDestroyed(web::WebState* web_state) {
  web_state->RemoveObserver(this);
  frames_manager_observation_.Reset();
}

// web::WebFramesManager::Observer:
void AutofillBottomSheetTabHelper::WebFrameBecameAvailable(
    web::WebFramesManager* web_frames_manager,
    web::WebFrame* web_frame) {
  auto* driver = autofill::AutofillDriverIOS::FromWebStateAndWebFrame(
      web_state_, web_frame);
  if (!driver) {
    return;
  }
  autofill_manager_observations_.AddObservation(&driver->GetAutofillManager());
}

// autofill::AutofillManager::Observer

void AutofillBottomSheetTabHelper::OnAutofillManagerDestroyed(
    autofill::AutofillManager& manager) {
  autofill_manager_observations_.RemoveObservation(&manager);
}

void AutofillBottomSheetTabHelper::OnFieldTypesDetermined(
    autofill::AutofillManager& manager,
    autofill::FormGlobalId form_id,
    FieldTypeSource source) {
  if (!base::FeatureList::IsEnabled(kIOSPaymentsBottomSheet)) {
    return;
  }
  autofill::FormStructure* form_structure = manager.FindCachedFormById(form_id);
  if (!form_structure || !form_structure->IsCompleteCreditCardForm()) {
    return;
  }
  if (auto* pdm = manager.client().GetPersonalDataManager();
      pdm->GetCreditCardsToSuggest().empty()) {
    return;
  }
  std::vector<autofill::FieldRendererId> renderer_ids;
  for (const auto& field : form_structure->fields()) {
    if (IsPaymentsBottomSheetTriggeringField(field->Type().GetStorableType())) {
      renderer_ids.push_back(field->unique_renderer_id);
    }
  }
  if (renderer_ids.empty()) {
    return;
  }
  // TODO(crbug.com/1441921): Remove `frame` once `renderer_ids` are
  // FieldGlobalIds.
  web::WebFrame* frame =
      static_cast<autofill::AutofillDriverIOS&>(manager.driver()).web_frame();
  if (!frame) {
    return;
  }
  std::string frame_id = frame->GetFrameId();
  AttachListeners(renderer_ids, registered_payments_renderer_ids_[frame_id],
                  frame_id);
}

// Private methods

bool AutofillBottomSheetTabHelper::HasReachedDismissLimit() {
  PrefService* const pref_service =
      ChromeBrowserState ::FromBrowserState(web_state_->GetBrowserState())
          ->GetPrefs();
  bool dismissLimitReached =
      pref_service->GetInteger(prefs::kIosPasswordBottomSheetDismissCount) >=
      kPasswordBottomSheetMaxDismissCount;
  base::UmaHistogramBoolean("IOS.IsEnabled.Password.BottomSheet",
                            !dismissLimitReached);
  return dismissLimitReached;
}

WEB_STATE_USER_DATA_KEY_IMPL(AutofillBottomSheetTabHelper)
