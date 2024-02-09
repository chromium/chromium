// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_TERMS_UI_H_
#define IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_TERMS_UI_H_

#include <string>

#include "ios/web/public/webui/web_ui_ios_controller.h"

// The WebUI handler for chrome://terms.
class TermsUI : public web::WebUIIOSController {
 public:
  TermsUI(web::WebUIIOS* web_ui, const std::string& name);

  TermsUI(const TermsUI&) = delete;
  TermsUI& operator=(const TermsUI&) = delete;

  ~TermsUI() override;
};

#endif  // IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_TERMS_UI_H_
