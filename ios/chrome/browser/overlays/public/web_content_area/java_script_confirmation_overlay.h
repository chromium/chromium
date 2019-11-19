// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_WEB_CONTENT_AREA_JAVA_SCRIPT_CONFIRMATION_OVERLAY_H_
#define IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_WEB_CONTENT_AREA_JAVA_SCRIPT_CONFIRMATION_OVERLAY_H_

#include "ios/chrome/browser/overlays/public/overlay_user_data.h"
#include "ios/chrome/browser/overlays/public/web_content_area/java_script_dialog_source.h"

// Configuration object for OverlayRequests for JavaScript confirm() calls.
class JavaScriptConfirmationOverlayRequestConfig
    : public OverlayUserData<JavaScriptConfirmationOverlayRequestConfig> {
 public:
  ~JavaScriptConfirmationOverlayRequestConfig() override;

  // The source of the confirmation dialog.
  const JavaScriptDialogSource& source() const { return source_; }
  // The message to be displayed in the confirmation dialog.
  const std::string& message() const { return message_; }

 private:
  OVERLAY_USER_DATA_SETUP(JavaScriptConfirmationOverlayRequestConfig);
  JavaScriptConfirmationOverlayRequestConfig(
      const JavaScriptDialogSource& source,
      const std::string& message);

  const JavaScriptDialogSource source_;
  const std::string message_;
};

// User interaction info for OverlayResponses for JavaScript confirm() calls.
class JavaScriptConfirmationOverlayResponseInfo
    : public OverlayUserData<JavaScriptConfirmationOverlayResponseInfo> {
 public:
  ~JavaScriptConfirmationOverlayResponseInfo() override;

  // Whether the user tapped the OK button on the dialog.
  bool dialog_confirmed() const { return dialog_confirmed_; }

 private:
  OVERLAY_USER_DATA_SETUP(JavaScriptConfirmationOverlayResponseInfo);
  JavaScriptConfirmationOverlayResponseInfo(bool dialog_confirmed);

  const bool dialog_confirmed_ = false;
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_WEB_CONTENT_AREA_JAVA_SCRIPT_CONFIRMATION_OVERLAY_H_
