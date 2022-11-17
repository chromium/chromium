// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pending_beacon_host.h"

#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_macros.h"
#include "content/browser/renderer_host/pending_beacon_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_result.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "services/network/public/cpp/cors/cors.h"
#include "services/network/public/cpp/data_element.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "url/url_constants.h"

namespace content {
namespace {

// Tells if `url` can be used by PendingBeacon.
// The renderer checks these criteria in pending_beacon.cc and should have
// already returned an exception instead of making an IPC to the browser.
// Double-check in the browser process and report a bad message since the
// renderer is possibly compromised.
bool ShouldBlockURL(const GURL& url) {
  return !url.is_valid() || !url.has_scheme() ||
         !url.SchemeIs(url::kHttpsScheme);
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
  // TODO(crbug.com/1293679): Abort if created from a non-secure context.
  // https://w3c.github.io/webappsec-secure-contexts/

  render_frame_host().GetProcess()->AddObserver(this);
}

void PendingBeaconHost::CreateBeacon(
    mojo::PendingReceiver<blink::mojom::PendingBeacon> receiver,
    const GURL& url,
    blink::mojom::BeaconMethod method) {
  if (ShouldBlockURL(url)) {
    mojo::ReportBadMessage("Unexpected url format from renderer");
    return;
  }

  UMA_HISTOGRAM_ENUMERATION("PendingBeaconHost.Action", Action::kCreate);
  auto beacon =
      std::make_unique<Beacon>(url, method, this, std::move(receiver));
  beacons_.emplace_back(std::move(beacon));
}

PendingBeaconHost::~PendingBeaconHost() {
  // The blink::Document is about to destroy.
  if (IsInObserverList()) {
    render_frame_host().GetProcess()->RemoveObserver(this);
  }
  CHECK(!IsInObserverList());

  // PendingBeaconHost gets cleared when either RenderFrameHost is deleted or
  // a cross-document non-BFCached navigation is committed in the same
  // RenderFrameHost (See content::DocumentUserData).
  // In both of the above case, pending beacons should be sent per Case B-1 from
  // https://github.com/WICG/pending-beacon/issues/3#issuecomment-1286397825
  SendAll(BatchAction::kSendAllOnHostDestroy);
}

void PendingBeaconHost::DeleteBeacon(Beacon* beacon) {
  auto iter = base::ranges::find(
      beacons_, beacon,
      [](const std::unique_ptr<Beacon>& b) { return b.get(); });
  if (iter != beacons_.end()) {
    UMA_HISTOGRAM_ENUMERATION("PendingBeaconHost.Action", Action::kDelete);
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
  UMA_HISTOGRAM_ENUMERATION("PendingBeaconHost.Action", Action::kSend);
  Send(to_send);
}

void PendingBeaconHost::SendAll(const BatchAction& action) {
  if (beacons_.empty()) {
    return;
  }

  // Swaps out from private field first to make any potential subsequent send
  // requests from renderer no-ops.
  std::vector<std::unique_ptr<Beacon>> to_send;
  to_send.swap(beacons_);
  UMA_HISTOGRAM_ENUMERATION("PendingBeaconHost.BatchAction", action);
  Send(to_send);
}

void PendingBeaconHost::Send(
    const std::vector<std::unique_ptr<Beacon>>& beacons) {
  if (beacons.empty()) {
    return;
  }

  // TODO(crbug.com/1378833): When document is in BackForwardCache, checks if it
  // has Background Sync granted before sending out the rest of beacons.
  // https://github.com/WICG/pending-beacon#privacy
  // Right now it cannot happen as `kPendingBeaconAPIForcesSendingOnNavigation`
  // is enabled.

  service_->SendBeacons(beacons, shared_url_factory_.get());
}

void PendingBeaconHost::SetReceiver(
    mojo::PendingReceiver<blink::mojom::PendingBeaconHost> receiver) {
  receiver_.Bind(std::move(receiver));
}

void PendingBeaconHost::SendAllOnNavigation() {
  if (!blink::features::kPendingBeaconAPIForcesSendingOnNavigation.Get()) {
    return;
  }

  // Sends out all `beacons_` ASAP to avoid network change happens.
  // This is to mitigate potential privacy issue that when network changes
  // after users think they have left a page, beacons queued in that page
  // still exist and get sent through the new network, which leaks navigation
  // history to the new network.
  // See https://github.com/WICG/pending-beacon/issues/30.
  SendAll(BatchAction::kSendAllOnNavigation);

  // Now all beacons are gone.
  // The renderer-side beacons should update their pending states by themselves.
}

void PendingBeaconHost::RenderProcessExited(
    RenderProcessHost*,
    const ChildProcessTerminationInfo&) {
  SendAll(BatchAction::kSendAllOnProcessExit);
}
void PendingBeaconHost::RenderProcessHostDestroyed(RenderProcessHost*) {
  render_frame_host().GetProcess()->RemoveObserver(this);
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
  if (ShouldBlockURL(url)) {
    mojo::ReportBadMessage("Unexpected url format from renderer");
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
}

}  // namespace content
