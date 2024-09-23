// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/browser/quota/quota_internals_ui.h"

#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/webui/web_ui_impl.h"
#include "content/grit/quota_internals_resources.h"
#include "content/grit/quota_internals_resources_map.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/url_constants.h"
#include "storage/browser/quota/quota_internals.mojom.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/browser/quota/quota_manager_proxy.h"

namespace content {

QuotaInternalsUI::QuotaInternalsUI(WebUI* web_ui) : WebUIController(web_ui) {
  WebUIDataSource* source = WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      kChromeUIQuotaInternalsHost);

  source->AddResourcePaths(
      base::make_span(kQuotaInternalsResources, kQuotaInternalsResourcesSize));

  source->SetDefaultResource(IDR_QUOTA_INTERNALS_QUOTA_INTERNALS_HTML);

  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources 'self';");

  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::TrustedTypes,
      "trusted-types static-types;");
}

WEB_UI_CONTROLLER_TYPE_IMPL(QuotaInternalsUI)

QuotaInternalsUI::~QuotaInternalsUI() = default;

void QuotaInternalsUI::WebUIRenderFrameCreated(
    RenderFrameHost* render_frame_host) {
  // Enable the JavaScript Mojo bindings in the renderer process, so the JS
  // code can call the Mojo APIs exposed by this WebUI.
  render_frame_host->EnableMojoJsBindings(nullptr);
}

void QuotaInternalsUI::BindInterface(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<storage::mojom::QuotaInternalsHandler> receiver) {
  static_cast<StoragePartitionImpl*>(render_frame_host->GetStoragePartition())
      ->GetQuotaManager()
      ->proxy()
      ->BindInternalsHandler(std::move(receiver));
}

}  // namespace content
