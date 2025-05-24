// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_PROFILE_INTERNALS_PROFILE_INTERNALS_HANDLER_H_
#define IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_PROFILE_INTERNALS_PROFILE_INTERNALS_HANDLER_H_

#include "base/values.h"
#include "ios/web/public/webui/web_ui_ios_message_handler.h"

// The handler for JavaScript messages for chrome://profile-internals.
class ProfileInternalsHandler : public web::WebUIIOSMessageHandler {
 public:
  ProfileInternalsHandler();
  ProfileInternalsHandler(const ProfileInternalsHandler&) = delete;
  ProfileInternalsHandler& operator=(const ProfileInternalsHandler&) = delete;
  ~ProfileInternalsHandler() override;

  // web::WebUIIOSMessageHandler.
  void RegisterMessages() override;

 private:
  // Handler for "getProfilesList" message from JS. Builds the list of profiles
  // and their attributes and sends it back to JS.
  void HandleGetProfilesList(const base::Value::List& args);
};

#endif  // IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_PROFILE_INTERNALS_PROFILE_INTERNALS_HANDLER_H_
