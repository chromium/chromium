// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_METRICS_INTERNALS_METRICS_INTERNALS_UI_H_
#define IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_METRICS_INTERNALS_METRICS_INTERNALS_UI_H_

#include <string>

#include "ios/web/public/webui/web_ui_ios_controller.h"

// LINT.IfChange(metrics_internals_ui)

// Controller for the chrome://metrics-internals page.
class MetricsInternalsUI : public web::WebUIIOSController {
 public:
  explicit MetricsInternalsUI(web::WebUIIOS* web_ui, const std::string& host);

  MetricsInternalsUI(const MetricsInternalsUI&) = delete;
  MetricsInternalsUI& operator=(const MetricsInternalsUI&) = delete;

  ~MetricsInternalsUI() override;
};

// LINT.ThenChange(//chrome/browser/ui/webui/metrics_internals/metrics_internals_ui.h)

#endif  // IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_METRICS_INTERNALS_METRICS_INTERNALS_UI_H_
