// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/provision_fetcher_impl.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "content/public/browser/provision_fetcher_factory.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace content {

// static
void ProvisionFetcherImpl::Create(
    RenderFrameHost* render_frame_host,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    mojo::PendingReceiver<media::mojom::ProvisionFetcher> receiver) {
  CHECK(render_frame_host);
  DCHECK(url_loader_factory);
  // The created ProvisionFetcherImpl is bound to (and owned by) `receiver` and
  // the document associated with `render_frame_host`.
  new ProvisionFetcherImpl(
      *render_frame_host, std::move(receiver),
      CreateProvisionFetcher(std::move(url_loader_factory)));
}

ProvisionFetcherImpl::ProvisionFetcherImpl(
    RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<media::mojom::ProvisionFetcher> receiver,
    std::unique_ptr<media::ProvisionFetcher> provision_fetcher)
    : DocumentService(render_frame_host, std::move(receiver)),
      provision_fetcher_(std::move(provision_fetcher)) {
  DVLOG(1) << __FUNCTION__;
}

ProvisionFetcherImpl::~ProvisionFetcherImpl() = default;

void ProvisionFetcherImpl::Retrieve(const GURL& default_url,
                                    const std::string& request_data,
                                    RetrieveCallback callback) {
  DVLOG(1) << __FUNCTION__ << ": " << default_url;
  provision_fetcher_->Retrieve(
      default_url, request_data,
      base::BindOnce(&ProvisionFetcherImpl::OnResponse,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ProvisionFetcherImpl::OnResponse(RetrieveCallback callback,
                                      bool success,
                                      const std::string& response) {
  DVLOG(1) << __FUNCTION__ << ": " << success;
  std::move(callback).Run(success, response);
}

}  // namespace content
