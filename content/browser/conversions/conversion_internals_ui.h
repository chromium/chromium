// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CONVERSIONS_CONVERSION_INTERNALS_UI_H_
#define CONTENT_BROWSER_CONVERSIONS_CONVERSION_INTERNALS_UI_H_

#include <memory>

#include "content/browser/conversions/conversion_internals.mojom.h"
#include "content/browser/conversions/conversion_manager.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_ui_controller.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {

class ConversionInternalsHandlerImpl;

// WebUI which handles serving the chrome://conversion-internals page.
class CONTENT_EXPORT ConversionInternalsUI : public WebUIController {
 public:
  explicit ConversionInternalsUI(WebUI* web_ui);
  ConversionInternalsUI(const ConversionInternalsUI& other) = delete;
  ConversionInternalsUI& operator=(const ConversionInternalsUI& other) = delete;
  ConversionInternalsUI(ConversionInternalsUI&& other) = delete;
  ConversionInternalsUI& operator=(ConversionInternalsUI&& other) = delete;
  ~ConversionInternalsUI() override;

  // WebUIController overrides:
  void WebUIRenderFrameCreated(RenderFrameHost* render_frame_host) override;

  void BindInterface(
      mojo::PendingReceiver<::mojom::ConversionInternalsHandler> receiver);

  void SetConversionManagerProviderForTesting(
      std::unique_ptr<ConversionManager::Provider> manager_provider);

 private:
  std::unique_ptr<ConversionInternalsHandlerImpl> ui_handler_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace content

#endif  // CONTENT_BROWSER_CONVERSIONS_CONVERSION_INTERNALS_UI_H_
