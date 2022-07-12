// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pending_beacon_host.h"

#include "base/memory/scoped_refptr.h"
#include "content/browser/renderer_host/pending_beacon_service.h"
#include "content/public/browser/render_frame_host.h"
#include "services/network/public/cpp/cors/cors.h"
#include "services/network/public/cpp/data_element.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace content {

PendingBeaconHost::PendingBeaconHost(
    RenderFrameHost* rfh,
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_factory,
    PendingBeaconService* service)
    : DocumentUserData<PendingBeaconHost>(rfh),
      receiver_(this),
      shared_url_factory_(std::move(shared_url_factory)),
      service_(service) {
  DCHECK(shared_url_factory_);
  DCHECK(service_);
}

void PendingBeaconHost::CreateBeacon(
    mojo::PendingReceiver<blink::mojom::PendingBeacon> receiver,
    const GURL& url,
    blink::mojom::BeaconMethod method,
    base::TimeDelta timeout) {
  auto beacon =
      std::make_unique<Beacon>(url, method, timeout, this, std::move(receiver));
  beacons_.emplace_back(std::move(beacon));
}

PendingBeaconHost::~PendingBeaconHost() {
  service_->SendBeacons(beacons_, shared_url_factory_.get());
}

void PendingBeaconHost::DeleteBeacon(Beacon* beacon) {
  auto iter = base::ranges::find(
      beacons_, beacon,
      [](const std::unique_ptr<Beacon>& b) { return b.get(); });
  if (iter != beacons_.end()) {
    beacons_.erase(iter);
  }
}

void PendingBeaconHost::SendBeacon(Beacon* beacon) {
  auto iter = base::ranges::find(
      beacons_, beacon,
      [](const std::unique_ptr<Beacon>& b) { return b.get(); });
  if (iter == beacons_.end()) {
    return;
  }
  std::unique_ptr<Beacon> beacon_ptr = std::move(*iter);
  beacons_.erase(iter);
  std::vector<std::unique_ptr<Beacon>> to_send;
  to_send.emplace_back(std::move(beacon_ptr));
  service_->SendBeacons(to_send, shared_url_factory_.get());
}

void PendingBeaconHost::SetReceiver(
    mojo::PendingReceiver<blink::mojom::PendingBeaconHost> receiver) {
  receiver_.Bind(std::move(receiver));
}

DOCUMENT_USER_DATA_KEY_IMPL(PendingBeaconHost);

void Beacon::Deactivate() {
  beacon_host_->DeleteBeacon(this);
}

Beacon::Beacon(const GURL& url,
               blink::mojom::BeaconMethod method,
               base::TimeDelta timeout,
               PendingBeaconHost* beacon_host,
               mojo::PendingReceiver<blink::mojom::PendingBeacon> receiver)
    : receiver_(this, std::move(receiver)),
      beacon_host_(beacon_host),
      url_(url),
      method_(method),
      timeout_(timeout) {
  DCHECK(beacon_host_);
}

Beacon::~Beacon() = default;

void Beacon::SetRequestData(
    scoped_refptr<network::ResourceRequestBody> request_body,
    const std::string& content_type) {
  if (method_ != blink::mojom::BeaconMethod::kPost) {
    mojo::ReportBadMessage("Unexpected BeaconMethod from renderer");
    return;
  }
  if (!content_type.empty() &&
      !network::cors::IsCorsSafelistedContentType(content_type)) {
    mojo::ReportBadMessage("Unexpected Content-Type from renderer");
    return;
  }

  content_type_ = content_type;

  // Move all DataElement into `request_elements_`.
  if (!request_body->elements_mutable()) {
    return;
  }
  request_elements_ = std::move(*request_body->elements_mutable());
}

void Beacon::SendNow() {
  beacon_host_->SendBeacon(this);
}

const std::unique_ptr<network::ResourceRequest>
Beacon::GenerateResourceRequest() const {
  auto request = std::make_unique<network::ResourceRequest>();
  if (method_ == blink::mojom::BeaconMethod::kGet) {
    request->method = net::HttpRequestHeaders::kGetMethod;
    request->url = url_;
  } else {
    request->method = net::HttpRequestHeaders::kPostMethod;
    request->url = url_;
    request->keepalive = true;
    if (!content_type_.empty()) {
      request->headers.SetHeader(net::HttpRequestHeaders::kContentType,
                                 content_type_);
    }
  }
  return request;
};

}  // namespace content
