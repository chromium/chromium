// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_METRICS_HISTOGRAMS_INTERNALS_UI_H_
#define CONTENT_BROWSER_METRICS_HISTOGRAMS_INTERNALS_UI_H_

#include "base/macros.h"
#include "content/public/browser/web_ui_controller.h"

namespace content {

// Handles serving the chrome://histograms HTML, JS, CSS as well as internal
// page requests.
class HistogramsInternalsUI : public WebUIController {
 public:
  explicit HistogramsInternalsUI(WebUI* web_ui);

 private:
  DISALLOW_COPY_AND_ASSIGN(HistogramsInternalsUI);
};

}  // namespace content

#endif  // CONTENT_BROWSER_METRICS_HISTOGRAMS_INTERNALS_UI_H_
