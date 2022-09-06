// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pending_beacon_host.h"

#include "base/memory/scoped_refptr.h"
#include "content/browser/renderer_host/pending_beacon_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_result.h"
#include "content/public/browser/render_frame_host.h"
#include "services/network/public/cpp/cors/cors.h"
#include "services/network/public/cpp/data_element.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"

namespace content {
namespace {

// Returns true if `host` has the Background Sync permission granted for current
// document.
bool IsBackgroundSyncGranted(RenderFrameHost* host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(host);

  auto* permission_controller =
      host->GetBrowserContext()->GetPermissionController();
  DCHECK(permission_controller);

  // Cannot use `PermissionController::GetPermissionStatusForCurrentDocument()`
  // as `host` might not have all its states available when in PendingBeaconHost
  // dtor even if it's still alive (See `DocumentUserData::render_frame_host()`)
  // Specifically, it will crash on Android when the controller requests a
  // RenderViewHost.
  return permission_controller
             ->GetPermissionResultForOriginWithoutContext(
                 blink::PermissionType::BACKGROUND_SYNC,
                 host->GetLastCommittedOrigin())
             .status == blink::mojom::PermissionStatus::GRANTED;
}

}  // namespace

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
    blink::mojom::BeaconMethod method) {
  auto beacon =
      std::make_unique<Beacon>(url, method, this, std::move(receiver));
  beacons_.emplace_back(std::move(beacon));
}

PendingBeaconHost::~PendingBeaconHost() {
  // The blink::Document is about to destroy.
  // Checks if it has Background Sync granted before sending out the rest of
  // beacons.
  // https://github.com/WICG/unload-beacon#privacy
  if (IsBackgroundSyncGranted(&render_frame_host())) {
    Send(beacons_);
  }
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
  Send(to_send);
}

void PendingBeaconHost::Send(
    const std::vector<std::unique_ptr<Beacon>>& beacons) {
  if (beacons.empty()) {
    return;
  }
  service_->SendBeacons(beacons, shared_url_factory_.get());
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
               PendingBeaconHost* beacon_host,
               mojo::PendingReceiver<blink::mojom::PendingBeacon> receiver)
    : receiver_(this, std::move(receiver)),
      beacon_host_(beacon_host),
      url_(url),
      method_(method) {
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

  content_type_ = content_type;

  if (!request_body->elements_mutable()) {
    return;
  }
  if (request_body->elements()->empty()) {
    return;
  }
  if (request_body->elements()->size() != 1) {
    mojo::ReportBadMessage("Complex body is not supported yet");
    return;
  }
  auto& data_element = (*request_body->elements_mutable())[0];
  switch (data_element.type()) {
    case network::DataElement::Tag::kBytes:
    case network::DataElement::Tag::kDataPipe:
    case network::DataElement::Tag::kFile:
      // These are copyable and supported types.
      break;
    case network::DataElement::Tag::kChunkedDataPipe:
      // This is an uncopyable and unsupported type.
      mojo::ReportBadMessage("Streaming body is not supported.");
      return;
  }
  request_element_ = std::move(data_element);
}

void Beacon::SetRequestURL(const GURL& url) {
  // Only GET Beacon is allowed to update its URL after construction.
  if (method_ != blink::mojom::BeaconMethod::kGet) {
    mojo::ReportBadMessage("Unexpected BeaconMethod from renderer");
    return;
  }
  url_ = url;
}

void Beacon::SendNow() {
  beacon_host_->SendBeacon(this);
}

const std::unique_ptr<network::ResourceRequest>
Beacon::GenerateResourceRequest() const {
  DCHECK(method_ == blink::mojom::BeaconMethod::kGet ||
         method_ == blink::mojom::BeaconMethod::kPost);

  auto request = std::make_unique<network::ResourceRequest>();

  request->url = url_;
  request->mode = network::mojom::RequestMode::kCors;
  request->request_initiator =
      beacon_host_->render_frame_host().GetLastCommittedOrigin();
  request->credentials_mode = network::mojom::CredentialsMode::kSameOrigin;

  if (method_ == blink::mojom::BeaconMethod::kGet) {
    request->method = net::HttpRequestHeaders::kGetMethod;
  } else {
    request->method = net::HttpRequestHeaders::kPostMethod;
    request->keepalive = true;
    if (!content_type_.empty()) {
      request->headers.SetHeader(net::HttpRequestHeaders::kContentType,
                                 content_type_);
    }
    if (request_element_.has_value()) {
      request->request_body =
          base::MakeRefCounted<network::ResourceRequestBody>();
      DCHECK_NE(request_element_->type(),
                network::DataElement::Tag::kChunkedDataPipe);
      request->request_body->elements_mutable()->push_back(
          request_element_->Clone());
    }
  }

  return request;
};

}  // namespace content
