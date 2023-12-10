// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/model/public/web_content_area/java_script_confirm_dialog_overlay.h"

#import "base/functional/bind.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/overlays/model/public/overlay_response.h"
#import "ios/chrome/browser/overlays/model/public/web_content_area/alert_constants.h"
#import "ios/chrome/browser/overlays/model/public/web_content_area/alert_overlay.h"
#import "ios/chrome/browser/overlays/model/public/web_content_area/java_script_dialog_overlay_utils.h"
#import "ui/base/l10n/l10n_util.h"

using alert_overlays::AlertRequest;
using alert_overlays::AlertResponse;
using alert_overlays::ButtonConfig;
using java_script_dialog_overlay::BlockDialogsButtonConfig;
using java_script_dialog_overlay::DialogMessage;
using java_script_dialog_overlay::DialogTitle;
using java_script_dialog_overlay::kButtonIndexOk;
using java_script_dialog_overlay::ShouldAddBlockDialogsButton;

namespace {

// The index of the block button for JS confirm dialogs.
const size_t kConfirmBlockButtonIndex = 2;

// Creates a JavaScriptConfirmDialogResponse from a response created with an
// AlertResponse.
std::unique_ptr<OverlayResponse> CreateDialogResponse(
    std::unique_ptr<OverlayResponse> response) {
  AlertResponse* alert_response = response->GetInfo<AlertResponse>();
  if (!alert_response)
    return nullptr;

  JavaScriptConfirmDialogResponse::Action action =
      JavaScriptConfirmDialogResponse::Action::kCancel;
  size_t button_index = alert_response->tapped_button_row_index();
  if (button_index == kButtonIndexOk) {
    action = JavaScriptConfirmDialogResponse::Action::kConfirm;
  } else if (button_index == kConfirmBlockButtonIndex) {
    action = JavaScriptConfirmDialogResponse::Action::kBlockDialogs;
  }

  return OverlayResponse::CreateWithInfo<JavaScriptConfirmDialogResponse>(
      action);
}

}  // namespace

#pragma mark - JavaScriptConfirmDialogRequest

OVERLAY_USER_DATA_SETUP_IMPL(JavaScriptConfirmDialogRequest);

JavaScriptConfirmDialogRequest::JavaScriptConfirmDialogRequest(
    web::WebState* web_state,
    const GURL& url,
    bool is_main_frame,
    NSString* message)
    : weak_web_state_(web_state->GetWeakPtr()),
      url_(url),
      is_main_frame_(is_main_frame),
      message_(message) {}

JavaScriptConfirmDialogRequest::~JavaScriptConfirmDialogRequest() = default;

void JavaScriptConfirmDialogRequest::CreateAuxiliaryData(
    base::SupportsUserData* user_data) {
  NSString* alert_title = DialogTitle(is_main_frame_, message());
  NSString* alert_message = DialogMessage(is_main_frame_, message());

  std::vector<std::vector<ButtonConfig>> button_configs{
      {ButtonConfig(l10n_util::GetNSString(IDS_OK))},
      {ButtonConfig(l10n_util::GetNSString(IDS_CANCEL),
                    UIAlertActionStyleCancel)}};
  if (ShouldAddBlockDialogsButton(web_state())) {
    button_configs.push_back(
        std::vector<ButtonConfig>{BlockDialogsButtonConfig()});
  }

  AlertRequest::CreateForUserData(user_data, alert_title, alert_message,
                                  kJavaScriptDialogAccessibilityIdentifier,
                                  /*text_field_configs=*/nil, button_configs,
                                  base::BindRepeating(&CreateDialogResponse));
}

#pragma mark - JavaScriptConfirmDialogResponse

OVERLAY_USER_DATA_SETUP_IMPL(JavaScriptConfirmDialogResponse);

JavaScriptConfirmDialogResponse::JavaScriptConfirmDialogResponse(Action action)
    : action_(action) {}

JavaScriptConfirmDialogResponse::~JavaScriptConfirmDialogResponse() = default;
