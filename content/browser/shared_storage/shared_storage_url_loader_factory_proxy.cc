// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_storage/shared_storage_url_loader_factory_proxy.h"

#include <stdint.h>

#include <utility>

#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/strings/escape.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "content/public/browser/global_request_id.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/isolation_info.h"
#include "net/cookies/site_for_cookies.h"
#include "net/http/http_request_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

SharedStorageURLLoaderFactoryProxy::SharedStorageURLLoaderFactoryProxy(
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        frame_url_loader_factory,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> pending_receiver,
    const url::Origin& frame_origin,
    const GURL& script_url)
    : frame_url_loader_factory_(std::move(frame_url_loader_factory)),
      receiver_(this, std::move(pending_receiver)),
      frame_origin_(frame_origin),
      script_url_(script_url) {}

SharedStorageURLLoaderFactoryProxy::~SharedStorageURLLoaderFactoryProxy() =
    default;

void SharedStorageURLLoaderFactoryProxy::CreateLoaderAndStart(
    mojo::PendingReceiver<network::mojom::URLLoader> receiver,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& url_request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK(frame_origin_.IsSameOriginWith(script_url_));

  if (url_request.url != script_url_) {
    receiver_.ReportBadMessage("Unexpected request");
    return;
  }

  network::ResourceRequest new_request;
  new_request.url = script_url_;
  new_request.headers.SetHeader(net::HttpRequestHeaders::kAccept,
                                "application/javascript");
  new_request.redirect_mode = network::mojom::RedirectMode::kError;
  new_request.credentials_mode = network::mojom::CredentialsMode::kSameOrigin;
  new_request.request_initiator = frame_origin_;
  new_request.mode = network::mojom::RequestMode::kSameOrigin;

  // TODO(crbug/1268616): create a new factory when the current one gets
  // disconnected.
  frame_url_loader_factory_->CreateLoaderAndStart(
      std::move(receiver), GlobalRequestID::MakeBrowserInitiated().request_id,
      network::mojom::kURLLoadOptionNone, new_request, std::move(client),
      traffic_annotation);

  receiver_.reset();
}

void SharedStorageURLLoaderFactoryProxy::Clone(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver) {
  NOTREACHED();
}

}  // namespace content
