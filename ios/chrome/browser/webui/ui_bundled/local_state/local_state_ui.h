// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_LOCAL_STATE_LOCAL_STATE_UI_H_
#define IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_LOCAL_STATE_LOCAL_STATE_UI_H_

#include <string>

#import "ios/web/public/webui/web_ui_ios_controller.h"

// Controller for chrome://local-state/ page.
class LocalStateUI : public web::WebUIIOSController {
 public:
  explicit LocalStateUI(web::WebUIIOS* web_ui, const std::string& host);

  LocalStateUI(const LocalStateUI&) = delete;
  LocalStateUI& operator=(const LocalStateUI&) = delete;

  ~LocalStateUI() override;
};

#endif  // IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_LOCAL_STATE_LOCAL_STATE_UI_H_
