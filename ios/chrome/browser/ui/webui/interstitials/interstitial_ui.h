// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_WEBUI_INTERSTITIALS_INTERSTITIAL_UI_H_
#define IOS_CHROME_BROWSER_UI_WEBUI_INTERSTITIALS_INTERSTITIAL_UI_H_

#include <string>

#include "ios/web/public/webui/web_ui_ios_controller.h"

// The WebUI handler for chrome://interstitials.
class InterstitialUI : public web::WebUIIOSController {
 public:
  explicit InterstitialUI(web::WebUIIOS* web_ui, const std::string& host);
  ~InterstitialUI() override;
  InterstitialUI(InterstitialUI&& other) = default;
  InterstitialUI& operator=(InterstitialUI&& other) = default;
};

#endif  // IOS_CHROME_BROWSER_UI_WEBUI_INTERSTITIALS_INTERSTITIAL_UI_H_
