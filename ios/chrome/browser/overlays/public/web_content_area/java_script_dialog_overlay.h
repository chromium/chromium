// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_WEB_CONTENT_AREA_JAVA_SCRIPT_DIALOG_OVERLAY_H_
#define IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_WEB_CONTENT_AREA_JAVA_SCRIPT_DIALOG_OVERLAY_H_

#include "base/callback.h"
#include "ios/chrome/browser/overlays/public/overlay_request_config.h"
#include "ios/chrome/browser/overlays/public/overlay_response_info.h"
#include "ios/web/public/ui/java_script_dialog_type.h"
#include "ios/web/public/web_state.h"
#include "url/gurl.h"

namespace java_script_dialog_overlays {

// Configuration object for OverlayRequests for JavaScript dialogs.
class JavaScriptDialogRequest
    : public OverlayRequestConfig<JavaScriptDialogRequest> {
 public:
  ~JavaScriptDialogRequest() override;

  web::JavaScriptDialogType type() const { return type_; }
  web::WebState* web_state() const { return web_state_getter_.Run(); }
  const GURL& url() const { return url_; }
  bool is_main_frame() const { return is_main_frame_; }
  NSString* message() const { return message_; }

  // The default text shown in the text field.  Only used for prompts.
  NSString* default_text_field_value() const {
    return default_text_field_value_;
  }

 private:
  OVERLAY_USER_DATA_SETUP(JavaScriptDialogRequest);
  JavaScriptDialogRequest(web::JavaScriptDialogType type,
                          web::WebState* web_state,
                          const GURL& url,
                          bool is_main_frame,
                          NSString* message,
                          NSString* default_text_field_value);

  // OverlayUserData:
  void CreateAuxiliaryData(base::SupportsUserData* user_data) override;

  const web::JavaScriptDialogType type_;
  web::WebState::Getter web_state_getter_;
  const GURL url_;
  bool is_main_frame_;
  NSString* message_ = nil;
  NSString* default_text_field_value_ = nil;
};

// Response type used for JavaScript dialogs.
class JavaScriptDialogResponse
    : public OverlayResponseInfo<JavaScriptDialogResponse> {
 public:
  ~JavaScriptDialogResponse() override;

  // The action selected by the user.
  enum class Action : short {
    kConfirm,      // Used when the user taps the OK button on a dialog.
    kCancel,       // Used when the user taps the Cancel button on a dialog.
    kBlockDialogs  // Used when the user taps the blocking option on a dialog,
                   // indicating that subsequent dialogs from the page should be
                   // blocked.
  };
  Action action() const { return action_; }
  // The user input.
  NSString* user_input() const { return user_input_; }

 private:
  OVERLAY_USER_DATA_SETUP(JavaScriptDialogResponse);
  JavaScriptDialogResponse(Action action, NSString* user_input);

  Action action_;
  NSString* user_input_ = nil;
};

}  // java_script_dialog_overlays

#endif  // IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_WEB_CONTENT_AREA_JAVA_SCRIPT_DIALOG_OVERLAY_H_
