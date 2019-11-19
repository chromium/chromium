// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_WEBUI_PREFS_INTERNALS_UI_H_
#define IOS_CHROME_BROWSER_UI_WEBUI_PREFS_INTERNALS_UI_H_

#include "base/macros.h"
#include "ios/web/public/webui/web_ui_ios_controller.h"

namespace web {
class WebUIIOS;
}

// The WebUIController for chrome://prefs-internals. Renders the current user
// prefs.
class PrefsInternalsUI : public web::WebUIIOSController {
 public:
  explicit PrefsInternalsUI(web::WebUIIOS* web_ui);
  ~PrefsInternalsUI() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(PrefsInternalsUI);
};

#endif  // IOS_CHROME_BROWSER_UI_WEBUI_PREFS_INTERNALS_UI_H_
