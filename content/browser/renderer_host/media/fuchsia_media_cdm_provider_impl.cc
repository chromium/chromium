// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/fuchsia_media_cdm_provider_impl.h"

#include <fuchsia/mediacodec/cpp/fidl.h>
#include <lib/sys/cpp/component_context.h>

#include "content/public/browser/provision_fetcher_factory.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "media/base/provision_fetcher.h"
#include "media/mojo/services/fuchsia_cdm_manager.h"

namespace content {

// static
void FuchsiaMediaCdmProviderImpl::Bind(
    RenderFrameHost* frame_host,
    mojo::PendingReceiver<media::mojom::FuchsiaMediaCdmProvider> receiver) {
  CHECK(frame_host);
  // The object will delete itself when connection to the frame is broken.
  new FuchsiaMediaCdmProviderImpl(*frame_host, std::move(receiver));
}

FuchsiaMediaCdmProviderImpl::FuchsiaMediaCdmProviderImpl(
    RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<media::mojom::FuchsiaMediaCdmProvider> receiver)
    : DocumentService(render_frame_host, std::move(receiver)) {}
FuchsiaMediaCdmProviderImpl::~FuchsiaMediaCdmProviderImpl() = default;

void FuchsiaMediaCdmProviderImpl::CreateCdm(
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
          .GetStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess();

  media::CreateFetcherCB create_fetcher_cb = base::BindRepeating(
      &CreateProvisionFetcher, std::move(url_loader_factory));
  cdm_manager->CreateAndProvision(
      key_system, origin(), std::move(create_fetcher_cb), std::move(request));
}

}  // namespace content
