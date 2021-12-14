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

// WebUIController for the chrome://quota-internals-2 page.
class CONTENT_EXPORT QuotaInternals2UI : public WebUIController {
 public:
  explicit QuotaInternals2UI(WebUI* web_ui);
  QuotaInternals2UI(const QuotaInternals2UI& other) = delete;
  QuotaInternals2UI& operator=(const QuotaInternals2UI& other) = delete;
  ~QuotaInternals2UI() override;

  // WebUIController overrides:
  void WebUIRenderFrameCreated(RenderFrameHost* render_frame_host) override;

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace content

#endif  // CONTENT_BROWSER_QUOTA_QUOTA_INTERNALS_UI_H_