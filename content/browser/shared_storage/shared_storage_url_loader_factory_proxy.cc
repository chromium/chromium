// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_storage/shared_storage_url_loader_factory_proxy.h"

#include <stdint.h>

#include <utility>

#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/strings/escape.h"
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
    const url::Origin& data_origin,
    const GURL& script_url,
    network::mojom::CredentialsMode credentials_mode,
    const net::SiteForCookies& site_for_cookies)
    : frame_url_loader_factory_(std::move(frame_url_loader_factory)),
      receiver_(this, std::move(pending_receiver)),
      frame_origin_(frame_origin),
      data_origin_(data_origin),
      script_url_(script_url),
      credentials_mode_(credentials_mode),
      site_for_cookies_(site_for_cookies) {}

SharedStorageURLLoaderFactoryProxy::~SharedStorageURLLoaderFactoryProxy() =
    default;

void SharedStorageURLLoaderFactoryProxy::CreateLoaderAndStart(
    mojo::PendingReceiver<network::mojom::URLLoader> receiver,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& url_request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  if (url_request.url != script_url_) {
    receiver_.ReportBadMessage("Unexpected request");
    return;
  }

  network::ResourceRequest new_request;
  new_request.url = script_url_;
  new_request.headers.SetHeader(net::HttpRequestHeaders::kAccept,
                                "application/javascript");
  if (!frame_origin_.IsSameOriginWith(data_origin_)) {
    // The data origin can't be opaque.
    DCHECK(!data_origin_.opaque());
    new_request.headers.SetHeader(kSecSharedStorageDataOriginHeader,
                                  data_origin_.Serialize());
  }
  new_request.redirect_mode = network::mojom::RedirectMode::kError;
  new_request.credentials_mode = credentials_mode_;
  new_request.site_for_cookies = site_for_cookies_;
  new_request.request_initiator = frame_origin_;
  new_request.mode = network::mojom::RequestMode::kCors;
  new_request.destination =
      network::mojom::RequestDestination::kSharedStorageWorklet;

  // TODO(crbug.com/40803630): create a new factory when the current one gets
  // disconnected.
  frame_url_loader_factory_->CreateLoaderAndStart(
      std::move(receiver), GlobalRequestID::MakeBrowserInitiated().request_id,
      network::mojom::kURLLoadOptionNone, new_request, std::move(client),
      traffic_annotation);

  receiver_.reset();
}

void SharedStorageURLLoaderFactoryProxy::Clone(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver) {
  NOTREACHED_IN_MIGRATION();
}

}  // namespace content
