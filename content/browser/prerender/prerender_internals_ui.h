// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRERENDER_PRERENDER_INTERNALS_UI_H_
#define CONTENT_BROWSER_PRERENDER_PRERENDER_INTERNALS_UI_H_

#include "content/public/browser/web_ui_controller.h"

namespace content {

// The WebUI for chrome://prerender-internals.
class PrerenderInternalsUI : public WebUIController {
 public:
  explicit PrerenderInternalsUI(WebUI* web_ui);
  PrerenderInternalsUI(const PrerenderInternalsUI&) = delete;
  PrerenderInternalsUI& operator=(const PrerenderInternalsUI&) = delete;
  ~PrerenderInternalsUI() override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRERENDER_PRERENDER_INTERNALS_UI_H_
