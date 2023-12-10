// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/model/public/web_content_area/java_script_alert_dialog_overlay.h"

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
using java_script_dialog_overlay::ShouldAddBlockDialogsButton;

namespace {
// The index of the block button for JS alerts.
const size_t kAlertBlockButtonIndex = 1;

// Creates a JavaScriptAlertDialogResponse from a response created with an
// AlertResponse.
std::unique_ptr<OverlayResponse> CreateDialogResponse(
    std::unique_ptr<OverlayResponse> response) {
  AlertResponse* alert_response = response->GetInfo<AlertResponse>();
  if (!alert_response)
    return nullptr;

  JavaScriptAlertDialogResponse::Action action =
      JavaScriptAlertDialogResponse::Action::kConfirm;
  size_t button_index = alert_response->tapped_button_row_index();
  if (button_index == kAlertBlockButtonIndex) {
    action = JavaScriptAlertDialogResponse::Action::kBlockDialogs;
  }

  return OverlayResponse::CreateWithInfo<JavaScriptAlertDialogResponse>(action);
}

}  // namespace

#pragma mark - JavaScriptAlertDialogRequest

OVERLAY_USER_DATA_SETUP_IMPL(JavaScriptAlertDialogRequest);

JavaScriptAlertDialogRequest::JavaScriptAlertDialogRequest(
    web::WebState* web_state,
    const GURL& url,
    bool is_main_frame,
    NSString* message)
    : weak_web_state_(web_state->GetWeakPtr()),
      url_(url),
      is_main_frame_(is_main_frame),
      message_(message) {}

JavaScriptAlertDialogRequest::~JavaScriptAlertDialogRequest() = default;

void JavaScriptAlertDialogRequest::CreateAuxiliaryData(
    base::SupportsUserData* user_data) {
  NSString* alert_title = DialogTitle(is_main_frame_, message());
  NSString* alert_message = DialogMessage(is_main_frame_, message());

  std::vector<std::vector<ButtonConfig>> button_configs{
      {ButtonConfig(l10n_util::GetNSString(IDS_OK))}};
  if (ShouldAddBlockDialogsButton(web_state())) {
    button_configs.push_back(
        std::vector<ButtonConfig>{BlockDialogsButtonConfig()});
  }

  AlertRequest::CreateForUserData(user_data, alert_title, alert_message,
                                  kJavaScriptDialogAccessibilityIdentifier,
                                  /*text_field_configs=*/nil, button_configs,
                                  base::BindRepeating(&CreateDialogResponse));
}

#pragma mark - JavaScriptAlertDialogResponse

OVERLAY_USER_DATA_SETUP_IMPL(JavaScriptAlertDialogResponse);

JavaScriptAlertDialogResponse::JavaScriptAlertDialogResponse(Action action)
    : action_(action) {}

JavaScriptAlertDialogResponse::~JavaScriptAlertDialogResponse() = default;
