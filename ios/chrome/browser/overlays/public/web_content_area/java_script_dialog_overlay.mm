// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/public/web_content_area/java_script_dialog_overlay.h"

#include "base/bind.h"
#include "base/logging.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/overlays/public/overlay_response.h"
#import "ios/chrome/browser/overlays/public/web_content_area/alert_overlay.h"
#import "ios/chrome/browser/ui/dialogs/dialog_constants.h"
#import "ios/chrome/browser/ui/dialogs/java_script_dialog_blocking_state.h"
#import "ios/chrome/browser/ui/elements/text_field_configuration.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using alert_overlays::AlertRequest;
using alert_overlays::AlertResponse;
using alert_overlays::ButtonConfig;

namespace java_script_dialog_overlays {
namespace {
// The index of the OK button in the alert button array.
const size_t kButtonIndexOk = 0;

// Whether a cancel button should be added for an overlay with |type|.
bool DialogsWithTypeUseCancelButtons(web::JavaScriptDialogType type) {
  return type != web::JAVASCRIPT_DIALOG_TYPE_ALERT;
}

// Whether the dialog blocking button should be added for an overlay from
// |source|.
bool ShouldAddBlockDialogsButton(web::WebState* web_state) {
  if (!web_state)
    return false;
  JavaScriptDialogBlockingState* blocking_state =
      JavaScriptDialogBlockingState::FromWebState(web_state);
  return blocking_state && blocking_state->show_blocking_option();
}

// The index of the dialog blocking button for a dialog with |type|.
size_t GetBlockingOptionIndex(web::JavaScriptDialogType type) {
  return DialogsWithTypeUseCancelButtons(type) ? 2 : 1;
}

// Creates an JavaScript dialog response for a dialog with |type| from a
// response created with an AlertResponse.
std::unique_ptr<OverlayResponse> CreateJavaScriptDialogResponse(
    web::JavaScriptDialogType type,
    std::unique_ptr<OverlayResponse> response) {
  AlertResponse* alert_response = response->GetInfo<AlertResponse>();
  if (!alert_response)
    return nullptr;

  JavaScriptDialogResponse::Action action =
      JavaScriptDialogResponse::Action::kCancel;
  NSString* user_input = nil;
  size_t button_index = alert_response->tapped_button_index();
  if (button_index == kButtonIndexOk) {
    action = JavaScriptDialogResponse::Action::kConfirm;
    user_input = [alert_response->text_field_values() firstObject];
  } else if (button_index == GetBlockingOptionIndex(type)) {
    action = JavaScriptDialogResponse::Action::kBlockDialogs;
  }

  return OverlayResponse::CreateWithInfo<JavaScriptDialogResponse>(action,
                                                                   user_input);
}
}  // namespace

#pragma mark - JavaScriptDialogRequest

OVERLAY_USER_DATA_SETUP_IMPL(JavaScriptDialogRequest);

JavaScriptDialogRequest::JavaScriptDialogRequest(
    web::JavaScriptDialogType type,
    web::WebState* web_state,
    const GURL& url,
    bool is_main_frame,
    NSString* message,
    NSString* default_text_field_value)
    : type_(type),
      weak_web_state_(web_state->GetWeakPtr()),
      url_(url),
      is_main_frame_(is_main_frame),
      message_(message),
      default_text_field_value_(default_text_field_value) {}

JavaScriptDialogRequest::~JavaScriptDialogRequest() = default;

void JavaScriptDialogRequest::CreateAuxiliaryData(
    base::SupportsUserData* user_data) {
  NSString* accessibility_identifier = kJavaScriptDialogAccessibilityIdentifier;

  // If the dialog is from the main frame, use the message text as the alert's
  // title.  Otherwise, use a title indicating that the dialog is from an iframe
  // and use the message text as the alert's message.
  NSString* alert_title = nil;
  NSString* alert_message = nil;
  if (is_main_frame_) {
    alert_title = message();
  } else {
    alert_title = l10n_util::GetNSString(
        IDS_JAVASCRIPT_MESSAGEBOX_TITLE_NONSTANDARD_URL_IFRAME);
    alert_message = message();
  }

  // Add a text field with the default value for prompts.
  NSArray<TextFieldConfiguration*>* text_field_configs = nil;
  if (type() == web::JAVASCRIPT_DIALOG_TYPE_PROMPT) {
    NSString* text_field_identifier =
        kJavaScriptDialogTextFieldAccessibilityIdentifier;
    text_field_configs = @[ [[TextFieldConfiguration alloc]
                   initWithText:default_text_field_value()
                    placeholder:nil
        accessibilityIdentifier:text_field_identifier
         autocapitalizationType:UITextAutocapitalizationTypeSentences
                secureTextEntry:NO] ];
  }

  // Add the buttons.
  std::vector<ButtonConfig> button_configs{
      ButtonConfig(l10n_util::GetNSString(IDS_OK))};
  if (DialogsWithTypeUseCancelButtons(type())) {
    button_configs.push_back(ButtonConfig(l10n_util::GetNSString(IDS_CANCEL),
                                          UIAlertActionStyleCancel));
  }
  if (ShouldAddBlockDialogsButton(web_state())) {
    NSString* action_title =
        l10n_util::GetNSString(IDS_IOS_JAVA_SCRIPT_DIALOG_BLOCKING_BUTTON_TEXT);
    button_configs.push_back(
        ButtonConfig(action_title, UIAlertActionStyleDestructive));
  }

  // Create the alert config.
  AlertRequest::CreateForUserData(
      user_data, alert_title, alert_message, accessibility_identifier,
      text_field_configs, button_configs,
      base::BindRepeating(&CreateJavaScriptDialogResponse, type()));
}

#pragma mark - JavaScriptDialogResponse

OVERLAY_USER_DATA_SETUP_IMPL(JavaScriptDialogResponse);

JavaScriptDialogResponse::JavaScriptDialogResponse(Action action,
                                                   NSString* user_input)
    : action_(action), user_input_([user_input copy]) {}

JavaScriptDialogResponse::~JavaScriptDialogResponse() = default;

}  // java_script_dialog_overlays
