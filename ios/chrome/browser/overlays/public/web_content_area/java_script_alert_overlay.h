// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_WEB_CONTENT_AREA_JAVA_SCRIPT_ALERT_OVERLAY_H_
#define IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_WEB_CONTENT_AREA_JAVA_SCRIPT_ALERT_OVERLAY_H_

#include "ios/chrome/browser/overlays/public/overlay_user_data.h"
#include "ios/chrome/browser/overlays/public/web_content_area/java_script_dialog_source.h"

// Configuration object for OverlayRequests for JavaScript alert() calls.
class JavaScriptAlertOverlayRequestConfig
    : public OverlayUserData<JavaScriptAlertOverlayRequestConfig> {
 public:
  ~JavaScriptAlertOverlayRequestConfig() override;

  // The source of the alert.
  const JavaScriptDialogSource& source() const { return source_; }
  // The message to be displayed in the alert.
  const std::string& message() const { return message_; }

 private:
  OVERLAY_USER_DATA_SETUP(JavaScriptAlertOverlayRequestConfig);
  JavaScriptAlertOverlayRequestConfig(const JavaScriptDialogSource& source,
                                      const std::string& message);

  const JavaScriptDialogSource source_;
  const std::string message_;
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_WEB_CONTENT_AREA_JAVA_SCRIPT_ALERT_OVERLAY_H_
