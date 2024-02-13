// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_VERSION_HANDLER_H_
#define IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_VERSION_HANDLER_H_

#include "base/values.h"
#include "ios/web/public/webui/web_ui_ios_message_handler.h"

// Handler class for Version page operations.
class VersionHandler : public web::WebUIIOSMessageHandler {
 public:
  VersionHandler();

  VersionHandler(const VersionHandler&) = delete;
  VersionHandler& operator=(const VersionHandler&) = delete;

  ~VersionHandler() override;

  // content::WebUIMessageHandler implementation.
  void RegisterMessages() override;

  // Callback for the "requestVariationInfo" message. This responds immediately
  // with the list of variations.
  void HandleRequestVariationInfo(const base::Value::List& args);
};

#endif  // IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_VERSION_HANDLER_H_
