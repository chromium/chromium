// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_DOWNLOAD_INTERNALS_UI_H_
#define IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_DOWNLOAD_INTERNALS_UI_H_

#include "ios/web/public/webui/web_ui_ios_controller.h"

// The WebUI for chrome://download-internals on iOS.
class DownloadInternalsUI : public web::WebUIIOSController {
 public:
  DownloadInternalsUI(web::WebUIIOS* web_ui, const std::string& host);

  DownloadInternalsUI(const DownloadInternalsUI&) = delete;
  void operator=(const DownloadInternalsUI&) = delete;
  ~DownloadInternalsUI() override;
};

#endif  // IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_DOWNLOAD_INTERNALS_UI_H_
