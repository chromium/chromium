// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/media_resource_provider_fuchsia.h"

#include "base/bind.h"
#include "base/fuchsia/process_context.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/provision_fetcher_factory.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "media/base/provision_fetcher.h"
#include "media/fuchsia/cdm/service/fuchsia_cdm_manager.h"

namespace content {

// static
void MediaResourceProviderFuchsia::Bind(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<media::mojom::FuchsiaMediaResourceProvider>
        receiver) {
  // The object will delete itself when connection to the frame is broken.
  new MediaResourceProviderFuchsia(frame_host, std::move(receiver));
}

MediaResourceProviderFuchsia::MediaResourceProviderFuchsia(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<media::mojom::FuchsiaMediaResourceProvider> receiver)
    : DocumentService(render_frame_host, std::move(receiver)) {}
MediaResourceProviderFuchsia::~MediaResourceProviderFuchsia() = default;

void MediaResourceProviderFuchsia::CreateCdm(
    const std::string& key_system,
    fidl::InterfaceRequest<fuchsia::media::drm::ContentDecryptionModule>
        request) {
  auto* cdm_manager = media::FuchsiaCdmManager::GetInstance();
  if (!cdm_manager) {
    DLOG(WARNING) << "FuchsiaCdmManager hasn't been initialized. Dropping "
                     "CreateCdm() request.";
    return;
  }

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
      render_frame_host()
          ->GetStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess();

  media::CreateFetcherCB create_fetcher_cb = base::BindRepeating(
      &content::CreateProvisionFetcher, std::move(url_loader_factory));
  cdm_manager->CreateAndProvision(
      key_system, origin(), std::move(create_fetcher_cb), std::move(request));
}

}  // namespace content
