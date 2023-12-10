// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_WEB_CONTENT_AREA_JAVA_SCRIPT_DIALOG_OVERLAY_UTILS_H_
#define IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_WEB_CONTENT_AREA_JAVA_SCRIPT_DIALOG_OVERLAY_UTILS_H_

#import "ios/chrome/browser/overlays/model/public/web_content_area/alert_overlay.h"

namespace web {
class WebState;
}  // namespace web

namespace java_script_dialog_overlay {

// The index of the OK button in the alert button array.
constexpr size_t kButtonIndexOk = 0;

// Whether the dialog blocking button should be added for an overlay from
// `web_state`.
bool ShouldAddBlockDialogsButton(web::WebState* web_state);

// Returns the button configuration for the button to block further JavaScript
// dialogs.
alert_overlays::ButtonConfig BlockDialogsButtonConfig();

// Returns the dialog title for a JavaScript dialog with `config_message`.
// `is_main_frame` is true iff the dialog is being presented by the main
// frame.
NSString* DialogTitle(bool is_main_frame, NSString* config_message);

// Returns the dialog message for a JavaScript dialog with `config_message`.
// `is_main_frame` is true iff the dialog is being presented by the main frame.
NSString* DialogMessage(bool is_main_frame, NSString* config_message);

}  // namespace java_script_dialog_overlay

#endif  // IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_WEB_CONTENT_AREA_JAVA_SCRIPT_DIALOG_OVERLAY_UTILS_H_
