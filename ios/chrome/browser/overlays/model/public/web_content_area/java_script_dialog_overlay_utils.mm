// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/model/public/web_content_area/java_script_dialog_overlay_utils.h"

#import "base/strings/sys_string_conversions.h"
#import "components/javascript_dialogs/core/dialog_util.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/dialogs/ui_bundled/java_script_dialog_blocking_state.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/origin.h"

using alert_overlays::ButtonConfig;

namespace java_script_dialog_overlay {

bool ShouldAddBlockDialogsButton(web::WebState* web_state) {
  if (!web_state) {
    return false;
  }
  JavaScriptDialogBlockingState* blocking_state =
      JavaScriptDialogBlockingState::FromWebState(web_state);
  return blocking_state && blocking_state->show_blocking_option();
}

alert_overlays::ButtonConfig BlockDialogsButtonConfig() {
  NSString* action_title =
      l10n_util::GetNSString(IDS_IOS_JAVA_SCRIPT_DIALOG_BLOCKING_BUTTON_TEXT);
  return ButtonConfig(action_title, UIAlertActionStyleDestructive);
}

NSString* DialogTitle(GURL main_frame_url, url::Origin alerting_frame_origin) {
  url::Origin main_frame_origin = url::Origin::Create(main_frame_url);
  std::u16string title = javascript_dialogs::util::DialogTitle(
      main_frame_origin, alerting_frame_origin);
  return base::SysUTF16ToNSString(title);
}
}  // namespace java_script_dialog_overlay
