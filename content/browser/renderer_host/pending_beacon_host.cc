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
  base::UnguessableToken id = base::UnguessableToken::Create();
  beacon_ids_.emplace_back(id);
  service_->CreateBeacon(std::move(receiver), id, url, method, timeout);
}

PendingBeaconHost::~PendingBeaconHost() = default;

void PendingBeaconHost::SetReceiver(
    mojo::PendingReceiver<blink::mojom::PendingBeaconHost> receiver) {
  receiver_.Bind(std::move(receiver));
}

DOCUMENT_USER_DATA_KEY_IMPL(PendingBeaconHost);

Beacon::Beacon(const base::UnguessableToken& id,
               const GURL& url,
               blink::mojom::BeaconMethod method,
               base::TimeDelta timeout,
               mojo::PendingReceiver<blink::mojom::PendingBeacon> receiver)
    : receiver_(this, std::move(receiver)),
      id_(id),
      url_(url),
      method_(method),
      timeout_(timeout) {}

Beacon::~Beacon() = default;

void Beacon::Deactivate() {
  // Beacons are not deleted on deactivation; they'll be cleaned up when the
  // document that owns the beacon is either hidden or discarded.
  // TODO(crbug.com/1293679): Clean up beacons when their owning document is
  // discarded or hidden.
  active_ = false;
}

}  // namespace content
