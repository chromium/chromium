// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/model/public/web_content_area/java_script_prompt_dialog_overlay.h"

#import "base/functional/bind.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/overlays/model/public/overlay_response.h"
#import "ios/chrome/browser/overlays/model/public/web_content_area/alert_constants.h"
#import "ios/chrome/browser/overlays/model/public/web_content_area/alert_overlay.h"
#import "ios/chrome/browser/overlays/model/public/web_content_area/java_script_dialog_overlay_utils.h"
#import "ios/chrome/browser/shared/ui/elements/text_field_configuration.h"
#import "ios/chrome/grit/ios_strings.h"
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

// The index of the block button for JS prompts.
const size_t kPromptBlockButtonIndex = 2;

// Creates a JavaScript prompt dialog response from a response created with an
// AlertResponse.
std::unique_ptr<OverlayResponse> CreateDialogResponse(
    std::unique_ptr<OverlayResponse> response) {
  AlertResponse* alert_response = response->GetInfo<AlertResponse>();
  if (!alert_response)
    return nullptr;

  JavaScriptPromptDialogResponse::Action action =
      JavaScriptPromptDialogResponse::Action::kCancel;
  NSString* user_input = nil;
  size_t button_index = alert_response->tapped_button_row_index();
  if (button_index == kButtonIndexOk) {
    action = JavaScriptPromptDialogResponse::Action::kConfirm;
    user_input = [alert_response->text_field_values() firstObject];
  } else if (button_index == kPromptBlockButtonIndex) {
    action = JavaScriptPromptDialogResponse::Action::kBlockDialogs;
  }

  return OverlayResponse::CreateWithInfo<JavaScriptPromptDialogResponse>(
      action, user_input);
}

}  // namespace

#pragma mark - JavaScriptPromptDialogRequest

OVERLAY_USER_DATA_SETUP_IMPL(JavaScriptPromptDialogRequest);

JavaScriptPromptDialogRequest::JavaScriptPromptDialogRequest(
    web::WebState* web_state,
    const GURL& url,
    bool is_main_frame,
    NSString* message,
    NSString* default_text_field_value)
    : weak_web_state_(web_state->GetWeakPtr()),
      url_(url),
      is_main_frame_(is_main_frame),
      message_(message),
      default_text_field_value_(default_text_field_value) {}

JavaScriptPromptDialogRequest::~JavaScriptPromptDialogRequest() = default;

void JavaScriptPromptDialogRequest::CreateAuxiliaryData(
    base::SupportsUserData* user_data) {
  NSString* alert_title = DialogTitle(is_main_frame_, message());
  NSString* alert_message = DialogMessage(is_main_frame_, message());

  // Add a text field with the default value.
  NSString* text_field_identifier =
      kJavaScriptDialogTextFieldAccessibilityIdentifier;
  NSArray<TextFieldConfiguration*>* text_field_configs =
      @[ [[TextFieldConfiguration alloc]
                     initWithText:default_text_field_value()
                      placeholder:nil
          accessibilityIdentifier:text_field_identifier
           autocapitalizationType:UITextAutocapitalizationTypeSentences
                  secureTextEntry:NO] ];
  // Add the buttons.
  std::vector<std::vector<ButtonConfig>> button_configs{
      {ButtonConfig(l10n_util::GetNSString(IDS_OK))},
      {ButtonConfig(l10n_util::GetNSString(IDS_CANCEL),
                    UIAlertActionStyleCancel)}};
  if (ShouldAddBlockDialogsButton(web_state())) {
    button_configs.push_back(
        std::vector<ButtonConfig>{BlockDialogsButtonConfig()});
  }

  // Create the alert config.
  AlertRequest::CreateForUserData(user_data, alert_title, alert_message,
                                  kJavaScriptDialogAccessibilityIdentifier,
                                  text_field_configs, button_configs,
                                  base::BindRepeating(&CreateDialogResponse));
}

#pragma mark - JavaScriptPromptDialogResponse

OVERLAY_USER_DATA_SETUP_IMPL(JavaScriptPromptDialogResponse);

JavaScriptPromptDialogResponse::JavaScriptPromptDialogResponse(
    Action action,
    NSString* user_input)
    : action_(action), user_input_([user_input copy]) {}

JavaScriptPromptDialogResponse::~JavaScriptPromptDialogResponse() = default;
