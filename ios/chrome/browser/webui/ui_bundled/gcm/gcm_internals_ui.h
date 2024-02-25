// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_GCM_GCM_INTERNALS_UI_H_
#define IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_GCM_GCM_INTERNALS_UI_H_

#include <string>

#include "ios/web/public/webui/web_ui_ios_controller.h"

namespace web {
class WebUIIOS;
}

// The WebUIIOS for chrome://gcm-internals.
class GCMInternalsUI : public web::WebUIIOSController {
 public:
  explicit GCMInternalsUI(web::WebUIIOS* web_ui, const std::string& host);

  GCMInternalsUI(const GCMInternalsUI&) = delete;
  GCMInternalsUI& operator=(const GCMInternalsUI&) = delete;

  ~GCMInternalsUI() override;
};

#endif  // IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_GCM_GCM_INTERNALS_UI_H_
