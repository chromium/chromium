// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_MEDIA_INTERNALS_UI_H_
#define CONTENT_BROWSER_MEDIA_MEDIA_INTERNALS_UI_H_

#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"

namespace content {

class MediaInternalsUI;

class MediaInternalsUIConfig : public DefaultWebUIConfig<MediaInternalsUI> {
 public:
  MediaInternalsUIConfig()
      : DefaultWebUIConfig(kChromeUIScheme, kChromeUIMediaInternalsHost) {}
};

// The implementation for the chrome://media-internals page.
class MediaInternalsUI : public WebUIController {
 public:
  explicit MediaInternalsUI(WebUI* web_ui);

  MediaInternalsUI(const MediaInternalsUI&) = delete;
  MediaInternalsUI& operator=(const MediaInternalsUI&) = delete;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_MEDIA_INTERNALS_UI_H_
