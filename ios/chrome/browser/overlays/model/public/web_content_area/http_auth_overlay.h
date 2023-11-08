// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_WEB_CONTENT_AREA_HTTP_AUTH_OVERLAY_H_
#define IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_WEB_CONTENT_AREA_HTTP_AUTH_OVERLAY_H_

#include <string>

#include "ios/chrome/browser/overlays/model/public/overlay_request_config.h"
#include "ios/chrome/browser/overlays/model/public/overlay_response_info.h"
#include "url/gurl.h"

// Name of UMA User Action logged when user taps Sign In button.
extern const char kHttpAuthSignInTappedActionName[];
// Name of UMA User Action logged when user taps Cancel button.
extern const char kHttpAuthCancelTappedActionName[];

// Configuration object for OverlayRequests for HTTP authentication challenges.
class HTTPAuthOverlayRequestConfig
    : public OverlayRequestConfig<HTTPAuthOverlayRequestConfig> {
 public:
  ~HTTPAuthOverlayRequestConfig() override;

  // The URL of the page requesting authentication.
  const GURL& url() const { return url_; }
  // The message to be displayed in the auth dialog.
  const std::string& message() const { return message_; }
  // The default text to use for the username field.
  const std::string& default_username() const { return default_username_; }

 private:
  OVERLAY_USER_DATA_SETUP(HTTPAuthOverlayRequestConfig);
  HTTPAuthOverlayRequestConfig(const GURL& url,
                               const std::string& message,
                               const std::string& default_username);

  // OverlayUserData:
  void CreateAuxiliaryData(base::SupportsUserData* user_data) override;

  const GURL url_;
  const std::string message_;
  const std::string default_username_;
};

// User interaction info for OverlayResponses for HTTP authentication dialogs.
class HTTPAuthOverlayResponseInfo
    : public OverlayResponseInfo<HTTPAuthOverlayResponseInfo> {
 public:
  ~HTTPAuthOverlayResponseInfo() override;

  // The username entered into the HTTP authentication dialog.
  const std::string& username() const { return username_; }
  // The password entered into the HTTP authentication dialog.
  const std::string& password() const { return password_; }

 private:
  OVERLAY_USER_DATA_SETUP(HTTPAuthOverlayResponseInfo);
  HTTPAuthOverlayResponseInfo(const std::string& username,
                              const std::string& password);

  const std::string username_;
  const std::string password_;
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_WEB_CONTENT_AREA_HTTP_AUTH_OVERLAY_H_
