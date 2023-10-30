// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/smart_card/smart_card_service.h"

#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/document_service.h"
#include "content/public/browser/isolated_context_util.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/smart_card_delegate.h"
#include "services/device/public/mojom/smart_card.mojom.h"
#include "third_party/blink/public/common/features_generated.h"

namespace content {

namespace {

// Deletes the SmartCardService when the connected document is destroyed.
class DocumentHelper
    : public content::DocumentService<blink::mojom::SmartCardService> {
 public:
  DocumentHelper(std::unique_ptr<SmartCardService> service,
                 RenderFrameHost& render_frame_host,
                 mojo::PendingReceiver<blink::mojom::SmartCardService> receiver)
      : DocumentService(render_frame_host, std::move(receiver)),
        service_(std::move(service)) {
    DCHECK(service_);
  }
  ~DocumentHelper() override = default;

  // blink::mojom::SmartCardService:
  void CreateContext(CreateContextCallback callback) override {
    service_->CreateContext(std::move(callback));
  }

 private:
  const std::unique_ptr<SmartCardService> service_;
};

}  // namespace

SmartCardService::SmartCardService(
    mojo::PendingRemote<device::mojom::SmartCardContextFactory> context_factory)
    : context_factory_(std::move(context_factory)) {}

SmartCardService::~SmartCardService() {}

// static
void SmartCardService::Create(
    RenderFrameHostImpl* render_frame_host,
    mojo::PendingReceiver<blink::mojom::SmartCardService> receiver) {
  BrowserContext* browser_context = render_frame_host->GetBrowserContext();
  DCHECK(browser_context);

  if (!base::FeatureList::IsEnabled(blink::features::kSmartCard)) {
    mojo::ReportBadMessage("The SmartCard feature is disabled.");
    return;
  }

  if (!render_frame_host->IsFeatureEnabled(
          blink::mojom::PermissionsPolicyFeature::kSmartCard)) {
    mojo::ReportBadMessage(
        "Access to the feature \"smart-card\" is disallowed by permissions "
        "policy.");
    return;
  }

  if (!HasIsolatedContextCapability(render_frame_host)) {
    mojo::ReportBadMessage(
        "Frame is not sufficiently isolated to use the Smart Card API.");
    return;
  }

  SmartCardDelegate* delegate =
      GetContentClient()->browser()->GetSmartCardDelegate(browser_context);
  if (!delegate) {
    mojo::ReportBadMessage("Browser has no Smart Card delegate.");
    return;
  }

  // DocumentHelper observes the lifetime of the document connected to
  // `render_frame_host` and destroys the SmartCardService when the Mojo
  // connection is disconnected, RenderFrameHost is deleted, or the
  // RenderFrameHost commits a cross-document navigation. It forwards its Mojo
  // interface to SmartCardService.
  new DocumentHelper(
      std::make_unique<SmartCardService>(
          delegate->GetSmartCardContextFactory(*browser_context)),
      *render_frame_host, std::move(receiver));
}

void SmartCardService::CreateContext(CreateContextCallback callback) {
  context_factory_->CreateContext(std::move(callback));
}

}  // namespace content
