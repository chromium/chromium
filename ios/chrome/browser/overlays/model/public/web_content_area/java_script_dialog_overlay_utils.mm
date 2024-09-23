// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/model/public/web_content_area/java_script_dialog_overlay_utils.h"

#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/dialogs/ui_bundled/java_script_dialog_blocking_state.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util.h"

using alert_overlays::ButtonConfig;

namespace java_script_dialog_overlay {

bool ShouldAddBlockDialogsButton(web::WebState* web_state) {
  if (!web_state)
    return false;
  JavaScriptDialogBlockingState* blocking_state =
      JavaScriptDialogBlockingState::FromWebState(web_state);
  return blocking_state && blocking_state->show_blocking_option();
}

alert_overlays::ButtonConfig BlockDialogsButtonConfig() {
  NSString* action_title =
      l10n_util::GetNSString(IDS_IOS_JAVA_SCRIPT_DIALOG_BLOCKING_BUTTON_TEXT);
  return ButtonConfig(action_title, UIAlertActionStyleDestructive);
}

NSString* DialogTitle(bool is_main_frame, NSString* config_message) {
  // If the dialog is from the main frame, use the message text as the alert's
  // title.
  if (is_main_frame) {
    return config_message;
  }
  // Otherwise, use a title indicating that the dialog is from an iframe.
  return l10n_util::GetNSString(
      IDS_JAVASCRIPT_MESSAGEBOX_TITLE_NONSTANDARD_URL_IFRAME);
}

NSString* DialogMessage(bool is_main_frame, NSString* config_message) {
  // If the dialog is from the main frame, the message is the alert's title and
  // no dialog message is necessary.
  if (is_main_frame) {
    return nil;
  }
  // If the dialog is from an iframe, a message indicating such is the title, so
  // the message from the JavaScript is returned here.
  return config_message;
}

}  // namespace java_script_dialog_overlay
