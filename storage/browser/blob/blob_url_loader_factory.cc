// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_url_loader_factory.h"

#include "base/functional/bind.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "storage/browser/blob/blob_url_registry.h"

namespace storage {

namespace {

// The mojo::Remote<BlobURLToken> parameter is passed in to make sure the
// connection stays alive until this method is called, it is not otherwise used
// by this method.
void CreateFactoryForToken(
    mojo::Remote<blink::mojom::BlobURLToken>,
    base::WeakPtr<BlobUrlRegistry> url_registry,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
    const base::UnguessableToken& token) {
  mojo::PendingRemote<blink::mojom::Blob> blob;
  GURL blob_url;
  if (url_registry)
    url_registry->GetTokenMapping(token, &blob_url, &blob);
  BlobURLLoaderFactory::Create(std::move(blob), blob_url, std::move(receiver));
}

}  // namespace

// static
void BlobURLLoaderFactory::Create(
    mojo::PendingRemote<blink::mojom::Blob> pending_blob,
    const GURL& blob_url,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver) {
  new BlobURLLoaderFactory(std::move(pending_blob), blob_url,
                           std::move(receiver));
}

// static
void BlobURLLoaderFactory::Create(
    mojo::PendingRemote<blink::mojom::BlobURLToken> token,
    base::WeakPtr<BlobUrlRegistry> url_registry,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver) {
  // Not every URLLoaderFactory user deals with the URLLoaderFactory simply
  // disconnecting very well, so make sure we always at least bind the receiver
  // to some factory that can then fail with a network error. Hence the callback
  // is wrapped in WrapCallbackWithDefaultInvokeIfNotRun.
  mojo::Remote<blink::mojom::BlobURLToken> token_remote(std::move(token));
  auto* raw_token = token_remote.get();
  raw_token->GetToken(mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      base::BindOnce(&CreateFactoryForToken, std::move(token_remote),
                     std::move(url_registry), std::move(receiver)),
      base::UnguessableToken()));
}

void BlobURLLoaderFactory::CreateLoaderAndStart(
    mojo::PendingReceiver<network::mojom::URLLoader> loader,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  if (url_.is_valid() && request.url != url_) {
    receivers_.ReportBadMessage("Invalid URL when attempting to fetch Blob");
    mojo::Remote<network::mojom::URLLoaderClient>(std::move(client))
        ->OnComplete(network::URLLoaderCompletionStatus(net::ERR_INVALID_URL));
    return;
  }
  if (!blob_) {
    mojo::Remote<network::mojom::URLLoaderClient>(std::move(client))
        ->OnComplete(
            network::URLLoaderCompletionStatus(net::ERR_FILE_NOT_FOUND));
    return;
  }
  blob_->Load(std::move(loader), request.method, request.headers,
              std::move(client));
}

void BlobURLLoaderFactory::Clone(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver) {
  receivers_.Add(this, std::move(receiver));
}

BlobURLLoaderFactory::BlobURLLoaderFactory(
    mojo::PendingRemote<blink::mojom::Blob> blob,
    const GURL& blob_url,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver)
    : blob_(std::move(blob)), url_(blob_url) {
  receivers_.Add(this, std::move(receiver));
  receivers_.set_disconnect_handler(base::BindRepeating(
      &BlobURLLoaderFactory::OnConnectionError, base::Unretained(this)));
}

BlobURLLoaderFactory::~BlobURLLoaderFactory() = default;

void BlobURLLoaderFactory::OnConnectionError() {
  if (!receivers_.empty())
    return;
  delete this;
}

}  // namespace storage
