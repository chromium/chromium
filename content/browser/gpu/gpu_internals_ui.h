// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GPU_GPU_INTERNALS_UI_H_
#define CONTENT_BROWSER_GPU_GPU_INTERNALS_UI_H_

#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"

namespace content {

class GpuInternalsUI;

class GpuInternalsUIConfig : public DefaultWebUIConfig<GpuInternalsUI> {
 public:
  GpuInternalsUIConfig()
      : DefaultWebUIConfig(kChromeUIScheme, kChromeUIGpuHost) {}
};

class GpuInternalsUI : public WebUIController {
 public:
  explicit GpuInternalsUI(WebUI* web_ui);

  GpuInternalsUI(const GpuInternalsUI&) = delete;
  GpuInternalsUI& operator=(const GpuInternalsUI&) = delete;
};

}  // namespace content

#endif  // CONTENT_BROWSER_GPU_GPU_INTERNALS_UI_H_
