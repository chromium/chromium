// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_METRICS_HISTOGRAMS_INTERNALS_UI_H_
#define CONTENT_BROWSER_METRICS_HISTOGRAMS_INTERNALS_UI_H_

#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"

namespace content {

class HistogramsInternalsUI;

// Config for chrome://histograms.
class HistogramsInternalsUIConfig
    : public DefaultWebUIConfig<HistogramsInternalsUI> {
 public:
  HistogramsInternalsUIConfig()
      : DefaultWebUIConfig(kChromeUIScheme, kChromeUIHistogramHost) {}
};

// Handles serving the chrome://histograms HTML, JS, CSS as well as internal
// page requests.
class HistogramsInternalsUI : public WebUIController {
 public:
  explicit HistogramsInternalsUI(WebUI* web_ui);

  HistogramsInternalsUI(const HistogramsInternalsUI&) = delete;
  HistogramsInternalsUI& operator=(const HistogramsInternalsUI&) = delete;
};

}  // namespace content

#endif  // CONTENT_BROWSER_METRICS_HISTOGRAMS_INTERNALS_UI_H_
