// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBRTC_WEBRTC_INTERNALS_UI_H_
#define CONTENT_BROWSER_WEBRTC_WEBRTC_INTERNALS_UI_H_

#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"

namespace content {

class WebRTCInternalsUI;

// The WebUIConfig for the chrome://webrtc-internals page.
class WebRTCInternalsUIConfig : public DefaultWebUIConfig<WebRTCInternalsUI> {
 public:
  WebRTCInternalsUIConfig()
      : DefaultWebUIConfig(kChromeUIScheme, kChromeUIWebRTCInternalsHost) {}
};

// The implementation for the chrome://webrtc-internals page.
class WebRTCInternalsUI : public WebUIController {
 public:
  explicit WebRTCInternalsUI(WebUI* web_ui);

  WebRTCInternalsUI(const WebRTCInternalsUI&) = delete;
  WebRTCInternalsUI& operator=(const WebRTCInternalsUI&) = delete;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBRTC_WEBRTC_INTERNALS_UI_H_
