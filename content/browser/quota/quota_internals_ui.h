// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_QUOTA_QUOTA_INTERNALS_UI_H_
#define CONTENT_BROWSER_QUOTA_QUOTA_INTERNALS_UI_H_

#include "content/public/browser/web_ui_controller.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "storage/browser/quota/quota_internals.mojom-forward.h"

namespace content {

class RenderFrameHost;
class WebUI;

// WebUIController for the chrome://quota-internals page.
class CONTENT_EXPORT QuotaInternalsUI : public WebUIController {
 public:
  explicit QuotaInternalsUI(WebUI* web_ui);
  QuotaInternalsUI(const QuotaInternalsUI& other) = delete;
  QuotaInternalsUI& operator=(const QuotaInternalsUI& other) = delete;
  ~QuotaInternalsUI() override;

  // WebUIController overrides:
  void WebUIRenderFrameCreated(RenderFrameHost* render_frame_host) override;

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace content

#endif  // CONTENT_BROWSER_QUOTA_QUOTA_INTERNALS_UI_H_