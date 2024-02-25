// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_CRASHES_UI_H_
#define IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_CRASHES_UI_H_

#include <string>

#include "ios/web/public/webui/web_ui_ios_controller.h"

namespace web {
class WebUIIOS;
}

class CrashesUI : public web::WebUIIOSController {
 public:
  explicit CrashesUI(web::WebUIIOS* web_ui, const std::string& host);

  CrashesUI(const CrashesUI&) = delete;
  CrashesUI& operator=(const CrashesUI&) = delete;
};

#endif  // IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_CRASHES_UI_H_
