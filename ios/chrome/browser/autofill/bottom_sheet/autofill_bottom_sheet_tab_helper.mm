// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/bottom_sheet/autofill_bottom_sheet_tab_helper.h"

#import "base/containers/contains.h"
#import "base/feature_list.h"
#import "base/metrics/histogram_functions.h"
#import "base/ranges/algorithm.h"
#import "components/autofill/core/browser/form_structure.h"
#import "components/autofill/ios/form_util/form_activity_params.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/password_manager/ios/password_account_storage_notice_handler.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/autofill/bottom_sheet/autofill_bottom_sheet_java_script_feature.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/commands/autofill_bottom_sheet_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/web/public/js_messaging/script_message.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/navigation/navigation_context.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
  web_state->AddObserver(this);
}

// Public methods

void AutofillBottomSheetTabHelper::SetAutofillBottomSheetHandler(
    id<AutofillBottomSheetCommands> commands_handler) {
  commands_handler_ = commands_handler;
}

void AutofillBottomSheetTabHelper::OnFormMessageReceived(
    const web::ScriptMessage& message) {
  autofill::FormActivityParams params;
  if (!commands_handler_ || !password_account_storage_notice_handler_ ||
      !autofill::FormActivityParams::FromMessage(message, &params)) {
    return;
  }

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

  AttachListeners(renderer_ids, registered_password_renderer_ids_, frame_id,
                  /*must_be_empty = */ false);
}

void AutofillBottomSheetTabHelper::AttachPaymentsListeners(
    const std::vector<autofill::FormStructure*>& forms,
    const std::string& frame_id) {
  // Verify that the payments bottom sheet feature is enabled
  if (!base::FeatureList::IsEnabled(kIOSPaymentsBottomSheet)) {
    return;
  }

  std::vector<autofill::FieldRendererId> renderer_ids;
  for (const autofill::FormStructure* form : forms) {
    if (form->IsCompleteCreditCardForm()) {
      for (const auto& field : form->fields()) {
        if (IsPaymentsBottomSheetTriggeringField(
                field->Type().GetStorableType())) {
          renderer_ids.emplace_back(field->unique_renderer_id);
        }
      }
    }
  }

  if (!renderer_ids.empty()) {
    AttachListeners(renderer_ids, registered_payments_renderer_ids_, frame_id,
                    /*must_be_empty = */ true);
  }
}

void AutofillBottomSheetTabHelper::AttachListeners(
    const std::vector<autofill::FieldRendererId>& renderer_ids,
    std::set<autofill::FieldRendererId>& registered_renderer_ids,
    const std::string& frame_id,
    bool must_be_empty) {
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
        new_renderer_ids, frame, must_be_empty);

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
      registered_password_renderer_ids_, frame, /*must_be_empty = */ false,
      refocus);
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
      registered_payments_renderer_ids_, frame, /*must_be_empty = */ true,
      refocus);
}

void AutofillBottomSheetTabHelper::DetachPasswordListeners(web::WebFrame* frame,
                                                           bool refocus) {
  // Verify that the password bottom sheet feature is enabled.
  if (!base::FeatureList::IsEnabled(
          password_manager::features::kIOSPasswordBottomSheet)) {
    return;
  }

  AutofillBottomSheetJavaScriptFeature::GetInstance()->DetachListeners(
      registered_password_renderer_ids_, frame, /*must_be_empty = */ false,
      refocus);
}

void AutofillBottomSheetTabHelper::DetachPaymentsListeners(web::WebFrame* frame,
                                                           bool refocus) {
  // Verify that the payments bottom sheet feature is enabled
  if (!base::FeatureList::IsEnabled(kIOSPaymentsBottomSheet)) {
    return;
  }

  AutofillBottomSheetJavaScriptFeature::GetInstance()->DetachListeners(
      registered_payments_renderer_ids_, frame, /*must_be_empty = */ true,
      refocus);
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
