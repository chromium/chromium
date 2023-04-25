// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/bottom_sheet/bottom_sheet_tab_helper.h"

#import "base/feature_list.h"
#import "components/autofill/ios/form_util/form_activity_params.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/password_manager/ios/password_account_storage_notice_handler.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/autofill/bottom_sheet/bottom_sheet_java_script_feature.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/password_bottom_sheet_commands.h"
#import "ios/web/public/js_messaging/script_message.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The maximum number of times the password bottom sheet can be dismissed before
// it gets disabled.
constexpr int kIosPasswordBottomSheetMaxDismissCount = 3;
}  // namespace

BottomSheetTabHelper::~BottomSheetTabHelper() = default;

BottomSheetTabHelper::BottomSheetTabHelper(
    web::WebState* web_state,
    id<PasswordsAccountStorageNoticeHandler>
        password_account_storage_notice_handler)
    : password_account_storage_notice_handler_(
          password_account_storage_notice_handler),
      web_state_(web_state) {}

// Public methods

void BottomSheetTabHelper::SetPasswordBottomSheetHandler(
    id<PasswordBottomSheetCommands> password_bottom_sheet_commands_handler) {
  password_bottom_sheet_commands_handler_ =
      password_bottom_sheet_commands_handler;
}

void BottomSheetTabHelper::OnFormMessageReceived(
    const web::ScriptMessage& message) {
  autofill::FormActivityParams params;
  if (!password_bottom_sheet_commands_handler_ ||
      !password_account_storage_notice_handler_ ||
      !autofill::FormActivityParams::FromMessage(message, &params)) {
    return;
  }

  if (![password_account_storage_notice_handler_
          shouldShowAccountStorageNotice]) {
    [password_bottom_sheet_commands_handler_ showPasswordBottomSheet:params];
    return;
  }

  __weak id<PasswordBottomSheetCommands>
      weak_password_bottom_sheet_commands_handler =
          password_bottom_sheet_commands_handler_;
  [password_account_storage_notice_handler_ showAccountStorageNotice:^{
    [weak_password_bottom_sheet_commands_handler
        showPasswordBottomSheet:params];
  }];
}

void BottomSheetTabHelper::AttachListeners(
    const std::vector<autofill::FieldRendererId>& renderer_ids,
    web::WebFrame* frame) {
  // Verify that the password bottom sheet feature is enabled and that it hasn't
  // been dismissed too many times.
  if (!base::FeatureList::IsEnabled(
          password_manager::features::kIOSPasswordBottomSheet) ||
      HasReachedDismissLimit()) {
    return;
  }

  // Enable the password bottom sheet.
  BottomSheetJavaScriptFeature::GetInstance()->AttachListeners(renderer_ids,
                                                               frame);
}

void BottomSheetTabHelper::DetachListenersAndRefocus(web::WebFrame* frame) {
  BottomSheetJavaScriptFeature::GetInstance()->DetachListenersAndRefocus(frame);
}

// Private methods

bool BottomSheetTabHelper::HasReachedDismissLimit() {
  PrefService* const pref_service =
      ChromeBrowserState ::FromBrowserState(web_state_->GetBrowserState())
          ->GetPrefs();
  return pref_service->GetInteger(prefs::kIosPasswordBottomSheetDismissCount) >
         kIosPasswordBottomSheetMaxDismissCount;
}

WEB_STATE_USER_DATA_KEY_IMPL(BottomSheetTabHelper)
