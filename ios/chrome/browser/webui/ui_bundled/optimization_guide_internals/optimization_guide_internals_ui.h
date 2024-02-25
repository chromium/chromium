// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_OPTIMIZATION_GUIDE_INTERNALS_OPTIMIZATION_GUIDE_INTERNALS_UI_H_
#define IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_OPTIMIZATION_GUIDE_INTERNALS_OPTIMIZATION_GUIDE_INTERNALS_UI_H_

#import "components/optimization_guide/optimization_guide_internals/webui/optimization_guide_internals.mojom.h"
#import "ios/web/public/webui/web_ui_ios_controller.h"
#import "mojo/public/cpp/bindings/pending_receiver.h"
#import "mojo/public/cpp/bindings/receiver.h"

class OptimizationGuideLogger;
class OptimizationGuideInternalsPageHandlerImpl;

namespace web {
class WebUIIOS;
}

// The WebUI controller for chrome://optimization-guide-internals.
class OptimizationGuideInternalsUI
    : public web::WebUIIOSController,
      public optimization_guide_internals::mojom::PageHandlerFactory {
 public:
  explicit OptimizationGuideInternalsUI(web::WebUIIOS* web_ui,
                                        const std::string& host);

  OptimizationGuideInternalsUI(const OptimizationGuideInternalsUI&) = delete;
  OptimizationGuideInternalsUI& operator=(const OptimizationGuideInternalsUI&) =
      delete;

  ~OptimizationGuideInternalsUI() override;

  void BindInterface(
      mojo::PendingReceiver<
          optimization_guide_internals::mojom::PageHandlerFactory> receiver);

 private:
  // optimization_guide_internals::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<optimization_guide_internals::mojom::Page> page)
      override;
  void RequestDownloadedModelsInfo(
      RequestDownloadedModelsInfoCallback callback) override;
  void RequestLoggedModelQualityClientIds(
      RequestLoggedModelQualityClientIdsCallback callback) override;

  std::unique_ptr<OptimizationGuideInternalsPageHandlerImpl>
      optimization_guide_internals_page_handler_;
  mojo::Receiver<optimization_guide_internals::mojom::PageHandlerFactory>
      optimization_guide_internals_page_factory_receiver_{this};

  // Logger to receive the debug logs from the optimization guide service. Not
  // owned. Guaranteed to outlive `this`, since the logger is owned by the
  // optimization guide keyed service, while `this` is part of
  // RenderFrameHostImpl::WebUIImpl.
  raw_ptr<OptimizationGuideLogger> optimization_guide_logger_;
};

#endif  // IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_OPTIMIZATION_GUIDE_INTERNALS_OPTIMIZATION_GUIDE_INTERNALS_UI_H_
