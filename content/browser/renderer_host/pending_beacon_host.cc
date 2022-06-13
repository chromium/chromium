// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pending_beacon_host.h"

#include "content/browser/renderer_host/pending_beacon_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace content {

PendingBeaconHost::PendingBeaconHost(
    RenderFrameHost* rfh,
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_factory,
    PendingBeaconService* service)
    : DocumentUserData<PendingBeaconHost>(rfh),
      receiver_(this),
      shared_url_factory_(std::move(shared_url_factory)),
      service_(service) {}

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
      timeout_(timeout) {}

Beacon::~Beacon() = default;

void Beacon::SetData(const std::string& data) {
  beacon_data_ = data;
}

void Beacon::SendNow() {
  beacon_host_->SendBeacon(this);
}

}  // namespace content
