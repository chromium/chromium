// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/browser/private_aggregation/private_aggregation_internals_ui.h"

#include <memory>
#include <utility>

#include "base/containers/span.h"
#include "content/browser/private_aggregation/private_aggregation_internals_handler_impl.h"
#include "content/grit/private_aggregation_internals_resources.h"
#include "content/grit/private_aggregation_internals_resources_map.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/bindings_policy.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"

namespace content {

PrivateAggregationInternalsUI::PrivateAggregationInternalsUI(WebUI* web_ui)
    : WebUIController(web_ui) {
  // Initialize the UI with no bindings. Mojo bindings will be separately
  // granted to frames within this WebContents.
  web_ui->SetBindings(BindingsPolicySet());
  WebUIDataSource* source = WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      kChromeUIPrivateAggregationInternalsHost);

  source->AddResourcePaths(
      base::make_span(kPrivateAggregationInternalsResources,
                      kPrivateAggregationInternalsResourcesSize));

  source->SetDefaultResource(
    IDR_PRIVATE_AGGREGATION_INTERNALS_PRIVATE_AGGREGATION_INTERNALS_HTML);

  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::TrustedTypes,
      "trusted-types static-types;");
}

WEB_UI_CONTROLLER_TYPE_IMPL(PrivateAggregationInternalsUI)

PrivateAggregationInternalsUI::~PrivateAggregationInternalsUI() = default;

void PrivateAggregationInternalsUI::WebUIRenderFrameCreated(
    RenderFrameHost* rfh) {
  // Enable the JavaScript Mojo bindings in the renderer process, so the JS
  // code can call the Mojo APIs exposed by this WebUI.
  rfh->EnableMojoJsBindings(nullptr);
}

void PrivateAggregationInternalsUI::BindInterface(
    mojo::PendingReceiver<private_aggregation_internals::mojom::Factory>
        factory) {
  factory_.reset();
  factory_.Bind(std::move(factory));
}

void PrivateAggregationInternalsUI::Create(
    mojo::PendingRemote<private_aggregation_internals::mojom::Observer>
        observer,
    mojo::PendingReceiver<private_aggregation_internals::mojom::Handler>
        handler) {
  ui_handler_ = std::make_unique<PrivateAggregationInternalsHandlerImpl>(
      web_ui(), std::move(observer), std::move(handler));
}

}  // namespace content
