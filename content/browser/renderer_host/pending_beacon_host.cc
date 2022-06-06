// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pending_beacon_host.h"

#include "content/browser/renderer_host/pending_beacon_service.h"

namespace content {

PendingBeaconHost::PendingBeaconHost(RenderFrameHost* rfh,
                                     PendingBeaconService* service)
    : DocumentUserData<PendingBeaconHost>(rfh),
      receiver_(this),
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
  service_->sendBeacons(beacons_);
}

void PendingBeaconHost::DeleteBeacon(Beacon* beacon) {
  auto iter = std::find_if(
      beacons_.begin(), beacons_.end(),
      [&](std::unique_ptr<Beacon>& b) { return b.get() == beacon; });
  if (iter != beacons_.end()) {
    beacons_.erase(iter);
  }
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

}  // namespace content
